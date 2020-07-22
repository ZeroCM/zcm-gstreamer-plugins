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

#ifndef _GST_ZCMIMAGESINK_H_
#define _GST_ZCMIMAGESINK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

#include <zcm/zcm.h>
#include "image_t.h"

G_BEGIN_DECLS

#define GST_TYPE_ZCMIMAGESINK (gst_zcmimagesink_get_type())
#define GST_ZCMIMAGESINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZCMIMAGESINK,GstZcmImageSink))
#define GST_ZCMIMAGESINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZCMIMAGESINK,GstZcmImageSinkClass))
#define GST_IS_ZCMIMAGESINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZCMIMAGESINK))
#define GST_IS_ZCMIMAGESINK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZCMIMAGESINK))

typedef struct _GstZcmImageSink GstZcmImageSink;
typedef struct _GstZcmImageSinkClass GstZcmImageSinkClass;

struct _GstZcmImageSink
{
  GstVideoSink base_zcmimagesink;

  // Privates
  zcm_t* zcm;
  GstVideoInfo info;
  image_t img;

  // Properties
  GString* url;
  GString* channel;
};

struct _GstZcmImageSinkClass
{
  GstVideoSinkClass base_zcmimagesink_class;
};

GType gst_zcmimagesink_get_type (void);

G_END_DECLS

#endif
