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

#ifndef _GST_ZCM_SNAP_H_
#define _GST_ZCM_SNAP_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <zcm/zcm.h>
#include "zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.h"

#include <pthread.h>
#include <stdbool.h>

G_BEGIN_DECLS

#define GST_TYPE_ZCM_SNAP   (gst_zcm_snap_get_type())
#define GST_ZCM_SNAP(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZCM_SNAP,GstZcmSnap))
#define GST_ZCM_SNAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZCM_SNAP,GstZcmSnapClass))
#define GST_IS_ZCM_SNAP(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZCM_SNAP))
#define GST_IS_ZCM_SNAP_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZCM_SNAP))

typedef struct _GstZcmSnap GstZcmSnap;
typedef struct _GstZcmSnapClass GstZcmSnapClass;

struct _GstZcmSnap
{
  GstVideoFilter base_zcmsnap;

  // Privates
  zcm_t* zcm;
  int16_t last_debounce;
  zcm_gstreamer_plugins_snap_t_subscription_t* sub;
  pthread_mutex_t mutex;
  bool take_picture;

  // Properties
  GString* url;
  GString* channel;
};

struct _GstZcmSnapClass
{
  GstVideoFilterClass base_zcmsnap_class;
};

GType gst_zcm_snap_get_type (void);

G_END_DECLS

#endif
