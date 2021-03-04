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
 * SECTION:element-gstzcmmultifilesink
 * @title: zcmmultifilesink
 *
 * ZcmMultiFileSink sinks data to files and publishes a zcm message for each write
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v videotestsrc ! zcmmultifilesink location=/tmp/%05d.raw
 * ]|
 * Sinks test video images into location and publishes traffic to ZCM_DEFAULT_URL
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "gstzcmmultifilesink.h"

#include "assert.h"
#include "dirent.h"
#include "libgen.h"
#include "stdio.h"

#include <sys/time.h>

#include "zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.h"

GST_DEBUG_CATEGORY_STATIC (gst_zcm_multifilesink_debug_category);
#define GST_CAT_DEFAULT gst_zcm_multifilesink_debug_category

/* prototypes */


static void gst_zcm_multifilesink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_zcm_multifilesink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_zcm_multifilesink_dispose (GObject * object);
static void gst_zcm_multifilesink_finalize (GObject * object);

static GstFlowReturn gst_zcm_multifilesink_show_frame (GstVideoSink *
    video_sink, GstBuffer * buf);

enum
{
  PROP_0,
  PROP_ZCM_URL,
  PROP_CHANNEL,
  PROP_LOCATION,
  PROP_PERIOD_US,
};

/* Private members */

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static inline uint64_t utime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // XXX Not sure if this will work across hour boundaries
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static inline void usleep(uint64_t us)
{
    struct timespec req = { 0 };
    req.tv_sec          = us / 1000000L;
    req.tv_nsec         = (us - req.tv_sec * 1000000L) * 1000L;
    nanosleep(&req, NULL);
}

