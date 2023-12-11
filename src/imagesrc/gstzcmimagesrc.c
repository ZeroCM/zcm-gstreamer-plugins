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
 * Receives frames over GSTREAMER_DATA channel
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
    PROP_VERBOSE,
    PROP_CHANNEL,
    PROP_ZCM_URL
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
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
static int  gst_update_src_caps (GstBaseSrc * src, GstZcmImageSrc *filter, GstBuffer *buffer);

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

    g_object_class_install_property (gobject_class, PROP_VERBOSE,
            g_param_spec_boolean ("verbose", "Verbose", "Produce verbose output",
                FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_ZCM_URL,
           g_param_spec_string ("url", "Zcm transport url",
              "The full zcm url specifying the zcm transport to be used",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CHANNEL,
          g_param_spec_string ("channel", "Zcm publish channel",
              "Channel name to publish data to",
              "GSTREAMER_DATA", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_details_simple(gstelement_class,
            "zcmimagesrc",
            "ZCM SOURCE",
            "zcm source subscribe to channel and pass images to next element in pipeline",
            "LIN_H");

    gst_element_class_add_pad_template (gstelement_class,
            gst_static_pad_template_get (&src_factory));

    gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_stop);
    gstbasesrc_class->fill = GST_DEBUG_FUNCPTR (gst_zcmimagesrc_fill);
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
        ZcmImageInfo * image_info = malloc (sizeof(ZcmImageInfo));
        image_info->width  = img->width;
        image_info->height = img->height;
        image_info->size   = img->size;
        image_info->framerate_num = 0;
        image_info->framerate_den = 1;
        image_info->frame_type =  img->pixelformat;
        image_info->buf = malloc (img->size);
        memcpy (image_info->buf, img->data, img->size);
        g_queue_push_head (zcmimagesrc->buf_queue, image_info);
        g_cond_broadcast(zcmimagesrc->cond);
    }
    g_mutex_unlock (zcmimagesrc->mutx);
}

static gboolean zcm_source_init (GstZcmImageSrc *zcmimagesrc)
{
    zcmimagesrc->buf_queue= g_queue_new();
    zcmimagesrc->update_caps= TRUE;
    zcmimagesrc->cond = g_new(GCond,1);
    zcmimagesrc->mutx =g_new(GMutex,1);
    g_mutex_init(zcmimagesrc->mutx);
    g_cond_init(zcmimagesrc->cond);
    const char *channel =  zcmimagesrc->channel;
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

/*
static void zcm_source_stop (GstZcmImageSrc *filter)
{
    g_mutex_lock (filter->mutx);
    ZcmImageInfo * frames = g_queue_pop_tail(filter->buf_queue);
    while (frames)
    {
        free(frames->buf);
        frames->buf=NULL;
        free(frames);
        frames = g_queue_pop_tail (filter->buf_queue);
    }
    g_queue_free (filter->buf_queue);
    g_mutex_unlock (filter->mutx);

    g_cond_clear (filter->cond);
    g_mutex_clear (filter->mutx);
    g_free (filter->mutx);
    g_free (filter->cond);
    zcm_stop(filter->zcm);
    zcm_destroy(filter->zcm);
}
*/

static gboolean gst_zcmimagesrc_stop (GstBaseSrc * basesrc)
{
    // GstZcmImageSrc *filter = (GstZcmImageSrc *) (basesrc);
    g_print ("stop called\n");
    return TRUE;
}

static int
gst_update_src_caps (GstBaseSrc * src, GstZcmImageSrc *filter, GstBuffer *buffer)
{
    GstCaps *caps = NULL;

    caps = (GstCaps *)gst_type_find_helper_for_buffer (GST_OBJECT (src),
                                                                buffer, NULL);

    /* carry over input caps as much as possible;
    * override with our own stuff */

    if (caps)
    {
        caps = gst_caps_make_writable (caps);
        // If we want to print / use any of this in the future
        //GstStructure *s = NULL;
        //s = gst_caps_get_structure (caps, 0);
        //int width_temp = 0;
        //gst_structure_get_int (s, "width", &width_temp);
    } else {
        caps = gst_caps_new_empty_simple("video/x-raw");
        //caps = gst_caps_new_full(gst_structure_new_empty("image/jpeg"),
                                 //gst_structure_new_empty("video/x-raw"),
                                 //NULL);
    }


    /* typically we don't output buffers until we have properly parsed some
    * config data, so we should at least know about version.
    * If not, it means it has been requested not to drop data, and
    * upstream and/or app must know what they are doing ... */

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
        return -1;

    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION, 25, 1, NULL);
    gst_caps_set_simple (caps, "width", G_TYPE_INT, filter->frame_info.width,
            "height", G_TYPE_INT, filter->frame_info.height, NULL);
    gst_base_src_set_caps (src, caps);
    return 0;

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

    ZcmImageInfo * frames = g_queue_pop_tail(filter->buf_queue);
    if (frames == NULL)
    {
        g_print ("exceded waiting time to receive the frame\n");
        g_mutex_unlock (filter->mutx);
        if (filter->update_caps == TRUE)
            return GST_FLOW_ERROR;
        return GST_FLOW_EOS;
    }

    if (filter->update_caps == TRUE)
    {
        filter->frame_info.width = frames->width;
        filter->frame_info.height = frames->height;
        filter->frame_info.framerate_num = frames->framerate_num;
        filter->frame_info.framerate_den = frames->framerate_den;
        filter->frame_info.frame_type = frames->frame_type;
        if (gst_update_src_caps (src, filter,buf) == -1)
        {
            g_print ("frametype %d not supported", filter->frame_info.frame_type);
            return GST_FLOW_ERROR;
        }

        filter->update_caps = FALSE;
    }

    gst_buffer_map (buf, &info, GST_MAP_WRITE);
    memcpy(info.data, frames->buf, frames->size);
    gst_buffer_resize (buf, 0, frames->size);
    gst_buffer_unmap (buf, &info);

    free(frames->buf);
    frames->buf=NULL;
    free(frames);
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
    filter->channel = NULL;
    filter->status = GST_FLOW_OK;
    gst_base_src_set_blocksize (GST_BASE_SRC (filter), DEFAULT_BLOCKSIZE);
}

static void
gst_zcmimagesrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    GstZcmImageSrc *filter = GST_ZCMIMAGESRC (object);

    switch (prop_id) {
        case PROP_VERBOSE:
            filter->verbose = g_value_get_boolean (value);
            break;
        case PROP_CHANNEL:
            filter->channel = g_strdup (g_value_get_string (value));
            break;
        case PROP_ZCM_URL:
            filter->zcm_url = g_strdup (g_value_get_string (value));
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
