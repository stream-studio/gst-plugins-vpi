#include "gstvpibasetransform.h"

#include "nvbufsurface.h"
#include "gstnvdsbufferpool.h"

#include <cuda.h>
#include <cuda_runtime.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0,
};

#define DEFAULT_GPU_ID 0
#define DEFAULT_BATCH_SIZE 1

struct _GstVpiBaseTransformPrivate
{
  // Unique ID of the element. The labels generated by the element will be
  // updated at index `unique_id` of attr_info array in NvDsObjectParams.
  guint unique_id;

  // Frame number of the current input buffer
  guint64 frame_num;

  // Input video info (resolution, color format, framerate, etc)
  GstVideoInfo video_info;

  // GPU ID on which we expect to execute the task
  guint gpu_id;

  // Buffer pool size used for optical flow output
  guint pool_size;

  gint batch_size;
  guint num_batch_buffers;

  gint input_feature;
  gint output_feature;
  gint cuda_mem_type;

  GstVideoFormat input_fmt;
  GstVideoFormat output_fmt;

  GstVideoInfo in_info;
  GstVideoInfo out_info;

  GstBufferPool *pool;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GstVpiBaseTransform, gst_vpi_base_transform, GST_TYPE_BASE_TRANSFORM);

GST_DEBUG_CATEGORY_STATIC (gst_vpi_base_transform_debug);
#define GST_CAT_DEFAULT gst_vpi_base_transform_debug

#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"


static void gst_vpi_base_transform_init(GstVpiBaseTransform *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  self->priv = gst_vpi_base_transform_get_instance_private (self);

  self->priv->gpu_id = DEFAULT_GPU_ID;
  self->priv->batch_size = DEFAULT_BATCH_SIZE;

  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (self), FALSE);

  #if defined(__aarch64__)
    self->priv->cuda_mem_type = NVBUF_MEM_DEFAULT;
  #else
    self->priv->cuda_mem_type = NVBUF_MEM_CUDA_UNIFIED;
  #endif
}

static void gst_vpi_base_transform_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_vpi_base_transform_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}


static GstFlowReturn gst_vpi_base_transform_transform (GstBaseTransform * base, GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(base);
  NvBufSurface *surface, *surface_out = NULL;
  GstVpiBaseTransformClass *bclass = GST_VPI_BASE_TRANSFORM_GET_CLASS (self);


  if (cudaSetDevice(self->priv->gpu_id) != cudaSuccess)
  {
    GST_ERROR("Error: failed to set GPU to %d\n", self->priv->gpu_id);
    return GST_FLOW_ERROR;
  }



  GstMapInfo in_map_info;
  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ)) {
    GST_ERROR ("Error: Failed to map gst buffer\n");
    return GST_FLOW_ERROR;
  }

  surface = (NvBufSurface *) in_map_info.data;


  VPIImage input;

  VPIImageData inputData;
  inputData.buffer.fd = surface->surfaceList[0].bufferDesc;
  inputData.bufferType = VPI_IMAGE_BUFFER_NVBUFFER;
  
  VPIImageWrapperParams params;
  params.colorSpec = VPI_COLOR_SPEC_DEFAULT;

  VPIStatus status = vpiImageCreateWrapper(&inputData , &params, VPI_BACKEND_CUDA, &input);
  if (status != 0){
    char message[200];

    vpiGetLastStatusMessage(message, 200);
    GST_ERROR("Error mapping VPI Image : %s, %s", vpiStatusGetName (status), message);

    return GST_FLOW_ERROR;
  }


  VPIImageFormat type;
  vpiImageGetFormat(input, &type);  
  
  
  GstMapInfo out_map_info;
  if (!gst_buffer_map (outbuf, &out_map_info, GST_MAP_READ)) {
    GST_ERROR ("Error: Failed to map gst buffer\n");
    return GST_FLOW_ERROR;
  }

  surface_out = (NvBufSurface *) out_map_info.data;

  surface_out->numFilled = 1;

  VPIImage output;


  VPIImageData outputData;
  outputData.buffer.fd = surface_out->surfaceList[0].bufferDesc;
  outputData.bufferType = VPI_IMAGE_BUFFER_NVBUFFER;
  


  status = vpiImageCreateWrapper(&outputData , NULL, VPI_BACKEND_CUDA, &output);
  if (status != 0){
    char message[200];

    vpiGetLastStatusMessage(message, 200);
    GST_ERROR("Error mapping VPI Image output : %s, %s", vpiStatusGetName (status), message);

    return GST_FLOW_ERROR;
  }


  if (bclass->transform != NULL){
    bclass->transform(self, input, output);
  }
  


  vpiImageDestroy(input);
  vpiImageDestroy(output);



  gst_buffer_unmap(inbuf, &in_map_info);
  gst_buffer_unmap(outbuf, &out_map_info);

  if (!gst_buffer_copy_into (outbuf, inbuf,
          (GstBufferCopyFlags) GST_BUFFER_COPY_METADATA, 0, -1)) {
    GST_DEBUG ("Buffer metadata copy failed \n");
  }


  self->priv->frame_num++;

  return GST_FLOW_OK;
}