static inline void printf_int_to_scanf(char* dst, const char* src)
{
    int inIntFmtStr = 0;
    while (*src != '\0') {
        if (inIntFmtStr) {
            if (*src == 'd' || *src == 'i' || *src == 'u' ||
                *src == 'x' || *src == 'X' || *src == 'o') {
                inIntFmtStr = 0;
            } else {
                ++src;
                continue;
            }
        } else if (*src == '%') {
            inIntFmtStr = 1;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void* pub_thread(void* usr)
{
  GstZcmMultiFileSink* zcmmultifilesink = (GstZcmMultiFileSink*) usr;

  bool exit;
  GString* channel = g_string_new("");
  gulong period_us;
  zcm_gstreamer_plugins_photo_t* photo = NULL;

  while (true) {
    pthread_mutex_lock(&zcmmultifilesink->mutex);

        exit = zcmmultifilesink->exit;
        g_string_assign(channel, zcmmultifilesink->channel->str);
        period_us = zcmmultifilesink->period_us;

        if (zcmmultifilesink->photo) {
            if (photo) zcm_gstreamer_plugins_photo_t_destroy(photo);
            photo = zcmmultifilesink->photo;
            zcmmultifilesink->photo = NULL;
        }

    pthread_mutex_unlock(&zcmmultifilesink->mutex);

    if (exit) break;

    if (photo) {
      photo->utime = utime();
      zcm_gstreamer_plugins_photo_t_publish(zcmmultifilesink->zcm, channel->str, photo);
    }

    usleep(period_us);
  }

  if (photo) zcm_gstreamer_plugins_photo_t_destroy(photo);
  g_string_free(channel, false);

  return NULL;
}

static void
destroy_zcm (GstZcmMultiFileSink* zcmmultifilesink)
{
  if (!zcmmultifilesink->zcm) return;

  zcm_stop(zcmmultifilesink->zcm);

  pthread_mutex_lock(&zcmmultifilesink->mutex);
  zcmmultifilesink->exit = true;
  pthread_mutex_unlock(&zcmmultifilesink->mutex);
  pthread_join(zcmmultifilesink->pub_thr, NULL);

  zcm_destroy(zcmmultifilesink->zcm);
  zcmmultifilesink->zcm = NULL;
}

static void
init_zcm (GstZcmMultiFileSink* zcmmultifilesink)
{
  destroy_zcm(zcmmultifilesink);

  zcmmultifilesink->zcm = zcm_create(zcmmultifilesink->url->str);

  zcm_start(zcmmultifilesink->zcm);

  zcmmultifilesink->exit = false;
  pthread_create(&zcmmultifilesink->pub_thr, NULL, pub_thread, zcmmultifilesink);
}


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstZcmMultiFileSink, gst_zcm_multifilesink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_zcm_multifilesink_debug_category,
        "zcmmultifilesink", 0, "debug category for zcmmultifilesink element"));

static gboolean
gst_zcmmultifilesink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (bsink);

  if (gst_caps_get_size(caps) != 1) {
    GST_DEBUG_OBJECT (zcmmultifilesink,
        "Only supporting caps of size 1 %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  GstVideoInfo info;
  if (!gst_video_info_from_caps (&info, caps)) {
    GST_DEBUG_OBJECT (zcmmultifilesink,
        "Could not get video info from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  GstVideoFormat pixelformat = GST_VIDEO_INFO_FORMAT(&info);
  zcmmultifilesink->pixelformat = gst_video_format_to_fourcc (GST_VIDEO_INFO_FORMAT(&info));
  if (zcmmultifilesink->pixelformat == 0) {
    switch (pixelformat) {
      case GST_VIDEO_FORMAT_RGBA:
        zcmmultifilesink->pixelformat = ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_RGBA;
        break;
      default:
        break;
    }
    GstStructure *s = gst_caps_get_structure(caps, 0);
    if (!strcmp("image/jpeg", gst_structure_get_name(s))) {
        zcmmultifilesink->pixelformat = ZCM_GSTREAMER_PLUGINS_IMAGE_T_PIXEL_FORMAT_MJPEG;
    }
  }

  zcmmultifilesink->info = info;

  return TRUE;
}

static void
gst_zcm_multifilesink_class_init (GstZcmMultiFileSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  GstVideoSinkClass *video_sink_class = GST_VIDEO_SINK_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass), &sinktemplate);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "ZcmMultiFileSink", "Generic",
      "Sinks data to files and publishes a zcm message for each write",
      "ZeroCM Team <www.zcm-project.org>");

  gstbasesink_class->set_caps = gst_zcmmultifilesink_setcaps;
  gobject_class->set_property = gst_zcm_multifilesink_set_property;
  gobject_class->get_property = gst_zcm_multifilesink_get_property;
  gobject_class->dispose = gst_zcm_multifilesink_dispose;
  gobject_class->finalize = gst_zcm_multifilesink_finalize;
  video_sink_class->show_frame = GST_DEBUG_FUNCPTR (gst_zcm_multifilesink_show_frame);

  g_object_class_install_property (gobject_class, PROP_ZCM_URL,
          g_param_spec_string ("url", "Zcm transport url",
              "The full zcm url specifying the zcm transport to be used",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
          g_param_spec_string ("channel", "Zcm publish channel",
              "Channel name to publish data to",
              "GSTREAMER_DATA", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOCATION,
          g_param_spec_string ("location", "File save location",
              "Disk location to save files to",
              "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PERIOD_US,
          g_param_spec_string ("period-us", "Publish period us",
              "Publish period of the zcm publish thread in microseconds",
              "100000", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_zcm_multifilesink_init (GstZcmMultiFileSink * zcmmultifilesink)
{
  zcmmultifilesink->url = g_string_new("");
  zcmmultifilesink->photo = NULL;
  zcmmultifilesink->channel = g_string_new("GSTREAMER_DATA");
  zcmmultifilesink->zcm = NULL;
  memset(&zcmmultifilesink->photo, 0, sizeof(zcmmultifilesink->photo));
  zcmmultifilesink->nwrites = 0;
  zcmmultifilesink->location = g_string_new("");
  zcmmultifilesink->period_us = 100 * 1000;
}

void
gst_zcm_multifilesink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (object);

  GST_DEBUG_OBJECT (zcmmultifilesink, "set_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_string_assign (zcmmultifilesink->url, g_value_get_string (value));
      init_zcm(zcmmultifilesink);
      break;
    case PROP_CHANNEL:
      pthread_mutex_lock(&zcmmultifilesink->mutex);
      g_string_assign (zcmmultifilesink->channel, g_value_get_string (value));
      pthread_mutex_unlock(&zcmmultifilesink->mutex);
      break;
    case PROP_LOCATION:
      g_string_assign (zcmmultifilesink->location, g_value_get_string (value));
      assert(strlen(zcmmultifilesink->location->str) < 256 && "location length exceeded");
      {
          char* loc1 = strdup(zcmmultifilesink->location->str);
          char* loc2 = strdup(zcmmultifilesink->location->str);
          // apparently these calls can modify the input,
          // but you actually want the output, so it's ugly
          char* bName = basename(loc1);
          char* dName = dirname(loc2);

          char* sFmt = strdup(bName);
          printf_int_to_scanf(sFmt, bName);

          int found = 0;
          DIR* dir;
          struct dirent *ent;
          dir = opendir(dName);
          if (dir) {
              while ((ent = readdir(dir)) != NULL) {
                  if (strcmp(ent->d_name, ".") == 0 ||
                      strcmp(ent->d_name, "..") == 0) continue;

                  sscanf(ent->d_name, sFmt, &found);
                  if (found >= zcmmultifilesink->nwrites) {
                      zcmmultifilesink->nwrites = found + 1;
                  }
              }
              closedir(dir);
              printf("Found existing files matching output pattern, starting from ");
          } else {
              printf("Output directory not found, starting from ");
          }
          printf(zcmmultifilesink->location->str, zcmmultifilesink->nwrites);
          printf("\n");

          free(loc1);
          free(loc2);
          free(sFmt);
      }
      break;
    case PROP_PERIOD_US:
      pthread_mutex_lock(&zcmmultifilesink->mutex);
      gulong tmp = atol(g_value_get_string(value));
      if (tmp) zcmmultifilesink->period_us = tmp;
      else GST_ERROR_OBJECT (zcmmultifilesink, "Input period-us is invalid");
      pthread_mutex_unlock(&zcmmultifilesink->mutex);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcm_multifilesink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (object);

  GST_DEBUG_OBJECT (zcmmultifilesink, "get_property");

  switch (property_id) {
    case PROP_ZCM_URL:
      g_value_set_string (value, zcmmultifilesink->url->str);
      break;
    case PROP_CHANNEL:
      pthread_mutex_lock(&zcmmultifilesink->mutex);
      g_value_set_string (value, zcmmultifilesink->channel->str);
      pthread_mutex_unlock(&zcmmultifilesink->mutex);
      break;
    case PROP_LOCATION:
      g_value_set_string (value, zcmmultifilesink->location->str);
      break;
    case PROP_PERIOD_US:
      pthread_mutex_lock(&zcmmultifilesink->mutex);
      char str[128];
      snprintf(str, 128, "%lu", zcmmultifilesink->period_us);
      g_value_set_string (value, str);
      pthread_mutex_unlock(&zcmmultifilesink->mutex);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_zcm_multifilesink_dispose (GObject * object)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (object);

  GST_DEBUG_OBJECT (zcmmultifilesink, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_zcm_multifilesink_parent_class)->dispose (object);
}

void
gst_zcm_multifilesink_finalize (GObject * object)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (object);

  GST_DEBUG_OBJECT (zcmmultifilesink, "finalize");

  /* clean up object here */

  destroy_zcm(zcmmultifilesink);

  G_OBJECT_CLASS (gst_zcm_multifilesink_parent_class)->finalize (object);
}

static GstFlowReturn
gst_zcm_multifilesink_show_frame (GstVideoSink * sink, GstBuffer * buf)
{
  GstZcmMultiFileSink *zcmmultifilesink = GST_ZCM_MULTIFILESINK (sink);

  zcm_gstreamer_plugins_photo_t photo;

  GST_DEBUG_OBJECT (zcmmultifilesink, "show_frame");

  if (zcmmultifilesink->location->str[0] == '\0') return GST_FLOW_ERROR;

  if (!zcmmultifilesink->zcm) init_zcm(zcmmultifilesink);

  if (gst_buffer_n_memory (buf) != 1) {
    GST_ERROR_OBJECT (zcmmultifilesink, "Support only 1 memory per buffer");
    return GST_FLOW_ERROR;
  }

  if (!zcmmultifilesink->zcm) return GST_FLOW_ERROR;

  GstVideoFrame src;
  if (!gst_video_frame_map (&src, &zcmmultifilesink->info, buf, GST_MAP_READ)) {
    GST_WARNING_OBJECT (zcmmultifilesink, "could not map image");
    return GST_FLOW_OK;
  }

  GstMapInfo info;
  if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
    GST_WARNING_OBJECT (zcmmultifilesink, "could not map buffer info");
    gst_video_frame_unmap (&src);
    return GST_FLOW_OK;
  }


  // write file
  char file_location_buf[256];
  snprintf(file_location_buf, 256, zcmmultifilesink->location->str, zcmmultifilesink->nwrites);

  FILE *fp;
  fp = fopen(file_location_buf, "w");
  if (fp <= 0) {
    perror("Failed to write file");
  } else {
      int ret = fwrite(info.data, 1, info.size, fp);
      if (ret < 0) {
        fprintf(stderr, "Failed to write file: %s\n", file_location_buf);
      } else {
        zcmmultifilesink->nwrites++;

        photo.width = zcmmultifilesink->info.width;
        photo.height = zcmmultifilesink->info.height;

        photo.pixelformat = zcmmultifilesink->pixelformat;

        gint num_strides = GST_VIDEO_FRAME_N_PLANES (&src);
        photo.stride = malloc (sizeof(int32_t) * num_strides);
        photo.num_strides = num_strides;

        for (size_t i = 0; i < num_strides; ++i) {
          photo.stride[i] = GST_VIDEO_FRAME_PLANE_STRIDE (&src, i);
        }

        photo.data_size = info.size;

        photo.filepath = file_location_buf;

        GstElement *gstElement = GST_ELEMENT (sink);
        GstClockTime baseTime = gst_element_get_base_time (gstElement);
        if (GST_BUFFER_DTS_IS_VALID(buf)) {
          photo.pic_utime = (baseTime + GST_BUFFER_DTS(buf)) / 1e3;
        } else {
          photo.pic_utime = 0;
        }

        photo.utime = utime();

        pthread_mutex_lock(&zcmmultifilesink->mutex);

            zcm_gstreamer_plugins_photo_t_publish(zcmmultifilesink->zcm, zcmmultifilesink->channel->str, &photo);

            if (zcmmultifilesink->photo) {
                zcm_gstreamer_plugins_photo_t_destroy(zcmmultifilesink->photo);
            }
            zcmmultifilesink->photo = zcm_gstreamer_plugins_photo_t_copy(&photo);

        pthread_mutex_unlock(&zcmmultifilesink->mutex);

        free(photo.stride);
      }
  }

  gst_buffer_unmap (buf, &info);
  gst_video_frame_unmap (&src);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "zcmmultifilesink", GST_RANK_NONE,
      GST_TYPE_ZCM_MULTIFILESINK);
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
    zcmmultifilesink,
    "Sinks data to files and publishes a zcm message for each write",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
