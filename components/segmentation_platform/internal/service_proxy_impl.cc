// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

#include <inttypes.h>
#include <memory>
#include <sstream>

#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/scheduler/execution_service.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {

namespace {
std::string SegmentMetadataToString(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_model_metadata())
    return std::string();

  return "model_metadata: { " +
         metadata_utils::SegmetationModelMetadataToString(
             segment_info.model_metadata()) +
         " }";
}

std::string PredictionResultToString(const proto::SegmentInfo& segment_info) {
  if (!segment_info.has_prediction_result())
    return std::string();
  const auto prediction_result = segment_info.prediction_result();
  base::Time time;
  if (prediction_result.has_timestamp_us()) {
    time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(prediction_result.timestamp_us()));
  }
  std::ostringstream time_string;
  time_string << time;
  return base::StringPrintf(
      "result: %f, time: %s",
      prediction_result.has_result() ? prediction_result.result() : 0,
      time_string.str().c_str());
}

base::flat_set<proto::SegmentId> GetAllSegemntIds(
    const std::vector<std::unique_ptr<Config>>& configs) {
  base::flat_set<proto::SegmentId> all_segment_ids;
  for (const auto& config : configs) {
    for (const auto& segment : config->segments) {
      all_segment_ids.insert(segment.first);
    }
  }
  return all_segment_ids;
}

}  // namespace

ServiceProxyImpl::ServiceProxyImpl(
    SegmentInfoDatabase* segment_db,
    DefaultModelManager* default_manager,
    SignalStorageConfig* signal_storage_config,
    std::vector<std::unique_ptr<Config>>* configs,
    const PlatformOptions& platform_options,
    base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>*
        segment_selectors)
    : force_refresh_results_(platform_options.force_refresh_results),
      segment_db_(segment_db),
      default_manager_(default_manager),
      signal_storage_config_(signal_storage_config),
      configs_(configs),
      segment_selectors_(segment_selectors) {}

ServiceProxyImpl::~ServiceProxyImpl() = default;

void ServiceProxyImpl::AddObserver(ServiceProxy::Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceProxyImpl::RemoveObserver(ServiceProxy::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceProxyImpl::OnServiceStatusChanged(bool is_initialized,
                                              int status_flag) {
  bool changed = (is_service_initialized_ != is_initialized) ||
                 (service_status_flag_ != status_flag);
  is_service_initialized_ = is_initialized;
  service_status_flag_ = status_flag;
  UpdateObservers(changed);
}

void ServiceProxyImpl::UpdateObservers(bool update_service_status) {
  if (observers_.empty())
    return;

  if (update_service_status) {
    for (auto& obs : observers_)
      obs.OnServiceStatusChanged(is_service_initialized_, service_status_flag_);
  }

  if (default_manager_ &&
      (static_cast<int>(ServiceStatus::kSegmentationInfoDbInitialized) &
       service_status_flag_)) {
    default_manager_->GetAllSegmentInfoFromBothModels(
        GetAllSegemntIds(*configs_), segment_db_,
        base::BindOnce(&ServiceProxyImpl::OnGetAllSegmentationInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ServiceProxyImpl::SetExecutionService(
    ExecutionService* model_execution_scheduler) {
  execution_service_ = model_execution_scheduler;
  segment_result_provider_ = SegmentResultProvider::Create(
      segment_db_, signal_storage_config_, default_manager_, execution_service_,
      base::DefaultClock::GetInstance(), /*force_refresh_results=*/true);
}

void ServiceProxyImpl::GetServiceStatus() {
  UpdateObservers(true /* update_service_status */);
}

void ServiceProxyImpl::ExecuteModel(SegmentId segment_id) {
  if (!execution_service_ ||
      segment_id == SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    return;
  }
  auto request = std::make_unique<SegmentResultProvider::GetResultOptions>();
  request->save_results_to_db = true;
  request->segment_id = segment_id;
  request->ignore_db_scores = true;
  segment_result_provider_->GetSegmentResult(std::move(request));
}

void ServiceProxyImpl::OverwriteResult(SegmentId segment_id, float result) {
  if (!execution_service_)
    return;

  if (segment_id != SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    execution_service_->OverwriteModelExecutionResult(
        segment_id, std::make_pair(result, ModelExecutionStatus::kSuccess));
  }
}

void ServiceProxyImpl::SetSelectedSegment(const std::string& segmentation_key,
                                          SegmentId segment_id) {
  if (!segment_selectors_ ||
      segment_selectors_->find(segmentation_key) == segment_selectors_->end()) {
    return;
  }
  if (segment_id != SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {
    auto& selector = segment_selectors_->at(segmentation_key);
    selector->UpdateSelectedSegment(segment_id, 0);
  }
}

void ServiceProxyImpl::OnGetAllSegmentationInfo(
    DefaultModelManager::SegmentInfoList segment_info) {
  if (!configs_)
    return;

  // Convert the |segment_info| vector to a map for quick lookup.
  base::flat_map<SegmentId, proto::SegmentInfo> segment_ids;
  for (const auto& info : segment_info) {
    const SegmentId segment_id = info->segment_info.segment_id();
    switch (info->segment_source) {
      case DefaultModelManager::SegmentSource::DATABASE:
        // If database info is available, then overwrite the existing entry.
        segment_ids[segment_id] = std::move(info->segment_info);
        break;
      case DefaultModelManager::SegmentSource::DEFAULT_MODEL:
        // If database info is not available then use default model info.
        if (segment_ids.count(segment_id) == 0) {
          segment_ids[segment_id] = std::move(info->segment_info);
        }
        break;
    }
  }

  std::vector<ServiceProxy::ClientInfo> result;
  for (const auto& config : *configs_) {
    SegmentId selected = SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
    if (segment_selectors_ &&
        segment_selectors_->find(config->segmentation_key) !=
            segment_selectors_->end()) {
      absl::optional<proto::SegmentId> target =
          segment_selectors_->at(config->segmentation_key)
              ->GetCachedSegmentResult()
              .segment;
      if (target.has_value()) {
        selected = *target;
      }
    }
    result.emplace_back(config->segmentation_key, selected);
    for (const auto& segment_id : config->segments) {
      if (!segment_ids.contains(segment_id.first))
        continue;
      const auto& info = segment_ids[segment_id.first];
      bool can_execute_segment =
          force_refresh_results_ ||
          (signal_storage_config_ &&
           signal_storage_config_->MeetsSignalCollectionRequirement(
               info.model_metadata()));
      result.back().segment_status.emplace_back(
          segment_id.first, SegmentMetadataToString(info),
          PredictionResultToString(info), can_execute_segment);
    }
  }

  for (auto& obs : observers_)
    obs.OnClientInfoAvailable(result);
}

void ServiceProxyImpl::OnModelExecutionCompleted(SegmentId segment_id) {
  // Update the observers with the new execution results.
  UpdateObservers(false);
}

}  // namespace segmentation_platform
