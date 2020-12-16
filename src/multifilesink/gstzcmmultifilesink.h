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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_ZCM_MULTIFILESINK_H_
#define _GST_ZCM_MULTIFILESINK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

#include <stdbool.h>
#include <pthread.h>

#include <zcm/zcm.h>
#include "zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.h"

G_BEGIN_DECLS

#define GST_TYPE_ZCM_MULTIFILESINK   (gst_zcm_multifilesink_get_type())
#define GST_ZCM_MULTIFILESINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZCM_MULTIFILESINK,GstZcmMultiFileSink))
#define GST_ZCM_MULTIFILESINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZCM_MULTIFILESINK,GstZcmMultiFileSinkClass))
#define GST_IS_ZCM_MULTIFILESINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZCM_MULTIFILESINK))
#define GST_IS_ZCM_MULTIFILESINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZCM_MULTIFILESINK))

typedef struct _GstZcmMultiFileSink GstZcmMultiFileSink;
typedef struct _GstZcmMultiFileSinkClass GstZcmMultiFileSinkClass;

struct _GstZcmMultiFileSink
{
  GstVideoSink base_zcmmultifilesink;

  // Privates
  zcm_t* zcm;
  GstVideoInfo info;
  zcm_gstreamer_plugins_photo_t* photo;
  int64_t nwrites;
  int32_t pixelformat;

  pthread_t pub_thr;
  pthread_mutex_t mutex;
  bool exit;

  // Properties
  GString* url;
  GString* channel;
  GString* location;
  gulong   period_us;
};

struct _GstZcmMultiFileSinkClass
{
  GstVideoSinkClass base_zcmmultifilesink_class;
};

GType gst_zcm_multifilesink_get_type (void);

G_END_DECLS

#endif
