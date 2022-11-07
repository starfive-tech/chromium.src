// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/in_memory_url_index.h"

#include <cinttypes>
#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "components/omnibox/common/omnibox_features.h"

// Initializes a allowlist of URL schemes.
void InitializeSchemeAllowlist(SchemeSet* allowlist,
                               const SchemeSet& client_schemes_to_allowlist) {
  DCHECK(allowlist);
  if (!allowlist->empty())
    return;  // Nothing to do, already initialized.

  allowlist->insert(client_schemes_to_allowlist.begin(),
                    client_schemes_to_allowlist.end());

  allowlist->insert(std::string(url::kAboutScheme));
  allowlist->insert(std::string(url::kFileScheme));
  allowlist->insert(std::string(url::kFtpScheme));
  allowlist->insert(std::string(url::kHttpScheme));
  allowlist->insert(std::string(url::kHttpsScheme));
  allowlist->insert(std::string(url::kMailToScheme));
}

InMemoryURLIndex::RestoreCacheObserver::~RestoreCacheObserver() = default;

// RebuildPrivateDataFromHistoryDBTask -----------------------------------------

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    RebuildPrivateDataFromHistoryDBTask(InMemoryURLIndex* index,
                                        const SchemeSet& scheme_allowlist)
    : index_(index),
      scheme_allowlist_(scheme_allowlist),
      succeeded_(false),
      task_creation_time_(base::TimeTicks::Now()) {}

bool InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  data_ = URLIndexPrivateData::RebuildFromHistory(db, scheme_allowlist_);
  succeeded_ = data_.get() && !data_->Empty();
  if (!succeeded_ && data_.get())
    data_->Clear();
  return true;
}

void InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    DoneRunOnMainThread() {
  index_->DoneRebuildingPrivateDataFromHistoryDB(succeeded_, data_);
  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexingTime.RoundTripTime",
                      base::TimeTicks::Now() - task_creation_time_);
}

InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask::
    ~RebuildPrivateDataFromHistoryDBTask() {
}

// InMemoryURLIndex ------------------------------------------------------------

InMemoryURLIndex::InMemoryURLIndex(bookmarks::BookmarkModel* bookmark_model,
                                   history::HistoryService* history_service,
                                   TemplateURLService* template_url_service,
                                   const base::FilePath& history_dir,
                                   const SchemeSet& client_schemes_to_allowlist)
    : bookmark_model_(bookmark_model),
      history_service_(history_service),
      template_url_service_(template_url_service),
      private_data_(new URLIndexPrivateData),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  InitializeSchemeAllowlist(&scheme_allowlist_, client_schemes_to_allowlist);
  // TODO(mrossetti): Register for language change notifications.
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "InMemoryURLIndex", base::ThreadTaskRunnerHandle::Get());
}

InMemoryURLIndex::~InMemoryURLIndex() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  DCHECK(!history_service_);
  DCHECK(shutdown_);
}

void InMemoryURLIndex::Init() {
  if (!history_service_)
    return;

  if (history_service_->backend_loaded()) {
    ScheduleRebuildFromHistory();
  } else {
    listen_to_history_service_loaded_ = true;
  }
}

void InMemoryURLIndex::ClearPrivateData() {
  private_data_->Clear();
}

// Querying --------------------------------------------------------------------

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const std::u16string& term_string,
    size_t cursor_position,
    size_t max_matches) {
  return private_data_->HistoryItemsForTerms(
      term_string, cursor_position, max_matches, bookmark_model_,
      template_url_service_);
}

// Updating --------------------------------------------------------------------

void InMemoryURLIndex::DeleteURL(const GURL& url) {
  private_data_->DeleteURL(url);
}

void InMemoryURLIndex::OnURLVisited(history::HistoryService* history_service,
                                    const history::URLRow& url_row,
                                    const history::VisitRow& new_visit) {
  DCHECK_EQ(history_service_, history_service);
  private_data_->UpdateURL(history_service_, url_row, scheme_allowlist_,
                           &private_data_tracker_);
}

void InMemoryURLIndex::OnURLsModified(history::HistoryService* history_service,
                                      const history::URLRows& changed_urls) {
  DCHECK_EQ(history_service_, history_service);
  for (const auto& row : changed_urls) {
    private_data_->UpdateURL(history_service_, row, scheme_allowlist_,
                             &private_data_tracker_);
  }
}

void InMemoryURLIndex::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    ClearPrivateData();
  } else {
    for (const auto& row : deletion_info.deleted_rows())
      private_data_->DeleteURL(row.url());
  }
}

void InMemoryURLIndex::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  if (listen_to_history_service_loaded_)
    ScheduleRebuildFromHistory();
  listen_to_history_service_loaded_ = false;
}

bool InMemoryURLIndex::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(scheme_allowlist_);

  // TODO(dyaroshev): Add support for scoped_refptr in
  //                  base::trace_event::EstimateMemoryUsage.
  res += sizeof(URLIndexPrivateData) + private_data_->EstimateMemoryUsage();

  const std::string dump_name =
      base::StringPrintf("omnibox/in_memory_url_index/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = process_memory_dump->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, res);
  return true;
}

// Cleanup ---------------------------------------------------------------------

void InMemoryURLIndex::Shutdown() {
  if (history_service_) {
    history_service_observation_.Reset();
    history_service_ = nullptr;
  }
  shutdown_ = true;
  private_data_tracker_.TryCancelAll();

#ifndef LEAK_SANITIZER
  // Intentionally create and then leak a scoped_refptr to private_data_. This
  // permanently raises the reference count so that the URLIndexPrivateData
  // destructor won't run during browser shutdown. This saves having to walk the
  // maps to free their memory, which saves time and avoids shutdown hangs,
  // especially if some of the memory has been paged out.
  base::NoDestructor<scoped_refptr<URLIndexPrivateData>> leak_reference(
      private_data_);
#endif
}

// Restoring from the History DB -----------------------------------------------

void InMemoryURLIndex::ScheduleRebuildFromHistory() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "omnibox", "InMemoryURLIndex::ScheduleRebuildFromHistory",
      TRACE_ID_LOCAL(this));
  DCHECK(history_service_);
  history_service_->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new InMemoryURLIndex::RebuildPrivateDataFromHistoryDBTask(
              this, scheme_allowlist_)),
      &private_data_tracker_);
}

void InMemoryURLIndex::DoneRebuildingPrivateDataFromHistoryDB(
    bool succeeded,
    scoped_refptr<URLIndexPrivateData> private_data) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "omnibox", "InMemoryURLIndex::ScheduleRebuildFromHistory",
      TRACE_ID_LOCAL(this));
  DCHECK(thread_checker_.CalledOnValidThread());
  if (succeeded) {
    private_data_tracker_.TryCancelAll();
    private_data_ = private_data;
  } else {
    private_data_->Clear();
  }
  restored_ = true;
  if (restore_cache_observer_)
    restore_cache_observer_->OnCacheRestoreFinished(succeeded);
}

void InMemoryURLIndex::RebuildFromHistory(
    history::HistoryDatabase* history_db) {
  private_data_tracker_.TryCancelAll();
  private_data_ =
      URLIndexPrivateData::RebuildFromHistory(history_db, scheme_allowlist_);
}
