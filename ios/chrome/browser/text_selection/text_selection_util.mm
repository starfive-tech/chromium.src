// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/text_selection_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kEnableExpKitCalendarTextClassifier{
    "EnableExpKitCalendarTextClassifier", base::FEATURE_DISABLED_BY_DEFAULT};
