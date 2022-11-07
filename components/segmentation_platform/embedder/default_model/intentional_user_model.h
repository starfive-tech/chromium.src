// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_INTENTIONAL_USER_MODEL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_INTENTIONAL_USER_MODEL_H_

#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Segmentation intentional user model provider. Provides a default model and
// metadata for the intentional user segment.
// TODO(crbug/1357107): Add support for non-android platforms.
class IntentionalUserModel : public ModelProvider {
 public:
  IntentionalUserModel();
  ~IntentionalUserModel() override = default;

  // Disallow copy/assign.
  IntentionalUserModel(IntentionalUserModel&) = delete;
  IntentionalUserModel& operator=(IntentionalUserModel&) = delete;

  // ModelProvider implementation.
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const std::vector<float>& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_INTENTIONAL_USER_MODEL_H_
