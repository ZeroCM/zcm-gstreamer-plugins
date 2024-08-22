/* GStreamer
 * Copyright (C) 2020 ZeroCM Team <www.zcm-project.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstzcmimagesrc
 * @title: zcmimagesrc
 *
 * ZcmImageSrc receives images over zcm channels and passes them back to the pipeline
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 zcmimagesrc channel=GSTREAMER_DATA url=ipc verbose=true do-timestamp=true ! videoconvert ! autovideosink
 * ]|
 * Receives frame over GSTREAMER_DATA channel
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gst/gst.h>
#include <glib.h>
#include <glib/gslice.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gsttypefindhelper.h>
#include <gst/base/gstbaseparse.h>
#include <unistd.h>
#include <sys/stat.h>
#include "gstzcmimagesrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_zcmimagesrc_debug);
#define GST_CAT_DEFAULT gst_zcmimagesrc_debug
#define DEFAULT_BLOCKSIZE       40*1024*1024

/* Filter signals and args */
enum
{
    PROP_0,
    PROP_CHANNEL,
    PROP_ZCM_URL,
    PROP_VERBOSE,
};

static GstStaticPadTemplate img_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (        // the capabilities of the padtemplate
        "image/jpeg, "
        "width = [ 16, 4096 ], "
        "height = [ 16, 4096 ], "
        "framerate = [ 0/1, 2147483647/1 ], "
    )
    );

static GstStaticPadTemplate vid_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (        // the capabilities of the padtemplate
        "video/x-raw, "
        "format = { (string)I420, (string)RGB, (string)BGR, (string)RGBA, (string)BGRA, (string)MJPEG }, "
        "width = [ 16, 4096 ], "
        "height = [ 16, 4096 ], "
        "framerate = [ 0/1, 2147483647/1 ]"
    )
    );

#define gst_zcmimagesrc_parent_class parent_class
G_DEFINE_TYPE (GstZcmImageSrc, gst_zcmimagesrc, GST_TYPE_BASE_SRC);

static void gst_zcmimagesrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_zcmimagesrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_zcmimagesrc_init (GstZcmImageSrc * filter);

static gboolean gst_zcmimagesrc_start (GstBaseSrc * basesrc);
static gboolean gst_zcmimagesrc_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_zcmimagesrc_fill (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer * buf);

static void gst_zcmimagesrc_finalize (GObject * object);
static gboolean gst_set_src_caps (GstBaseSrc * src, GstCaps *caps);
static gboolean gst_update_src_caps (GstBaseSrc * basesrc, GstBuffer *buffer);

static GstStateChangeReturn gst_zcmimagesrc_change_state (GstElement * element, GstStateChange transition);

/* GObject vmethod implementations */
/* initialize the zcmimagesrc's class */
static void
gst_zcmimagesrc_class_init (GstZcmImageSrcClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSrcClass *gstbasesrc_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

    gobject_class->set_property = gst_zcmimagesrc_set_property;
    gobject_class->get_property = gst_zcmimagesrc_get_property;

    g_object_class_install_property (gobject_class, PROP_CHANNEL,
          g_param_spec_string ("channel", "Zcm publish channel",
              "Channel name to publish data to",
              "GSTREAMER_DATA", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_ZCM_URL,
           g_param_spec_string ("url", "Zcm transport url",
              "The full zcm url specifying the zcm transport to be used",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_VERBOSE,
            g_param_spec_boolean ("verbose", "Verbose", "Produce verbose output",
                FALSE, G_PARAM_READWRITE));

    gst_element_class_set_details_simple(gstelement_class,
            "zcmimagesrc",
            "ZCM SOURCE",
            "zcm source subscribe to channel and pass images to next element in pipeline",
            "LIN_H");

    gst_element_class_add_pad_template (gstelement_class,
            gst_static_pad_template_get (&vid_factory));
    gst_element_class_add_pad_template (gstelement_class,
            gst_static_pad_template_get (&img_factory));

    gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_stop);
    gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_fill);
    gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_set_src_caps);
    gobject_class->finalize = gst_zcmimagesrc_finalize;

    gstelement_class->change_state =  GST_DEBUG_FUNCPTR (gst_zcmimagesrc_change_state);
}

