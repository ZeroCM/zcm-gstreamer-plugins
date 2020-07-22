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

#ifndef _GST_ZCMSINK_H_
#define _GST_ZCMSINK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>

#include <zcm/zcm.h>

G_BEGIN_DECLS

#define GST_TYPE_ZCMSINK   (gst_zcmsink_get_type())
#define GST_ZCMSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ZCMSINK,GstZcmsink))
#define GST_ZCMSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ZCMSINK,GstZcmsinkClass))
#define GST_IS_ZCMSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ZCMSINK))
#define GST_IS_ZCMSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ZCMSINK))

typedef struct _GstZcmsink GstZcmsink;
typedef struct _GstZcmsinkClass GstZcmsinkClass;

struct _GstZcmsink
{
  GstVideoSink base_zcmsink;
  GString* url;
  GString* channel;
  zcm_t* zcm;
};

struct _GstZcmsinkClass
{
  GstVideoSinkClass base_zcmsink_class;
};

GType gst_zcmsink_get_type (void);

G_END_DECLS

#endif
