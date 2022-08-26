// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/constants.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace segmentation_platform {

const char* SegmentationKeyToUmaName(const std::string& segmentation_key) {
  // Please keep in sync with SegmentationKey variant in
  // //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
  // Should also update the field trials allowlist in
  // go/segmentation-field-trials-map.
  if (segmentation_key == kAdaptiveToolbarSegmentationKey) {
    return kAdaptiveToolbarUmaName;
  } else if (segmentation_key == kDummySegmentationKey) {
    return kDummyFeatureUmaName;
  } else if (segmentation_key == kChromeStartAndroidSegmentationKey) {
    return kChromeStartAndroidUmaName;
  } else if (segmentation_key == kQueryTilesSegmentationKey) {
    return kQueryTilesUmaName;
  } else if (segmentation_key == kChromeLowUserEngagementSegmentationKey) {
    return kChromeLowUserEngagementUmaName;
  } else if (segmentation_key == kFeedUserSegmentationKey) {
    return kFeedUserSegmentUmaName;
  } else if (segmentation_key == kContextualPageActionsKey) {
    return kContextualPageActionsUmaName;
  } else if (base::StartsWith(segmentation_key, "test_key")) {
    return "TestKey";
  }
  NOTREACHED();
  return "Unknown";
}

// Please keep in sync with SegmentationModel variant in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
// Should also update the field trials allowlist in
// go/segmentation-field-trials-map.
std::string SegmentIdToHistogramVariant(proto::SegmentId segment_id) {
  switch (segment_id) {
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB:
      return "NewTab";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE:
      return "Share";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE:
      return "Voice";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY:
      return "Dummy";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID:
      return "ChromeStartAndroid";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES:
      return "QueryTiles";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT:
      return "ChromeLowUserEngagement";
    case proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER:
      return "FeedUserSegment";
    case proto::SegmentId::
        OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING:
      return "ContextualPageActionPriceTracking";
    default:
      // This case is reached when UNKNOWN segment is valid, in case of boolean
      // segment results.
      // TODO(crbug.com/1346389): UNKNOWN must be handled separately and add a
      // NOTREACHED() here after fixing tests.
      return "Other";
  }
}

}  // namespace segmentation_platform