static void gst_zcmimagesrc_finalize (GObject * object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void zcm_image_handler(const zcm_recv_buf_t *rbuf, const char *channel,
                       const zcm_gstreamer_plugins_image_t *img, void *user)
{
    GstZcmImageSrc *zcmimagesrc = (GstZcmImageSrc *)user;
    g_mutex_lock (zcmimagesrc->mutx);
    if (zcmimagesrc->verbose == TRUE)
    {
        g_print ("got image %p\n", img);
        g_print ("image time %ld\n", img->utime);
        g_print ("image res %d*%d\n", img->width, img->height);
        g_print ("image data %p\a", img->data);
        g_print ("image  pixel format  %d\n", img->pixelformat);
    }
    if (img->data != NULL)
    {
        if (!zcmimagesrc->image_info) {
            zcmimagesrc->image_info = (ZcmImageInfo*) calloc(1, sizeof(ZcmImageInfo));
        }

        zcmimagesrc->image_info->width  = img->width;
        zcmimagesrc->image_info->height = img->height;

        if (img->size != zcmimagesrc->image_info->size) {
            zcmimagesrc->image_info->buf = realloc(zcmimagesrc->image_info->buf, img->size);
            zcmimagesrc->image_info->size = img->size;
        }
        memcpy(zcmimagesrc->image_info->buf, img->data, img->size);

        zcmimagesrc->image_info->frame_type = img->pixelformat;

        g_cond_broadcast(zcmimagesrc->cond);
    }
    g_mutex_unlock (zcmimagesrc->mutx);
}

static gboolean zcm_source_init (GstZcmImageSrc *zcmimagesrc)
{
    zcmimagesrc->update_caps = TRUE;
    zcmimagesrc->cond = g_new(GCond,1);
    zcmimagesrc->mutx = g_new(GMutex,1);
    g_mutex_init(zcmimagesrc->mutx);
    g_cond_init(zcmimagesrc->cond);
    const char *channel = zcmimagesrc->channel;
    zcmimagesrc->zcm = zcm_create(zcmimagesrc->zcm_url);
    if (!zcmimagesrc->zcm)
    {
        g_print ("Initialization failed\n");
        return FALSE;
    }
    zcm_gstreamer_plugins_image_t_subscribe(zcmimagesrc->zcm, channel, &zcm_image_handler, zcmimagesrc);
    zcm_start(zcmimagesrc->zcm);
    return TRUE;
}

static gboolean gst_zcmimagesrc_start (GstBaseSrc * basesrc)
{
    GstZcmImageSrc *filter = (GstZcmImageSrc *) (basesrc);
    zcm_source_init (filter);
    return TRUE;
}

static void zcm_source_stop (GstZcmImageSrc *filter)
{
// Putting this in causes a deadlock on a failed pipeline
/*
    g_mutex_lock (filter->mutx);
    if (filter->image_info) {
        if (filter->image_info->buf) free(filter->image_info->buf);
        filter->image_info->buf=NULL;
        free(filter->image_info);
        filter->image_info = NULL;
    }
    g_mutex_unlock (filter->mutx);

    g_cond_clear (filter->cond);
    g_mutex_clear (filter->mutx);
    g_free (filter->mutx);
    g_free (filter->cond);
    zcm_stop(filter->zcm);
    zcm_destroy(filter->zcm);
*/
}

static gboolean gst_zcmimagesrc_stop (GstBaseSrc * basesrc)
{
    GstZcmImageSrc *filter = (GstZcmImageSrc *) (basesrc);
    zcm_source_stop(filter);
    return TRUE;
}

static gboolean gst_set_src_caps (GstBaseSrc * src, GstCaps *caps)
{
    GstZcmImageSrc *filter = (GstZcmImageSrc *) (src);

    /*
    for (size_t i = 0; i < gst_caps_get_size(caps); i++) {
        g_print("Caps: %s\n", gst_structure_to_string(gst_caps_get_structure(caps, i)));
    }
    // */

    /* typically we don't output buffers until we have properly parsed some
    * config data, so we should at least know about version.
    * If not, it means it has been requested not to drop data, and
    * upstream and/or app must know what they are doing ... */

    if (filter->frame_info_valid != TRUE) {
        return TRUE;
    }

    if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_I420)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "I420", NULL);
    else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_RGB)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "RGB", NULL);
    else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_BGR)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "BGR", NULL);
    else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_RGBA)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "RGBA", NULL);
    else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_BGRA)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "BGRA", NULL);
    else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_MJPEG)
        gst_caps_set_simple (caps, "format", G_TYPE_STRING, "MJPEG", NULL);
    else
        return FALSE;

    GValue framerate_min = G_VALUE_INIT;
    g_value_init(&framerate_min, GST_TYPE_FRACTION);
    gst_value_set_fraction(&framerate_min, 0, 1);

    GValue framerate_max = G_VALUE_INIT;
    g_value_init(&framerate_max, GST_TYPE_FRACTION);
    gst_value_set_fraction(&framerate_max, 2147483647, 1);

    GValue framerate = G_VALUE_INIT;
    g_value_init(&framerate, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range(&framerate, &framerate_min, &framerate_max);

    gst_caps_set_value (caps, "framerate", &framerate);

    gst_caps_set_simple (caps, "width", G_TYPE_INT, filter->frame_info.width, NULL);
    gst_caps_set_simple (caps, "height", G_TYPE_INT, filter->frame_info.height, NULL);

    // RRR (Bendes): Actually set the caps on the src pad

    return TRUE;

}

