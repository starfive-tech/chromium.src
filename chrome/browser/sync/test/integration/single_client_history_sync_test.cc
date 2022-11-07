// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace sync_pb {

// Makes the GMock matchers print out a readable version of the protobuf.
void PrintTo(const HistorySpecifics& history, std::ostream* os) {
  *os << "[ Visit time: " << history.visit_time_windows_epoch_micros()
      << ", Originator: " << history.originator_cache_guid()
      << ", Redirects: ( ";
  for (int i = 0; i < history.redirect_entries_size(); i++) {
    *os << history.redirect_entries(i).url() << " ";
  }
  *os << "), Transition: " << history.page_transition().core_transition()
      << ", Referring visit: " << history.originator_referring_visit_id()
      << ", Duration: " << history.visit_duration_micros() << " ]";
}

}  // namespace sync_pb

namespace {

using testing::AllOf;
using testing::Not;
using testing::UnorderedElementsAre;

MATCHER_P(UrlIs, url, "") {
  if (arg.redirect_entries_size() != 1) {
    return false;
  }
  return arg.redirect_entries(0).url() == url;
}

MATCHER_P(CoreTransitionIs, transition, "") {
  return arg.page_transition().core_transition() == transition;
}

MATCHER(HasReferringVisit, "") {
  return arg.originator_referring_visit_id() != 0;
}

MATCHER(HasVisitDuration, "") {
  return arg.visit_duration_micros() > 0;
}

MATCHER(StandardFieldsArePopulated, "") {
  // Checks all fields that should never be empty/unset/default. Some fields can
  // be legitimately empty, or are set after an entity is first created.
  // May be legitimately empty:
  //   redirect_entries.title, redirect_entries.redirect_type,
  //   originator_referring_visit_id, originator_opener_visit_id,
  //   root_task_id, parent_task_id
  // Populated later:
  //   visit_duration_micros, page_language, password_state
  return arg.visit_time_windows_epoch_micros() > 0 &&
         !arg.originator_cache_guid().empty() &&
         arg.redirect_entries_size() > 0 &&
         arg.redirect_entries(0).originator_visit_id() > 0 &&
         !arg.redirect_entries(0).url().empty() && arg.has_browser_type() &&
         arg.window_id() > 0 && arg.tab_id() > 0 && arg.task_id() > 0 &&
         arg.http_response_code() > 0;
}

std::vector<sync_pb::HistorySpecifics> SyncEntitiesToHistorySpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::HistorySpecifics> history;
  for (sync_pb::SyncEntity& entity : entities) {
    DCHECK(entity.specifics().has_history());
    history.push_back(std::move(entity.specifics().history()));
  }
  return history;
}

// A helper class that waits for the HISTORY entities on the FakeServer to match
// a given GMock matcher.
class ServerHistoryMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::HistorySpecifics>>;

  explicit ServerHistoryMatchChecker(const Matcher& matcher);
  ~ServerHistoryMatchChecker() override;
  ServerHistoryMatchChecker(const ServerHistoryMatchChecker&) = delete;
  ServerHistoryMatchChecker& operator=(const ServerHistoryMatchChecker&) =
      delete;

  // FakeServer::Observer overrides.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

ServerHistoryMatchChecker::ServerHistoryMatchChecker(const Matcher& matcher)
    : matcher_(matcher) {}

ServerHistoryMatchChecker::~ServerHistoryMatchChecker() = default;

void ServerHistoryMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::HISTORY)) {
    CheckExitCondition();
  }
}

bool ServerHistoryMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::HistorySpecifics> entities =
      SyncEntitiesToHistorySpecifics(
          fake_server()->GetSyncEntitiesByModelType(syncer::HISTORY));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

class SingleClientHistorySyncTest : public SyncTest {
 public:
  SingleClientHistorySyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(syncer::kSyncEnableHistoryDataType);
  }
  ~SingleClientHistorySyncTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(embedded_test_server()->Start());

    SyncTest::SetUpOnMainThread();
  }

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

