// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/webui_interaction_test_util.h"

#include <sstream>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIInteractionTestUtilTestId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIInteractionTestUtilTestId2);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInteractionTestUtilCustomEventType2);

constexpr char kEmptyDocumentURL[] = "/empty.html";
constexpr char kDocumentWithTitle1URL[] = "/title1.html";
constexpr char kDocumentWithTitle2URL[] = "/title2.html";
constexpr char kDocumentWithLinksURL[] = "/links.html";

}  // namespace

class WebUIInteractionTestUtilTest : public InProcessBrowserTest {
 public:
  WebUIInteractionTestUtilTest() = default;
  ~WebUIInteractionTestUtilTest() override = default;
  WebUIInteractionTestUtilTest(const WebUIInteractionTestUtilTest&) = delete;
  void operator=(const WebUIInteractionTestUtilTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, GetBrowserFromContext) {
  Browser* const other_browser = CreateBrowser(browser()->profile());
  EXPECT_EQ(browser(), WebUIInteractionTestUtil::GetBrowserFromContext(
                           browser()->window()->GetElementContext()));
  EXPECT_EQ(other_browser, WebUIInteractionTestUtil::GetBrowserFromContext(
                               other_browser->window()->GetElementContext()));
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementCreatedForExistingPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Using this constructor hits all of the rest of the constructors, saving us
  // the hassle of writing three identical tests.
  auto util = WebUIInteractionTestUtil::ForExistingTabInContext(
      browser()->window()->GetElementContext(),
      kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(element->IsA<TrackedElementWebPage>());
                        EXPECT_EQ(
                            util.get(),
                            element->AsA<TrackedElementWebPage>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementCreatedForExistingWebContentsWithoutWebView) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForTabWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(element->IsA<TrackedElementWebPage>());
                        EXPECT_EQ(
                            util.get(),
                            element->AsA<TrackedElementWebPage>()->owner());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementRecreatedOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, LoadPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPage(url);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  DCHECK_EQ(url, util->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, IsPageLoaded) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->is_page_loaded());
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_FALSE(util->is_page_loaded());
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->is_page_loaded());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementRecreatedWithDifferentIdOnNavigate) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->set_page_identifier(
                                 kWebUIInteractionTestUtilTestId2);
                             NavigateParams navigate_params(
                                 browser(), url, ui::PAGE_TRANSITION_TYPED);
                             Navigate(&navigate_params);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementRecreatedWithDifferentIdOnBackForward) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Do two navigations, then go back, then forward again.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);
  const GURL url2 = embedded_test_server()->GetURL(kDocumentWithTitle2URL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);

  // Load the first page and make sure we wait for the page transition.
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             // Load the second page and wait for it to finish
                             // loading.
                             util->set_page_identifier(
                                 kWebUIInteractionTestUtilTestId2);
                             util->LoadPage(url2);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoBack(browser()));
                             util->set_page_identifier(
                                 kWebUIInteractionTestUtilTestId);
                             chrome::GoBack(browser(),
                                            WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url, util->web_contents()->GetURL());
                             EXPECT_TRUE(chrome::CanGoForward(browser()));
                             util->set_page_identifier(
                                 kWebUIInteractionTestUtilTestId2);
                             chrome::GoForward(
                                 browser(), WindowOpenDisposition::CURRENT_TAB);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(url2, util->web_contents()->GetURL());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, EvaluateInt) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(1, util->Evaluate("() => 1").GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, EvaluateString) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(
                                 std::string("The quick brown fox"),
                                 util->Evaluate("() => 'The quick brown fox'")
                                     .GetString());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, EvaluatePromise) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  constexpr char kPromiseScript[] =
      "() => new Promise((resolve) => setTimeout(resolve(123), 300))";
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_EQ(123,
                                       util->Evaluate(kPromiseScript).GetInt());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnStateChangeOnCurrentCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate("function() { window.value = 1; }");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnStateChangeOnDelayedCondition) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      window.value = 0;
                                      setTimeout(
                                        function() { window.value = 1; },
                                        300);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       StateChangeTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      window.value = 0;
                                      setTimeout(
                                        function() { window.value = 1; },
                                        1000);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.test_function = "() => window.value";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, StateChangeOnPromise) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  constexpr base::TimeDelta kPollTime = base::Milliseconds(50);
  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.test_function = R"(() => {
                                 return new Promise(r => {
                                       setTimeout(() => r(1), 100);
                                     });
                                 })";
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.polling_interval = kPollTime;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendStateChangeEventsForDifferentDataTypes) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);

  // Poll significantly faster than the value in the page is expected to
  // change; this allows us to verify that the value changes after a non-zero
  // amount of time.
  constexpr base::TimeDelta kPollTime = base::Milliseconds(50);
  constexpr int kScriptDelayMs = 150;

  base::ElapsedTimer timer;
  base::TimeDelta last;

  // Sets window.value to an initial value, and then some time later, sets it
  // to a final value.
  const auto post_and_listen = [&](base::Value initial, base::Value final) {
    std::string script = content::JsReplace(
        R"(function() {
             window.value = $1;
             setTimeout(function() { window.value = $2; }, $3);
           })",
        std::move(initial), std::move(final), kScriptDelayMs);
    util->Evaluate(script);
    WebUIInteractionTestUtil::StateChange state_change;
    state_change.test_function = "() => window.value";
    state_change.event = kInteractionTestUtilCustomEventType;
    state_change.polling_interval = kPollTime;
    util->SendEventOnStateChange(state_change);
  };

  // Verifies that multiple polling intervals have passed before the condition
  // we were watching becomes true.
  const auto check_elapsed = [&]() {
    auto next = timer.Elapsed();
    EXPECT_GT(next, last + 2 * kPollTime);
    last = next;
  };

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             last = timer.Elapsed();
                             // Integers:
                             post_and_listen(base::Value(0), base::Value(1));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Booleans:
                             post_and_listen(base::Value(false),
                                             base::Value(true));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Strings:
                             post_and_listen(base::Value(""),
                                             base::Value("foo"));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             // Doubles:
                             post_and_listen(base::Value(0.0),
                                             base::Value(6.1));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             base::Value::List list;
                             list.Append(false);
                             post_and_listen(base::Value(),
                                             base::Value(std::move(list)));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             check_elapsed();
                             base::Value::Dict dict;
                             dict.Set("foo", "bar");
                             post_and_listen(base::Value(),
                                             base::Value(std::move(dict)));
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) { check_elapsed(); }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnStateChangeOnAlreadyExists) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"a#title1"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnStateChangeOnExistsAfterDelay) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('ul');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                        },
                                        300);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       StateChangeExistsTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"ul#foo"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('ul');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                        },
                                        1000);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExists;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnStateChangeOnAlreadyExistsAndConditionTrue) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"a#title1"};
  const char kTestCondition[] = "el => (el.innerText == 'Go to title1')";

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExistsAndConditionTrue;
                             state_change.test_function = kTestCondition;
                             state_change.where = kQuery;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(
                            util->EvaluateAt(kQuery, kTestCondition).GetBool());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(
    WebUIInteractionTestUtilTest,
    SendEventOnStateChangeOnExistsAndConditionTrueAfterDelay) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"h1#foo"};
  const char kTestCondition[] = "el => (el.innerText == 'bar')";

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      setTimeout(
                                        function() {
                                          let el = document.createElement('h1');
                                          el.id = 'foo';
                                          document.body.appendChild(el);
                                          setTimeout(
                                            function() {
                                              let el = document.querySelector(
                                                  'h1#foo');
                                              el.innerText = 'bar';
                                            },
                                            100);
                                        },
                                        300);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExistsAndConditionTrue;
                             state_change.where = kQuery;
                             state_change.test_function = kTestCondition;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(state_change);
                             EXPECT_FALSE(util->Exists(kQuery));
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                           kInteractionTestUtilCustomEventType)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_TRUE(
                            util->EvaluateAt(kQuery, kTestCondition).GetBool());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       StateChangeExistsAndConditionTrueTimeoutSendsEvent) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"h1#foo"};
  const char kTestCondition[] = "el => (el.innerText == 'bar')";

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(
                                 R"(function () {
                                      let el = document.createElement('h1');
                                      el.id = 'foo';
                                      document.body.appendChild(el);
                                      setTimeout(
                                          function() {
                                            let el = document.querySelector(
                                                'h1#foo');
                                            el.innerText = 'bar';
                                          },
                                          1000);
                                    })");
                             WebUIInteractionTestUtil::StateChange state_change;
                             state_change.type = WebUIInteractionTestUtil::
                                 StateChange::Type::kExistsAndConditionTrue;
                             state_change.where = kQuery;
                             state_change.test_function = kTestCondition;
                             state_change.event =
                                 kInteractionTestUtilCustomEventType;
                             state_change.timeout = base::Milliseconds(300);
                             state_change.timeout_event =
                                 kInteractionTestUtilCustomEventType2;
                             util->SendEventOnStateChange(state_change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType2)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ExecuteCanChangePageState) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        // This is an artificial value that is not
                        // initially true.
                        const char kCheckFunction[] = "() => !!window.value";
                        EXPECT_FALSE(util->Evaluate(kCheckFunction).GetBool());

                        // Prepare to send an event when the condition
                        // becomes true.
                        WebUIInteractionTestUtil::StateChange state_change;
                        state_change.test_function = kCheckFunction;
                        state_change.event =
                            kInteractionTestUtilCustomEventType;
                        util->SendEventOnStateChange(state_change);

                        // Immediately set a truthy value.
                        util->Execute("() => { window.value = 1; }");
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ExecuteAtCanChangePageState) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);
  const WebUIInteractionTestUtil::DeepQuery kQuery = {"a#title1"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        // This is an artificial value that is not
                        // initially true.
                        const char kCheckFunction[] =
                            "el => (el.innerText === 'abcde')";

                        // Verify that the check function is false.
                        EXPECT_FALSE(
                            util->EvaluateAt(kQuery, kCheckFunction).GetBool());

                        // Set up a condition check for a text string that
                        // doesn't exist in the document.
                        WebUIInteractionTestUtil::StateChange state_change;
                        state_change.type = WebUIInteractionTestUtil::
                            StateChange::Type::kConditionTrue;
                        state_change.where = kQuery;
                        state_change.test_function = kCheckFunction;
                        state_change.event =
                            kInteractionTestUtilCustomEventType;
                        util->SendEventOnStateChange(state_change);

                        // Set the expected text using ExecuteAt().
                        // The check function should become true.
                        util->ExecuteAt(kQuery,
                                        "el => { el.innerText = 'abcde'; }");
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       NavigatePageFromScriptCreatesNewElement) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->Evaluate(content::JsReplace(
                                 "function() { window.location = $1; }",
                                 url.spec()));
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementRemovedOnMoveToNewBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        chrome::MoveTabsToExistingWindow(
                            browser(), other_browser,
                            {browser()->tab_strip_model()->active_index()});
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_THAT(
      ui::ElementTracker::GetElementTracker()
          ->GetAllMatchingElementsInAnyContext(kWebUIInteractionTestUtilTestId),
      testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ElementRemovedOnPageClosed) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             browser()->tab_strip_model()->CloseSelectedTabs();
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, OpenPageInNewTabInactive) {
  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, false);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index, model->active_index());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, OpenPageInNewTabActive) {
  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto* const model = browser()->tab_strip_model();
  const int count = model->GetTabCount();
  const int index = model->active_index();
  util->LoadPageInNewTab(url, true);
  EXPECT_EQ(count + 1, model->GetTabCount());
  EXPECT_EQ(index + 1, model->active_index());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, ForNextTabInContext) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInContext(
      browser()->window()->GetElementContext(),
      kWebUIInteractionTestUtilTestId2);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, ForNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);
  Browser* const browser2 = CreateBrowser(browser()->profile());

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInBrowser(
      browser2, kWebUIInteractionTestUtilTestId2);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util2->LoadPageInNewTab(url, true);
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());

  // Wait for the element in the other browser to appear.
  // TODO(dfried): when we support cross-context sequences, these can be
  // combined.

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback,
                         completed2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted2);

  auto sequence2 =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .SetContext(browser2->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ForNextTabInAnyBrowserFreshBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);
  Browser* browser2 = nullptr;

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebUIInteractionTestUtilTestId2);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Open a completely new browser, we'll detect it
                             // opened and capture its first tab.
                             browser2 = CreateBrowser(browser()->profile());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());

  // Wait for the element in the other browser to appear.
  // TODO(dfried): when we support cross-context sequences, these can be
  // combined.

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback,
                         completed2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted2);

  auto sequence2 =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed2.Get())
          .SetAbortedCallback(aborted2.Get())
          .SetContext(browser2->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed2, Run,
                       sequence2->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       ForNextTabInAnyBrowserSameBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kEmptyDocumentURL);

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebUIInteractionTestUtilTestId2);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->LoadPageInNewTab(url, false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId2)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  EXPECT_EQ(url, util2->web_contents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       MovePageToNewBrowserTriggersTabInAnyBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInAnyBrowser(
      kWebUIInteractionTestUtilTestId2);

  auto get_element2 = [&]() {
    const auto result = ui::ElementTracker::GetElementTracker()
                            ->GetAllMatchingElementsInAnyContext(
                                kWebUIInteractionTestUtilTestId2);
    return result.empty() ? nullptr : result.front();
  };

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(nullptr, get_element2());
                        chrome::MoveTabsToExistingWindow(
                            browser(), other_browser,
                            {browser()->tab_strip_model()->active_index()});
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       MovePageToNewBrowserTriggersNextTabInBrowser) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  Browser* const other_browser = CreateBrowser(browser()->profile());

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  auto util2 = WebUIInteractionTestUtil::ForNextTabInBrowser(
      other_browser, kWebUIInteractionTestUtilTestId2);

  auto get_element2 = [&]() {
    const auto result = ui::ElementTracker::GetElementTracker()
                            ->GetAllMatchingElementsInAnyContext(
                                kWebUIInteractionTestUtilTestId2);
    return result.empty() ? nullptr : result.front();
  };

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kWebUIInteractionTestUtilTestId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(nullptr, get_element2());
                        chrome::MoveTabsToExistingWindow(
                            browser(), other_browser,
                            {browser()->tab_strip_model()->active_index()});
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
  auto* const element = get_element2();
  EXPECT_NE(nullptr, element);
  EXPECT_EQ(other_browser->window()->GetElementContext(), element->context());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, ExistsInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery1{
      "settings-ui", "settings-main#main", "div#noSearchResults"};
  const WebUIInteractionTestUtil::DeepQuery kQuery2{
      "settings-ui", "settings-main#foo", "div#noSearchResults"};

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(util->Exists(kQuery1));
                             std::string failed;
                             EXPECT_FALSE(util->Exists(kQuery2, &failed));
                             EXPECT_EQ(kQuery2[1], failed);
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, EvaluateAtInWebUIPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery{
      "settings-ui", "settings-main#main", "div#noSearchResults"};

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             const auto result =
                                 util->EvaluateAt(kQuery, "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_FALSE(result.GetString().empty());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       EvaluateAtNotExistElement) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery{
      "settings-ui", "settings-main#main", "not-exists-element"};

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  util->LoadPage(GURL("chrome://settings"));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             const auto result =
                                 util->EvaluateAt(kQuery, "(el, err) => !!el");
                             EXPECT_TRUE(result.is_bool());
                             EXPECT_FALSE(result.GetBool());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, ExistsInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery1{"#ref"};
  const WebUIInteractionTestUtil::DeepQuery kQuery2{"#not-present"};

  // These queries check that we can properly escape quotes:
  const WebUIInteractionTestUtil::DeepQuery kQuery3{"[id=\"ref\"]"};
  const WebUIInteractionTestUtil::DeepQuery kQuery4{"[id='ref']"};

  // These queries check that we can return strings with quotes on failure:
  const WebUIInteractionTestUtil::DeepQuery kQuery5{"[id=\"not-present\"]"};
  const WebUIInteractionTestUtil::DeepQuery kQuery6{"[id='not-present']"};

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Using DeepQuery.
                             EXPECT_TRUE(util->Exists(kQuery1));
                             std::string failed;
                             EXPECT_FALSE(util->Exists(kQuery2, &failed));
                             EXPECT_EQ(kQuery2[0], failed);
                             EXPECT_TRUE(util->Exists(kQuery3));
                             EXPECT_TRUE(util->Exists(kQuery4));
                             EXPECT_FALSE(util->Exists(kQuery5, &failed));
                             EXPECT_EQ(kQuery5[0], failed);
                             EXPECT_FALSE(util->Exists(kQuery6, &failed));
                             EXPECT_EQ(kQuery6[0], failed);

                             // Using the simple string selector version.
                             EXPECT_TRUE(util->Exists(kQuery1[0]));
                             EXPECT_FALSE(util->Exists(kQuery2[0]));
                             EXPECT_TRUE(util->Exists(kQuery3[0]));
                             EXPECT_TRUE(util->Exists(kQuery4[0]));
                             EXPECT_FALSE(util->Exists(kQuery5[0]));
                             EXPECT_FALSE(util->Exists(kQuery6[0]));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, EvaluateAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery{"#ref"};
  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Test evaluate at with a DeepQuery.
                             auto result =
                                 util->EvaluateAt(kQuery, "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_EQ("ref link", result.GetString());

                             // Test evaluate at with a plain string selector.
                             result = util->EvaluateAt(kQuery[0],
                                                       "el => el.innerText");
                             EXPECT_TRUE(result.is_string());
                             EXPECT_EQ("ref link", result.GetString());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnConditionStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery{"#ref"};
  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->EvaluateAt(kQuery,
                                              R"(el => {
                                      el.innerText = '';
                                      setTimeout(() => el.innerText = 'foo',
                                                 300);
                                    })");
                             WebUIInteractionTestUtil::StateChange change;
                             change.test_function = "el => el.innerText";
                             change.where = kQuery;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       SendEventOnExistsStateChangeAtInStandardPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const WebUIInteractionTestUtil::DeepQuery kQuery1{"#ref"};
  const WebUIInteractionTestUtil::DeepQuery kQuery2{"#ref", "p#pp"};
  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithLinksURL);
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             util->EvaluateAt(kQuery1,
                                              R"(el => {
                                                el.innerText = '';
                                                setTimeout(() =>
                                                    el.innerHTML =
                                                        '<p id="pp">foo</p>',
                                                 300);
                                                })");
                             WebUIInteractionTestUtil::StateChange change;
                             change.where = kQuery2;
                             change.event = kInteractionTestUtilCustomEventType;
                             util->SendEventOnStateChange(change);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionTestUtilCustomEventType)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest, CompareScreenshot_View) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kAppMenuButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 WebUIInteractionTestUtil::CompareScreenshot(
                                     element, "AppMenuButton", "3600270"));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       CompareScreenshot_WebPage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // Set the browser view to a consistent size.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetWidget()->SetSize({400, 300});

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebUIInteractionTestUtilTestId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);
  util->LoadPage(url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kWebUIInteractionTestUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             EXPECT_TRUE(
                                 WebUIInteractionTestUtil::CompareScreenshot(
                                     element, std::string(), "3600270"));
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This is a regression test for the case where we open a new tab in a way that
// causes it not to have a URL; previously, it would not create an element
// because navigating_away_from_ was empty.
IN_PROC_BROWSER_TEST_F(WebUIInteractionTestUtilTest,
                       CreatesElementForPageWithBlankURL) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kExistingTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabElementId);

  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<WebUIInteractionTestUtil> existing_tab =
      WebUIInteractionTestUtil::ForExistingTabInBrowser(browser(),
                                                        kExistingTabElementId);
  std::unique_ptr<WebUIInteractionTestUtil> new_tab;

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetContext(browser()->window()->GetElementContext())
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          // Get the first tab and inject code to pop up a second window.
          // Because the second window is created using a javascript: URL, it
          // will not report a valid URL.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kExistingTabElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*, ui::TrackedElement*) {
                        new_tab = WebUIInteractionTestUtil::ForNextTabInBrowser(
                            browser(), kNewTabElementId);
                        // Cause a tab to come into being and do some stuff.
                        existing_tab->Evaluate(
                            "() => window.open('javascript:window.foo=1')");
                      }))
                  .Build())
          // Verify that the element for the second tab is still created,
          // despite it not having a URL.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kNewTabElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
