// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"

#include <array>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

namespace {
using proto::SegmentId;

// Default parameters for the model.
constexpr SegmentId kSegmentId = SegmentId::RESUME_HEAVY_USER_SEGMENT;

// InputFeatures.
constexpr std::array<MetadataWriter::UMAFeature, 5> kUMAFeatures = {
    MetadataWriter::UMAFeature::FromUserAction("MobileBookmarkManagerOpen", 7),
    MetadataWriter::UMAFeature::FromUserAction("NewTabPage.MostVisited.Clicked",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("TabGroup.Created.OpenInNewTab",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("Android.HistoryPage.OpenItem",
                                               7),
    MetadataWriter::UMAFeature::FromUserAction("MobileMenuRecentTabs", 7),
};

}  // namespace

ResumeHeavyUserModel::ResumeHeavyUserModel() : ModelProvider(kSegmentId) {}

void ResumeHeavyUserModel::InitAndFetchModel(
    const ModelUpdatedCallback& model_updated_callback) {
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      /*min_signal_collection_length_days=*/7,
      /*signal_storage_length_days=*/14);

  // Set discrete mapping.
  writer.AddBooleanSegmentDiscreteMapping(kResumeHeavyUserKey);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(), kUMAFeatures.size());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(model_updated_callback, kSegmentId,
                                     std::move(metadata), /*model_version=*/1));
}

void ResumeHeavyUserModel::ExecuteModelWithInput(
    const std::vector<float>& inputs,
    ExecutionCallback callback) {
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  const int bookmarks_opened = inputs[0];
  const int mv_tiles_clicked = inputs[1];
  const int opened_ntp_from_tab_groups = inputs[2];
  const int opened_item_from_history = inputs[3];
  const int opened_recent_tabs = inputs[4];
  float result = 0;

  // Determine if the user has used chrome features to resume workflow.
  if (bookmarks_opened >= 2 || mv_tiles_clicked >= 2 ||
      opened_ntp_from_tab_groups >= 2 || opened_item_from_history >= 2 ||
      opened_recent_tabs >= 2) {
    result = 1;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool ResumeHeavyUserModel::ModelAvailable() {
  return true;
}

}  // namespace segmentation_platform
