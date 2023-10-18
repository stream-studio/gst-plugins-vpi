#ifndef __GST_VPI_BASE_TRANSFORM_H__
#define __GST_VPI_BASE_TRANSFORM_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <vpi/Image.h>

typedef struct _GstVpiBaseTransform GstVpiBaseTransform;
typedef struct _GstVpiBaseTransformPrivate GstVpiBaseTransformPrivate;


G_BEGIN_DECLS

typedef struct _GstVpiBaseTransformClass GstVpiBaseTransformClass;

#define GST_TYPE_VPI_BASE_TRANSFORM (gst_vpi_base_transform_get_type())
#define GST_IS_VPI_BASE_TRANSFORM(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VPI_BASE_TRANSFORM))
#define GST_IS_VPI_BASE_TRANSFORM_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_VPI_BASE_TRANSFORM))
#define GST_VPI_BASE_TRANSFORM_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_VPI_BASE_TRANSFORM, GstVpiBaseTransformClass))
#define GST_VPI_BASE_TRANSFORM(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VPI_BASE_TRANSFORM, GstVpiBaseTransform))
#define GST_VPI_BASE_TRANSFORM_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_VPI_BASE_TRANSFORM, GstVpiBaseTransformClass))
#define GST_VPI_BASE_TRANSFORM_CAST(obj)            ((GstVpiBaseTransform *)(obj))


struct _GstVpiBaseTransformClass {
  GstBaseTransformClass parent_class;

  GstFlowReturn (*transform)    (GstVpiBaseTransform *trans, VPIImage *input,
                                 VPIImage *output);

  gboolean (*prepare)    (GstVpiBaseTransform *trans, GstCaps * incaps,
    GstCaps * outcaps);

  gboolean (*release)    (GstVpiBaseTransform *trans);    

  gpointer _gst_reserved[GST_PADDING];

};


struct _GstVpiBaseTransform {
  GstBaseTransform         parent;

  /*< private >*/
  GstVpiBaseTransformPrivate *priv;

  gpointer _gst_reserved[GST_PADDING];
};

G_END_DECLS

#endif
