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
 * SECTION:element-gstzcmsnap
 * @title: zcmsnap
 *
 * ZcmSnap blocks all frames except those corresponding to snap commands received over zcm
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! zcmsnap ! fakesink
 * ]|
 * Blocks all frames except those requested by snap_t messages received on the GSTREAMER_SNAP channel
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstzcmsnap.h"

GST_DEBUG_CATEGORY_STATIC (gst_zcm_snap_debug_category);
#define GST_CAT_DEFAULT gst_zcm_snap_debug_category

/* prototypes */


static void gst_zcm_snap_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_zcm_snap_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_zcm_snap_dispose (GObject * object);
static void gst_zcm_snap_finalize (GObject * object);

static gboolean gst_zcm_snap_start (GstBaseTransform * trans);
static gboolean gst_zcm_snap_stop (GstBaseTransform * trans);
static gboolean gst_zcm_snap_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_zcm_snap_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstFlowReturn gst_zcm_snap_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_ZCM_URL,
  PROP_CHANNEL,
};

/* pad templates */
/*
    UYVY
    YUY2 (zcm: YUYV)
    IYU1
    IYU2
    I420
    NV12
    GRAY8 (zcm: GRAY)
    RGB
    BGR
    RGBA
    BGRA
    GRAY16_BE (zcm: BE_GRAY16)
    GRAY16_LE (zcm: LE_GRAY16)
    RGB16 (zcm: LE_RGB16)
*/
#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ UYVY, YUY2, IYU1, IYU2, I420, NV12, GRAY8," \
                        "  RGB, BGR, RGBA, BGRA, GRAY16_BE, GRAY16_LE," \
                        "  RGB16 }")
#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ UYVY, YUY2, IYU1, IYU2, I420, NV12, GRAY8," \
                        "  RGB, BGR, RGBA, BGRA, GRAY16_BE, GRAY16_LE," \
                        "  RGB16 }")


/* Private members */

static void
handler (const zcm_recv_buf_t* rbuf, const char* channel,
         const zcm_gstreamer_plugins_snap_t* msg, void* usr)
{
  GstZcmImageSink* zcmsnap = (GstZcmImageSink*) usr;
  if (zcmsnap->last_debounce != msg->debounce) {
    zcmsnap->last_debounce = msg->debounce;
    pthread_mutex_lock(&zcmsnap->mutex);
    zcmsnap->take_picture = true;
    pthread_mutex_unlock(&zcmsnap->mutex);
  }
}

static void
reinit_zcm (GstZcmImageSink * zcmsnap)
{
  if (zcmsnap->zcm) {
    if (sub) {
      zcm_gstreamer_plugins_snap_t_unsubscribe(zcmsnap->zcm, zcmsnap->sub);
    }
    zcm_stop(zcmsnap->zcm);
    zcm_destroy(zcmsnap->zcm);
  }
  zcmsnap->zcm = zcm_create(zcmsnap->url->str);
  sub = zcm_gstreamer_plugins_snap_t_subscribe(zcmsnap->zcm, zcmimagesink->channel->str, &handler, zcmsnap);
  zcm_start(zcmsnap->zcm);
}

static void
reinit_subscription (GstZcmImageSink * zcmsnap)
{
  assert(zcmsnap->zcm);
  if (sub) {
    zcm_gstreamer_plugins_snap_t_unsubscribe(zcmsnap->zcm, zcmsnap->sub);
  }
  sub = zcm_gstreamer_plugins_snap_t_subscribe(zcmsnap->zcm, zcmimagesink->channel->str, &handler, zcmsnap);
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstZcmSnap, gst_zcm_snap, GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_zcm_snap_debug_category, "zcmsnap", 0,
        "debug category for zcmsnap element"));

static void
gst_zcm_snap_class_init (GstZcmSnapClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "ZcmSnap", "Generic",
      "Blocks all frames except those requested by snap_t messages received over zcm",
      "ZeroCM Team <www.zcm-project.org>");

  gobject_class->set_property = gst_zcm_snap_set_property;
  gobject_class->get_property = gst_zcm_snap_get_property;
  gobject_class->dispose = gst_zcm_snap_dispose;
  gobject_class->finalize = gst_zcm_snap_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_zcm_snap_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_zcm_snap_stop);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_zcm_snap_set_info);
  video_filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_zcm_snap_transform_frame);
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_zcm_snap_transform_frame_ip);


  g_object_class_install_property (gobject_class, PROP_ZCM_URL,
          g_param_spec_string ("url", "Zcm transport url",
              "The full zcm url specifying the zcm transport to be used",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
          g_param_spec_string ("channel", "Zcm publish channel",
              "Channel name for snap_t subscription",
              "GSTREAMER_DATA", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_zcm_snap_init (GstZcmSnap * zcmsnap)
{
  zcmsnap->url = g_string_new("");
  zcmsnap->channel = g_string_new("GSTREAMER_SNAP");
  zcmsnap->zcm = NULL;
  pthread_mutex_init(&zcmsnap->mutex, NULL);

  int16_t last_debounce;
  zcm_gstreamer_plugins_snap_t_subscription_t* sub;
  pthread_mutex_t mutex;
  bool take_picture;
}

void
gst_zcm_snap_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (object);

  GST_DEBUG_OBJECT (zcmsnap, "set_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_string_assign (zcmsnap->url, g_value_get_string (value));
      reinit_zcm(zcmsnap);
      reinit_subscription(zcmsnap);
      break;
    case PROP_CHANNEL:
      g_string_assign (zcmsnap->channel, g_value_get_string (value));
      reinit_subscription(zcmsnap);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcm_snap_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (object);

  GST_DEBUG_OBJECT (zcmsnap, "get_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_value_set_string (value, zcmsnap->url->str);
      break;
    case PROP_CHANNEL:
      g_value_set_string (value, zcmsnap->channel->str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcm_snap_dispose (GObject * object)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (object);

  GST_DEBUG_OBJECT (zcmsnap, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_zcm_snap_parent_class)->dispose (object);
}

void
gst_zcm_snap_finalize (GObject * object)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (object);

  GST_DEBUG_OBJECT (zcmsnap, "finalize");

  /* clean up object here */

  zcm_stop(zcmsnap->zcm);
  zcm_destroy(zcmsnap->zcm);
  zcmsnap->zcm = NULL;

  G_OBJECT_CLASS (gst_zcm_snap_parent_class)->finalize (object);
}

static gboolean
gst_zcm_snap_start (GstBaseTransform * trans)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (trans);

  GST_DEBUG_OBJECT (zcmsnap, "start");

  return TRUE;
}

static gboolean
gst_zcm_snap_stop (GstBaseTransform * trans)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (trans);

  GST_DEBUG_OBJECT (zcmsnap, "stop");

  return TRUE;
}

static gboolean
gst_zcm_snap_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (filter);

  GST_DEBUG_OBJECT (zcmsnap, "set_info");

  return TRUE;
}

/* transform */
static GstFlowReturn
gst_zcm_snap_transform_frame (GstVideoFilter * filter, GstVideoFrame * inframe,
    GstVideoFrame * outframe)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (filter);

  GST_DEBUG_OBJECT (zcmsnap, "transform_frame");

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_zcm_snap_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  GstZcmSnap *zcmsnap = GST_ZCM_SNAP (filter);

  GST_DEBUG_OBJECT (zcmsnap, "transform_frame_ip");

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "zcmsnap", GST_RANK_NONE,
      GST_TYPE_ZCM_SNAP);
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    zcmsnap,
    "Blocks all frames except those requested by snap_t messages received over zcm",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