static GstCaps *
gst_vpi_base_transform_transform_caps (GstBaseTransform * btrans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCapsFeatures *feature = NULL;
  GstCaps *new_caps = NULL;

  if (direction == GST_PAD_SINK)
  {
    new_caps = gst_caps_new_simple ("video/x-raw",
          "width", GST_TYPE_INT_RANGE, 1, G_MAXINT, "height", GST_TYPE_INT_RANGE, 1,G_MAXINT, NULL);

  }
  else if (direction == GST_PAD_SRC)
  {
    new_caps = gst_caps_new_simple ("video/x-raw",
          "width", GST_TYPE_INT_RANGE, 1, G_MAXINT, "height", GST_TYPE_INT_RANGE, 1,G_MAXINT, NULL);
  }

  feature = gst_caps_features_new ("memory:NVMM", NULL);
  gst_caps_set_features (new_caps, 0, feature);

  if(gst_caps_is_fixed (caps))
  {
    GstStructure *fs = gst_caps_get_structure (caps, 0);
    const GValue *fps_value;
    guint i, n = gst_caps_get_size(new_caps);

    fps_value = gst_structure_get_value (fs, "framerate");

    // We cannot change framerate
    for (i = 0; i < n; i++)
    {
      fs = gst_caps_get_structure (new_caps, i);
      gst_structure_set_value (fs, "framerate", fps_value);
    }
  }
  return new_caps;
}

static GstFlowReturn gst_vpi_base_transform_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstBuffer *gstOutBuf = NULL;
  GstFlowReturn result = GST_FLOW_OK;
  GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(trans);

  result = gst_buffer_pool_acquire_buffer (self->priv->pool, &gstOutBuf, NULL);
  GST_DEBUG_OBJECT (self, "%s : Frame=%lu Gst-OutBuf=%p\n",
		  __func__, self->priv->frame_num, gstOutBuf);

  if (result != GST_FLOW_OK)
  {
    GST_ERROR_OBJECT (self, "gst_nv_spherical_warper_prepare_output_buffer failed");
    return result;
  }

  *outbuf = gstOutBuf;
  return result;
}


static gboolean gst_vpi_base_transform_size(GstBaseTransform* btrans,
        GstPadDirection dir, GstCaps *caps, gsize size, GstCaps* othercaps, gsize* othersize)
{
    gboolean ret = TRUE;
    GstVideoInfo info;

    ret = gst_video_info_from_caps(&info, othercaps);
    if (ret) *othersize = info.size;

    return ret;
}

static  gboolean gst_vpi_base_transform_start(GstBaseTransform * trans){
  GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(trans);
  return TRUE;
}

