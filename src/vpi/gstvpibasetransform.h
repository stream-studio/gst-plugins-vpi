#ifndef __GST_VPI_BASE_TRANSFORM_H__
#define __GST_VPI_BASE_TRANSFORM_H__

#include <gst/gst.h>
#include "nvbufsurface.h"
#include "gstnvdsbufferpool.h"

#include <vpi/Image.h>
#include <vpi/Stream.h>

G_BEGIN_DECLS

#define GST_TYPE_VPI_BASE_TRANSFORM gst_vpi_base_transform_get_type ()
G_DECLARE_FINAL_TYPE (GstVpiBaseTransform, gst_vpi_base_transform, GST, VPI_BASE_TRANSFORM, GstBaseTransform)

struct GstVpiBaseTransformClass {
  GstBaseTransformClass parent_class;

  GstFlowReturn (*transform)    (GstVpiBaseTransform *trans, VPIImage *input,
                                 VPIImage *output);

  gboolean (*prepare)    (GstVpiBaseTransform *trans, GstCaps * incaps,
    GstCaps * outcaps);

  gboolean (*release)    (GstVpiBaseTransform *trans);    



};

G_END_DECLS

#endif
