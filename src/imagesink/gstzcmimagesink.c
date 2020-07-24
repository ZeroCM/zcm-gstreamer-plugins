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
 * SECTION:element-gstzcmimagesink
 * @title: zcmimagesink
 *
 * ZcmImageSink sinks data from a gstreamer pipeline into a zcm transport
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! zcmimagesink
 * ]|
 * Sinks fake data into the zcm transport defined by ZCM_DEFAULT_URL
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "gstzcmimagesink.h"

GST_DEBUG_CATEGORY_STATIC (gst_zcmimagesink_debug_category);
#define GST_CAT_DEFAULT gst_zcmimagesink_debug_category

/* prototypes */


static void gst_zcmimagesink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_zcmimagesink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_zcmimagesink_dispose (GObject * object);
static void gst_zcmimagesink_finalize (GObject * object);

static GstFlowReturn gst_zcmimagesink_show_frame (GstVideoSink * video_sink,
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


/* Private members */

static void
reinit_zcm (GstZcmImageSink * zcmimagesink)
{
  if (zcmimagesink->zcm) {
      zcm_stop(zcmimagesink->zcm);
      zcm_destroy(zcmimagesink->zcm);
  }
  zcmimagesink->zcm = zcm_create(zcmimagesink->url->str);
  zcm_start(zcmimagesink->zcm);
}


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstZcmImageSink, gst_zcmimagesink, GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_zcmimagesink_debug_category, "zcmimagesink", 0,
        "debug category for zcmimagesink element"));

static gboolean
gst_zcmimagesink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (bsink);

  GstVideoInfo info;
  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (zcmimagesink,
        "Could not get video info from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  zcmimagesink->img.width = info.width;
  zcmimagesink->img.height = info.height;

  GstVideoFormat pixelformat = GST_VIDEO_INFO_FORMAT(&info);
  zcmimagesink->img.pixelformat = gst_video_format_to_fourcc (GST_VIDEO_INFO_FORMAT(&info));
  if (zcmimagesink->img.pixelformat == 0) {
    switch (pixelformat) {
      case GST_VIDEO_FORMAT_RGBA:
        zcmimagesink->img.pixelformat = ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_RGBA;
        break;
      default:
        break;
    }
  }

  zcmimagesink->info = info;

  return TRUE;
}

static void
gst_zcmimagesink_class_init (GstZcmImageSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "ZcmImageSink", "Generic",
      "Sinks data from a pipeline into a zcm transport",
      "ZeroCM Team <www.zcm-project.org>");

  gstbasesink_class->set_caps = gst_zcmimagesink_setcaps;
  gobject_class->set_property = gst_zcmimagesink_set_property;
  gobject_class->get_property = gst_zcmimagesink_get_property;
  gobject_class->dispose = gst_zcmimagesink_dispose;
  gobject_class->finalize = gst_zcmimagesink_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_zcmimagesink_show_frame);

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
gst_zcmimagesink_init (GstZcmImageSink * zcmimagesink)
{
  zcmimagesink->url = g_string_new("");
  zcmimagesink->channel = g_string_new("GSTREAMER_DATA");
  zcmimagesink->zcm = NULL;
  memset(&zcmimagesink->img, 0, sizeof(zcmimagesink->img));
}

void
gst_zcmimagesink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (object);

  GST_DEBUG_OBJECT (zcmimagesink, "set_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_string_assign (zcmimagesink->url, g_value_get_string (value));
      reinit_zcm(zcmimagesink);
      break;
    case PROP_CHANNEL:
      g_string_assign (zcmimagesink->channel, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcmimagesink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (object);

  GST_DEBUG_OBJECT (zcmimagesink, "get_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_value_set_string (value, zcmimagesink->url->str);
      break;
    case PROP_CHANNEL:
      g_value_set_string (value, zcmimagesink->channel->str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcmimagesink_dispose (GObject * object)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (object);

  GST_DEBUG_OBJECT (zcmimagesink, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_zcmimagesink_parent_class)->dispose (object);
}

void
gst_zcmimagesink_finalize (GObject * object)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (object);

  GST_DEBUG_OBJECT (zcmimagesink, "finalize");

  /* clean up object here */

  zcm_stop(zcmimagesink->zcm);
  zcm_destroy(zcmimagesink->zcm);
  zcmimagesink->zcm = NULL;

  if (zcmimagesink->img.stride) {
    free (zcmimagesink->img.stride);
    zcmimagesink->img.num_strides = 0;
  }

  G_OBJECT_CLASS (gst_zcmimagesink_parent_class)->finalize (object);
}

static GstFlowReturn
gst_zcmimagesink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstZcmImageSink *zcmimagesink = GST_ZCMIMAGESINK (sink);

  GST_DEBUG_OBJECT (zcmimagesink, "show_frame");

  if (!zcmimagesink->zcm) reinit_zcm(zcmimagesink);

  if (gst_buffer_n_memory (buf) != 1) {
    GST_ERROR_OBJECT (zcmimagesink, "Support only 1 memory per buffer");
    return GST_FLOW_ERROR;
  }

  if (zcmimagesink->zcm) {

    GstVideoFrame src;
    if (!gst_video_frame_map (&src, &zcmimagesink->info, buf, GST_MAP_READ)) {
      GST_WARNING_OBJECT (zcmimagesink, "could not map image");
      return GST_FLOW_OK;
    }

    gint num_strides = GST_VIDEO_FRAME_N_PLANES (&src);
    if (num_strides != zcmimagesink->img.num_strides) {
      if (zcmimagesink->img.num_strides != 0) {
        free (zcmimagesink->img.stride);
      }
      zcmimagesink->img.stride = malloc (sizeof(int32_t) * num_strides);
      zcmimagesink->img.num_strides = num_strides;
    }

    for (size_t i = 0; i < num_strides; ++i) {
      zcmimagesink->img.stride[i] = GST_VIDEO_FRAME_PLANE_STRIDE (&src, i);
    }

    GstMapInfo info;
    if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
      GST_WARNING_OBJECT (zcmimagesink, "could not map buffer info");
      gst_video_frame_unmap (&src);
      return GST_FLOW_OK;
    }

    zcmimagesink->img.size = info.size;
    zcmimagesink->img.data = info.data;

    zcm_gstreamer_plugins_image_t_publish (zcmimagesink->zcm, zcmimagesink->channel->str, &zcmimagesink->img);

    gst_buffer_unmap (buf, &info);
    gst_video_frame_unmap (&src);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "zcmimagesink", GST_RANK_NONE,
      GST_TYPE_ZCMIMAGESINK);
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
    zcmimagesink,
    "Sinks data from a pipeline into a zcm transport",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