#if !BUILDFLAG(IS_ANDROID)
    // On non-Android platforms, SyncTest doesn't create any tabs in the
    // profiles/browsers it creates. Create an "empty" tab here, so that
    // NavigateToURL() will have a non-null WebContents to navigate in.
    for (int i = 0; i < num_clients(); ++i) {
      if (!AddTabAtIndexToBrowser(GetBrowser(0), 0, GURL("about:blank"),
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
        return false;
      }
    }
#endif

    return true;
  }

  void NavigateToURL(const GURL& url,
                     ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED,
                     const GURL& referrer = GURL()) {
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = transition;
    if (referrer.is_valid()) {
      params.referrer =
          content::Referrer(referrer, network::mojom::ReferrerPolicy::kAlways);
    }
    content::NavigateToURLBlockUntilNavigationsComplete(GetActiveWebContents(),
                                                        params, 1);
  }

  bool WaitForHistory(
      testing::Matcher<std::vector<sync_pb::HistorySpecifics>> matcher) {
    return ServerHistoryMatchChecker(matcher).Wait();
  }

  std::vector<sync_pb::HistorySpecifics> GetAllServerHistory() {
    return SyncEntitiesToHistorySpecifics(
        fake_server_->GetSyncEntitiesByModelType(syncer::HISTORY));
  }

 private:
  content::WebContents* GetActiveWebContents() {
#if BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetActiveWebContents(this);
#else
    // Note: chrome_test_utils::GetActiveWebContents() doesn't work on
    // non-Android platforms, since it uses the profile created by
    // InProcessBrowserTest, not the profile(s) from SyncTest.
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
#endif
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest,
                       DoesNotUploadRetroactively) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Navigate somewhere before Sync is turned on.
  GURL not_synced_url =
      embedded_test_server()->GetURL("not-synced.com", "/sync/simple.html");
  NavigateToURL(not_synced_url);

  // Navigate on. The previous URL should *not* get synced, but this one
  // (currently open at the time Sync is turned on) will get synced when it
  // gets updated, which in practice happens on the next navigation, or when the
  // tab is closed.
  GURL synced_url1 =
      embedded_test_server()->GetURL("synced1.com", "/sync/simple.html");
  NavigateToURL(synced_url1);

  // Note: On Android, SetupSync(WAIT_FOR_COMMITS_TO_COMPLETE) (the default)
  // waits for an "about:blank" tab to show up in the Sessions data on the fake
  // server. Since this test already navigated away, that'll never happen. So
  // use the slightly-weaker WAIT_FOR_SYNC_SETUP_TO_COMPLETE here.
  ASSERT_TRUE(SetupSync(SyncTest::WAIT_FOR_SYNC_SETUP_TO_COMPLETE))
      << "SetupSync() failed.";

  // After Sync was enabled, navigate further.
  GURL synced_url2 =
      embedded_test_server()->GetURL("synced2.com", "/sync/simple.html");
  NavigateToURL(synced_url2);

  // The last two URLs (currently open while Sync was turned on, and
  // navigated-to after Sync was turned on, respectively) should have been
  // synced. The first URL (closed before Sync was turned on) should not have
  // been synced.
  EXPECT_TRUE(WaitForHistory(UnorderedElementsAre(UrlIs(synced_url1.spec()),
                                                  UrlIs(synced_url2.spec()))));
}

IN_PROC_BROWSER_TEST_F(SingleClientHistorySyncTest, UploadsAllFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Navigate to some URL, and make sure it shows up on the server.
  GURL url1 =
      embedded_test_server()->GetURL("www.host1.com", "/sync/simple.html");
  NavigateToURL(url1, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  EXPECT_TRUE(WaitForHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec())))));

  // Navigate to a second URL. This "completes" the first visit, which should
  // cause it to get updated with some details that are known only now, e.g.
  // the visit duration.
  // Note that currently, HistoryBackend depends on the presence of a referrer
  // to correctly populate the visit_duration (see crbug.com/1357013).
  GURL url2 =
      embedded_test_server()->GetURL("www.host2.com", "/sync/simple.html");
  NavigateToURL(url2, ui::PAGE_TRANSITION_LINK, /*referrer=*/url1);

  EXPECT_TRUE(WaitForHistory(UnorderedElementsAre(
      AllOf(StandardFieldsArePopulated(), UrlIs(url1.spec()),
            CoreTransitionIs(sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK),
            Not(HasReferringVisit()), HasVisitDuration()),
      AllOf(StandardFieldsArePopulated(), UrlIs(url2.spec()),
            CoreTransitionIs(sync_pb::SyncEnums_PageTransition_LINK),
            HasReferringVisit()))));
}

}  // namespace