static gboolean gst_vpi_base_transform_release(GstVpiBaseTransform *self){

  GstVpiBaseTransformClass *bclass = GST_VPI_BASE_TRANSFORM_GET_CLASS (self);

  if (bclass->release != NULL){
    return bclass->release(self);
  }

  return TRUE;
}


static gboolean gst_vpi_base_transform_stop(GstBaseTransform * trans){
  GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(trans);

  gst_vpi_base_transform_release(self);
  if (self->priv->pool) {
    gst_buffer_pool_set_active (self->priv->pool, FALSE);
    gst_object_unref(self->priv->pool);
    self->priv->pool = NULL;
  }


  return TRUE;
}



static gboolean gst_vpi_base_transform_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVpiBaseTransform *self = GST_VPI_BASE_TRANSFORM(btrans);
  GstStructure *config = NULL;
  GstVpiBaseTransformClass *bclass = GST_VPI_BASE_TRANSFORM_GET_CLASS (btrans);


  GST_INFO ("in videoconvert caps = %s\n", gst_caps_to_string(incaps));

  /* Save the input video information, since this will be required later. */
  gst_video_info_from_caps(&self->priv->video_info, incaps);

  if (self->priv->batch_size == 0)
  {
    GST_ERROR ("Received invalid batch_size i.e. 0\n");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&self->priv->out_info, outcaps)) {
    GST_ERROR ("invalid output caps");
    return FALSE;
  }
  self->priv->output_fmt = GST_VIDEO_FORMAT_INFO_FORMAT (self->priv->out_info.finfo);

  if (!self->priv->pool)
  {
    self->priv->pool = gst_nvds_buffer_pool_new ();
    config = gst_buffer_pool_get_config (self->priv->pool);

    g_print ("out videoconvert caps = %s\n", gst_caps_to_string(outcaps));
    gst_buffer_pool_config_set_params (config, outcaps, sizeof (NvBufSurface), 4, 4); // TODO: remove 4 hardcoding

    gst_structure_set (config,
        "memtype", G_TYPE_UINT, self->priv->cuda_mem_type,
        "gpu-id", G_TYPE_UINT, self->priv->gpu_id,
        "batch-size", G_TYPE_UINT, self->priv->batch_size, NULL);

    GST_INFO_OBJECT (self, " %s Allocating Buffers in NVM Buffer Pool for Max_Views=%d\n",
        __func__, self->priv->batch_size);


    /* set config for the created buffer pool */
    if (!gst_buffer_pool_set_config (self->priv->pool, config)) {
      GST_WARNING ("bufferpool configuration failed");
      return FALSE;
    }

    gboolean is_active = gst_buffer_pool_set_active (self->priv->pool, TRUE);
    if (!is_active) {
      GST_WARNING (" Failed to allocate the buffers inside the output pool");
      return FALSE;
    } else {
      GST_DEBUG (" Output buffer pool (%p) successfully created",
                  self->priv->pool);
    }
  }

  if (bclass->prepare != NULL){
    return bclass->prepare(self, incaps, outcaps);
  }
  return TRUE;  
}



static void gst_vpi_base_transform_class_init(GstVpiBaseTransformClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_vpi_base_transform_set_property;
  object_class->get_property = gst_vpi_base_transform_get_property;


  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_vpi_base_transform_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_base_transform_stop);
  base_transform_class->transform = GST_DEBUG_FUNCPTR(gst_vpi_base_transform_transform);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_vpi_base_transform_set_caps);
  base_transform_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (gst_vpi_base_transform_prepare_output_buffer);
  base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(gst_vpi_base_transform_transform_caps);

  base_transform_class->transform_size = GST_DEBUG_FUNCPTR(gst_vpi_base_transform_size);

  klass->transform = NULL;  
  klass->prepare = NULL;
  klass->release = NULL;

  gst_element_class_set_static_metadata(element_class,
                                        "GstBaseVPITransform",
                                        "GstBaseVPITransform",
                                        "GstBaseVPITransform",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
