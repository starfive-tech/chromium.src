// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/features.h"

#import "base/feature_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kFullscreenPromosManager{"FullscreenPromosManager",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsFullscreenPromosManagerEnabled() {
  return base::FeatureList::IsEnabled(kFullscreenPromosManager);
}
