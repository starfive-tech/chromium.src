// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {
namespace {

using SuggestResults = std::vector<FileSuggestData>;

constexpr char kSchema[] = "zero_state_drive://";

// How long to wait before making the first request for results from the
// ItemSuggestCache.
constexpr base::TimeDelta kFirstUpdateDelay = base::Seconds(10);

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.DriveZeroStateProvider.Latency",
                          latency);
}

// TODO(crbug.com/1258415): This exists to reroute results depending on which
// launcher is enabled, and should be removed after the new launcher launch.
ash::SearchResultDisplayType GetDisplayType() {
  return ash::features::IsProductivityLauncherEnabled()
             ? ash::SearchResultDisplayType::kContinue
             : ash::SearchResultDisplayType::kList;
}

bool IsDriveDisabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

}  // namespace

ZeroStateDriveProvider::ZeroStateDriveProvider(
    Profile* profile,
    SearchController* search_controller,
    drive::DriveIntegrationService* drive_service,
    session_manager::SessionManager* session_manager)
    : profile_(profile),
      drive_service_(drive_service),
      session_manager_(session_manager),
      file_suggest_service_(
          FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile)),
      construction_time_(base::Time::Now()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);

  // `FileSuggestKeyedServiceFactory` ensures to build the keyed
  // service when the app list syncable service is built. Meanwhile,
  // `ZeroStateDriveProvider` is built only when the app list syncable service
  // exists. Therefore, `file_suggest_service_` should always be true.
  DCHECK(file_suggest_service_);

  if (drive_service_) {
    if (drive_service_->IsMounted()) {
      // DriveFS is mounted, so we can fetch results immediately.
      OnFileSystemMounted();
    } else {
      // Wait for DriveFS to be mounted, then fetch results. This happens in
      // OnFileSystemMounted.
      drive_observation_.Observe(drive_service_);
    }
  }

  if (session_manager_)
    session_observation_.Observe(session_manager_);

  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager)
    power_observation_.Observe(power_manager);

  file_suggest_service_observation_.Observe(file_suggest_service_);
}

ZeroStateDriveProvider::~ZeroStateDriveProvider() = default;

void ZeroStateDriveProvider::OnFileSystemMounted() {
  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_filesystem_mounted", true);

  if (kUpdateCache) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ZeroStateDriveProvider::MaybeUpdateCache,
                       weak_factory_.GetWeakPtr()),
        kFirstUpdateDelay);
  }
}

void ZeroStateDriveProvider::OnSessionStateChanged() {
  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_session_state_changed", true);

  // Update cache if the user has logged in.
  if (session_manager_->session_state() ==
          session_manager::SessionState::ACTIVE &&
      kUpdateCache) {
    MaybeUpdateCache();
  }
}

void ZeroStateDriveProvider::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_screen_idle_state_changed", true);

  // Update cache if the screen changed from off to on.
  if (screen_off_ && !proto.dimmed() && !proto.off() && kUpdateCache) {
    MaybeUpdateCache();
  }
  screen_off_ = proto.off();
}

void ZeroStateDriveProvider::ViewClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher, "itemsuggest_query_on_view_closing",
      true);
  if (kUpdateCache) {
    MaybeUpdateCache();
  }
}

ash::AppListSearchResultType ZeroStateDriveProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateDrive;
}

bool ZeroStateDriveProvider::ShouldBlockZeroState() const {
  return true;
}

void ZeroStateDriveProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
}

void ZeroStateDriveProvider::StartZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();

  query_start_time_ = base::TimeTicks::Now();

  // Cancel any in-flight queries for this provider.
  weak_factory_.InvalidateWeakPtrs();

  file_suggest_service_->GetSuggestFileData(
      FileSuggestKeyedService::SuggestionType::kItemSuggest,
      base::BindOnce(&ZeroStateDriveProvider::OnSuggestFileDataFetched,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateDriveProvider::OnSuggestFileDataFetched(
    absl::optional<SuggestResults> suggest_results) {
  // Fail to fetch the suggest data, so return early.
  if (!suggest_results)
    return;

  SetSearchResults(*suggest_results);
}

void ZeroStateDriveProvider::SetSearchResults(SuggestResults suggest_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(suggest_results.size());
  int item_index = 0;
  SearchProvider::Results provider_results;
  for (const auto& result : suggest_results) {
    const double score = 1.0 - (item_index / total_items);
    ++item_index;

    provider_results.emplace_back(
        MakeListResult(result.file_path, result.prediction_reason, score));
  }

  SwapResults(&provider_results);
  LogLatency(base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeListResult(
    const base::FilePath& filepath,
    const absl::optional<std::string>& prediction_reason,
    const float relevance) {
  absl::optional<std::u16string> details;
  if (prediction_reason && ash::features::IsProductivityLauncherEnabled())
    details = base::UTF8ToUTF16(prediction_reason.value());

  auto result = std::make_unique<FileResult>(
      kSchema, filepath, details, ash::AppListSearchResultType::kZeroStateDrive,
      GetDisplayType(), relevance, std::u16string(), FileResult::Type::kFile,
      profile_);
  return result;
}

void ZeroStateDriveProvider::MaybeUpdateCache() {
  if (base::Time::Now() - kFirstUpdateDelay > construction_time_) {
    file_suggest_service_->MaybeUpdateItemSuggestCache(
        base::PassKey<ZeroStateDriveProvider>());
  }
}

void ZeroStateDriveProvider::OnFileSuggestionUpdated(
    FileSuggestKeyedService::SuggestionType type) {
  DCHECK_EQ(FileSuggestKeyedService::SuggestionType::kItemSuggest, type);
  StartZeroState();
}

}  // namespace app_list
