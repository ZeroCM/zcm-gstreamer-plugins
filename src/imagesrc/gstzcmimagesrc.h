#ifndef __GST_ZCMSRC_H__
#define __GST_ZCMSRC_H__

#include <gst/gst.h>
#include <glib.h>
#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>
#include "zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.h"
G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_ZCMIMAGESRC \
  (gst_zcmimagesrc_get_type())
#define GST_ZCMIMAGESRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZCMIMAGESRC,GstZcmImageSrc))
#define GST_ZCMIMAGESRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZCMIMAGESRC,GstZcmImageSrcClass))
#define GST_IS_ZCMIMAGESRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZCMIMAGESRC))
#define GST_IS_ZCMIMAGESRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZCMIMAGESRC))
#define G_QUEUE_INIT { NULL, NULL, 0 }

typedef struct _GstZcmImageSrc      GstZcmImageSrc;
typedef struct _GstZcmImageSrcClass GstZcmImageSrcClass;

typedef struct _ZcmImageInfo
{
    unsigned int       width;
    unsigned int       height;
    unsigned char    * buf;
    unsigned int       size;
    unsigned int       framerate_num;
    unsigned int       framerate_den;
    unsigned int       frame_type;
} ZcmImageInfo;


struct _GstZcmImageSrc
{
    GstBaseSrc       element;
    gchar           *channel;
    gchar           *zcm_url;
    GstPad          *sinkpad, *srcpad;
    gboolean         verbose;
    ZcmImageInfo     frame_info;
    GstFlowReturn    status;
    GCond           *cond;
    GMutex          *mutx;
    gboolean         update_caps;
    GQueue*          buf_queue;
    zcm_t *zcm;
};

struct _GstZcmImageSrcClass
{
  GstBaseSrcClass parent_class;
};

GType gst_zcmimagesrc_get_type (void);

G_END_DECLS

#endif /* __GST_ZCMSRC_H__ */