static gboolean gst_update_src_caps (GstBaseSrc * src, GstBuffer *buffer)
{
    GstZcmImageSrc *filter = (GstZcmImageSrc *) (src);

    GstCaps *caps = NULL;
    caps = (GstCaps *)gst_type_find_helper_for_buffer (GST_OBJECT (src),
                                                       buffer, NULL);

    /* carry over input caps as much as possible;
    * override with our own stuff */
    if (caps) {
        caps = gst_caps_make_writable (caps);
        // If we want to print / use any of this in the future
        //GstStructure *s = NULL;
        //s = gst_caps_get_structure (caps, 0);
        //int width_temp = 0;
        //gst_structure_get_int (s, "width", &width_temp);
        if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_MJPEG) {
            gst_structure_set_name(gst_caps_get_structure(caps, 0), "image/jpeg");
        } else {
            gst_structure_set_name(gst_caps_get_structure(caps, 0), "video/x-raw");
        }
    } else if (filter->frame_info.frame_type == ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_MJPEG) {
        caps = gst_caps_new_empty_simple("image/jpeg");
    } else {
        caps = gst_caps_new_empty_simple("video/x-raw");
    }

    return gst_base_src_set_caps (src, caps);
}

static GstFlowReturn gst_zcmimagesrc_fill (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer * buf)
{
    GstMapInfo info;
    GstZcmImageSrc *filter = (GstZcmImageSrc *)src;

    gint64 endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
    /* Waiting for buffer */
    g_mutex_lock (filter->mutx);

    /*Condition wait is done to sync with zcm image output*/
    g_cond_wait_until (filter->cond, filter->mutx, endtime);

    if (filter->image_info == NULL)
    {
        g_print ("exceeded waiting time to receive the frame\n");
        g_mutex_unlock (filter->mutx);
        if (filter->update_caps == TRUE)
            return GST_FLOW_ERROR;
        return GST_FLOW_EOS;
    }

    if (filter->update_caps == TRUE)
    {
        filter->frame_info.width = filter->image_info->width;
        filter->frame_info.height = filter->image_info->height;
        filter->frame_info.frame_type = filter->image_info->frame_type;
        filter->frame_info_valid = TRUE;
        if (gst_update_src_caps (src, buf) == FALSE)
        {
            g_print ("frametype %d not supported", filter->frame_info.frame_type);
            return GST_FLOW_ERROR;
        }

        filter->update_caps = FALSE;
    }

    gst_buffer_map (buf, &info, GST_MAP_WRITE);
    gst_buffer_resize (buf, 0, filter->image_info->size);
    memcpy(info.data, filter->image_info->buf, filter->image_info->size);
    gst_buffer_unmap (buf, &info);

    g_mutex_unlock (filter->mutx);
    return GST_FLOW_OK;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_zcmimagesrc_init (GstZcmImageSrc * filter)
{
    filter->verbose = FALSE;
    filter->channel = "GSTREAMER_DATA";
    filter->zcm_url = NULL;
    filter->status = GST_FLOW_OK;
    filter->frame_info_valid = FALSE;
    filter->image_info = NULL;
    gst_base_src_set_blocksize (GST_BASE_SRC (filter), DEFAULT_BLOCKSIZE);
}

static void
gst_zcmimagesrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    GstZcmImageSrc *filter = GST_ZCMIMAGESRC (object);

    switch (prop_id) {
        case PROP_CHANNEL:
            filter->channel = g_strdup (g_value_get_string (value));
            break;
        case PROP_ZCM_URL:
            filter->zcm_url = g_strdup (g_value_get_string (value));
            break;
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gst_zcmimagesrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
    GstZcmImageSrc *filter = GST_ZCMIMAGESRC (object);
    switch (prop_id) {
        case PROP_VERBOSE:
            g_value_set_boolean (value, filter->verbose);
            break;
        case PROP_CHANNEL:
            g_value_set_string (value, filter->channel);
            break;
        case PROP_ZCM_URL:
            g_value_set_string (value, filter->zcm_url);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static GstStateChangeReturn
gst_zcmimagesrc_change_state (GstElement * element, GstStateChange transition)
{

    switch (transition) {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            transition = GST_STATE_CHANGE_PAUSED_TO_PLAYING;
            break;
        case GST_STATE_CHANGE_NULL_TO_READY:
            break;
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            break;
        default :
            g_print ("NULL\n");
            break;
    }

    GstStateChangeReturn ret =
            GST_ELEMENT_CLASS (parent_class)->change_state (element,transition);

    return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
zcmimagesrc_init (GstPlugin * zcmimagesrc)
{
    /* debug category for fltering log messages
     *
     * exchange the string 'Template zcmimagesrc' with your description
     */
    GST_DEBUG_CATEGORY_INIT (gst_zcmimagesrc_debug, "zcmimagesrc",
            0, "Template zcmimagesrc");

    return gst_element_register (zcmimagesrc, "zcmimagesrc", GST_RANK_NONE,
            GST_TYPE_ZCMIMAGESRC);
}

#ifndef VERSION
#define VERSION "1.0.0"
#endif
#ifndef PACKAGE
#define PACKAGE "ZeroCM"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "zcm-gstreamer-plugins"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/ZeroCM/zcm-gstreamer-plugins"
#endif

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    zcmimagesrc,
    "Receives images over zcm and passes them into the pipeline",
    zcmimagesrc_init,
    VERSION,
    "LGPL",
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
