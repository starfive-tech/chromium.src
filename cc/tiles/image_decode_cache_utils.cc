// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_

#include "cc/tiles/image_decode_cache_utils.h"

#include "base/check.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace cc {

// static
bool ImageDecodeCacheUtils::ScaleToHalfFloatPixmapUsingN32Intermediate(
    const SkPixmap& source_pixmap,
    SkPixmap* scaled_pixmap,
    PaintFlags::FilterQuality filter_quality) {
  // Target pixmap should be half float backed.
  DCHECK(scaled_pixmap->colorType() == kRGBA_F16_SkColorType);
  // Filter quality should be medium or high. This is needed if the device
  // (Android KitKat and lower) does not support mipmaps properly. Mipmaps are
  // used only for medium and high filter qualities.
  DCHECK(filter_quality >= PaintFlags::FilterQuality::kMedium);

  // Convert to kN32 color type if necessary
  SkPixmap n32_pixmap = source_pixmap;
  SkBitmap n32_bitmap;
  if (source_pixmap.info().colorType() == kRGBA_F16_SkColorType) {
    SkImageInfo n32_image_info =
        source_pixmap.info().makeColorType(kN32_SkColorType);
    if (!n32_bitmap.tryAllocPixels(n32_image_info))
      return false;
    n32_pixmap = n32_bitmap.pixmap();
    source_pixmap.readPixels(n32_pixmap, 0, 0);
  }
  // Scale
  SkBitmap n32_resized_bitmap;
  SkImageInfo n32_resize_info =
      n32_pixmap.info().makeWH(scaled_pixmap->width(), scaled_pixmap->height());
  if (!n32_resized_bitmap.tryAllocPixels(n32_resize_info))
    return false;
  if (!n32_pixmap.scalePixels(
          n32_resized_bitmap.pixmap(),
          PaintFlags::FilterQualityToSkSamplingOptions(filter_quality)))
    return false;
  // Convert back to f16 and return
  return n32_resized_bitmap.readPixels(*scaled_pixmap, 0, 0);
}

// static
bool ImageDecodeCacheUtils::ShouldEvictCaches(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return false;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
