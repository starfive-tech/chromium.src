// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"

#import "base/check.h"
#import "base/path_service.h"
#import "components/metrics/persistent_histograms.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/first_run/trending_queries_field_trial.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void IOSChromeFieldTrials::SetUpFieldTrials() {
  // Persistent histograms must be enabled as soon as possible.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistograms(user_data_dir);
  }
}

void IOSChromeFieldTrials::SetUpFeatureControllingFieldTrials(
    bool has_seed,
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list) {
  // Note: On iOS, the `low_entropy_provider` is guaranteed to be non-null.
  DCHECK(low_entropy_provider);

  // Disable trials when testing to remove sources of nondeterminism.
  // WARNING: Do not add any field trials until after this check, or
  // else they will be incorrectly randomized during EG testing.
  if (tests_hook::DisableClientSideFieldTrials()) {
    return;
  }

  // Add code here to enable field trials that are active at first run.
  // See http://crrev/c/1128269 for an example.
  fre_field_trial::Create(*low_entropy_provider, feature_list,
                          GetApplicationContext()->GetLocalState());
  trending_queries_field_trial::Create(
      *low_entropy_provider, feature_list,
      GetApplicationContext()->GetLocalState());
}
