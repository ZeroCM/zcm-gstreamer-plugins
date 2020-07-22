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
 * SECTION:element-gstzcmsink
 *
 * The zcmsink element sinks data from a gstreamer pipeline into a
 * zcm transport
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! zcmsink
 * ]|
 * Sinks fake data into the zcm transport defined by ZCM_DEFAULT_URL
 * </refsect2>
 */

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "gstzcmsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_zcmsink_debug_category);
#define GST_CAT_DEFAULT gst_zcmsink_debug_category

/* prototypes */


static void gst_zcmsink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_zcmsink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_zcmsink_dispose (GObject * object);
static void gst_zcmsink_finalize (GObject * object);

static GstFlowReturn gst_zcmsink_show_frame (GstVideoSink * video_sink,
    GstBuffer * buf);

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
/* FIXME: add/remove formats you can handle */
#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ UYVY, YUY2, IYU1, IYU2, I420, NV12, GRAY8," \
                        "  RGB, BGR, RGBA, BGRA, GRAY16_BE, GRAY16_LE," \
                        "  RGB16 }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstZcmsink, gst_zcmsink, GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_zcmsink_debug_category, "zcmsink", 0,
        "debug category for zcmsink element"));

static void
gst_zcmsink_class_init (GstZcmsinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "GstZcmSink", "Generic",
      "Sinks data from a pipeline into a zcm transport",
      "ZeroCM Team <www.zcm-project.org>");

  gobject_class->set_property = gst_zcmsink_set_property;
  gobject_class->get_property = gst_zcmsink_get_property;
  gobject_class->dispose = gst_zcmsink_dispose;
  gobject_class->finalize = gst_zcmsink_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_zcmsink_show_frame);

  g_object_class_install_property (gobject_class, PROP_ZCM_URL,
          g_param_spec_string ("url", "Zcm transport url",
              "The full zcm url specifying the zcm transport to be used",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
          g_param_spec_string ("channel", "Zcm publish channel",
              "Channel name to publish data to",
              "GSTREAMER_DATA", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_zcmsink_init (GstZcmsink * zcmsink)
{
  zcmsink->url = g_string_new("");
  zcmsink->channel = g_string_new("GSTREAMER_DATA");
  zcmsink->zcm = NULL;
}

static void
reinit_zcm (GstZcmsink * zcmsink)
{
  if (zcmsink->zcm) {
      zcm_stop(zcmsink->zcm);
      zcm_destroy(zcmsink->zcm);
  }
  zcmsink->zcm = zcm_create(zcmsink->url->str);
  zcm_start(zcmsink->zcm);
}

void
gst_zcmsink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZcmsink *zcmsink = GST_ZCMSINK (object);

  GST_DEBUG_OBJECT (zcmsink, "set_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_string_assign (zcmsink->url, g_value_get_string (value));
      reinit_zcm(zcmsink);
      break;
    case PROP_CHANNEL:
      g_string_assign (zcmsink->channel, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcmsink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstZcmsink *zcmsink = GST_ZCMSINK (object);

  GST_DEBUG_OBJECT (zcmsink, "get_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_value_set_string (value, zcmsink->url->str);
      break;
    case PROP_CHANNEL:
      g_value_set_string (value, zcmsink->channel->str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcmsink_dispose (GObject * object)
{
  GstZcmsink *zcmsink = GST_ZCMSINK (object);

  GST_DEBUG_OBJECT (zcmsink, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_zcmsink_parent_class)->dispose (object);
}

void
gst_zcmsink_finalize (GObject * object)
{
  GstZcmsink *zcmsink = GST_ZCMSINK (object);

  GST_DEBUG_OBJECT (zcmsink, "finalize");

  /* clean up object here */

  zcm_stop(zcmsink->zcm);
  zcm_destroy(zcmsink->zcm);
  zcmsink->zcm = NULL;

  G_OBJECT_CLASS (gst_zcmsink_parent_class)->finalize (object);
}

static GstFlowReturn
gst_zcmsink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstZcmsink *zcmsink = GST_ZCMSINK (sink);

  GST_DEBUG_OBJECT (zcmsink, "show_frame");

  if (gst_buffer_n_memory (buf) != 1) {
    GST_ERROR_OBJECT (zcmsink, "Support only 1 memory per buffer");
    return GST_FLOW_ERROR;
  }

  if (zcmsink->zcm) {
    GstMapInfo info;
    gst_buffer_map (buf, &info, GST_MAP_READ);
    zcm_publish (zcmsink->zcm, zcmsink->channel->str, info.data, info.size);
    gst_buffer_unmap (buf, &info);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "zcmsink", GST_RANK_NONE,
      GST_TYPE_ZCMSINK);
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
    zcmsink,
    "Sinks data from a pipeline into a zcm transport",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

