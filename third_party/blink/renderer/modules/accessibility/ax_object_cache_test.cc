// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// TODO(nektar): Break test up into multiple tests.
TEST_F(AccessibilityTest, IsARIAWidget) {
  String test_content =
      "<body>"
      "<span id=\"plain\">plain</span><br>"
      "<span id=\"button\" role=\"button\">button</span><br>"
      "<span id=\"button-parent\" "
      "role=\"button\"><span>button-parent</span></span><br>"
      "<span id=\"button-caps\" role=\"BUTTON\">button-caps</span><br>"
      "<span id=\"button-second\" role=\"another-role "
      "button\">button-second</span><br>"
      "<span id=\"aria-bogus\" aria-bogus=\"bogus\">aria-bogus</span><br>"
      "<span id=\"aria-selected\" aria-selected>aria-selected</span><br>"
      "<span id=\"haspopup\" "
      "aria-haspopup=\"true\">aria-haspopup-true</span><br>"
      "<div id=\"focusable\" tabindex=\"1\">focusable</div><br>"
      "<div tabindex=\"2\"><div "
      "id=\"focusable-parent\">focusable-parent</div></div><br>"
      "</body>";

  SetBodyInnerHTML(test_content);
  Element* root(GetDocument().documentElement());
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("plain")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-parent")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-caps")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("button-second")));
  EXPECT_FALSE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-bogus")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("aria-selected")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("haspopup")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable")));
  EXPECT_TRUE(AXObjectCache::IsInsideFocusableElementOrARIAWidget(
      *root->getElementById("focusable-parent")));
}

class MockAXObject : public AXObject {
 public:
  explicit MockAXObject(AXObjectCacheImpl& ax_object_cache)
      : AXObject(ax_object_cache) {}
  static unsigned num_children_changed_calls_;

  void ChildrenChangedWithCleanLayout() final { num_children_changed_calls_++; }
  Document* GetDocument() const final { return &AXObjectCache().GetDocument(); }
  void AddChildren() final {}
  ax::mojom::blink::Role NativeRoleIgnoringAria() const override {
    return ax::mojom::blink::Role::kUnknown;
  }
};

unsigned MockAXObject::num_children_changed_calls_ = 0;

TEST_F(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued) {
  auto& document = GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  DCHECK(ax_object_cache);

  wtf_size_t max_updates = 10;
  ax_object_cache->SetMaxPendingUpdatesForTesting(max_updates);

  MockAXObject* ax_obj = MakeGarbageCollected<MockAXObject>(*ax_object_cache);
  ax_object_cache->AssociateAXID(ax_obj);
  for (unsigned i = 0; i < max_updates + 1; i++) {
    ax_object_cache->DeferTreeUpdate(
        &AXObjectCacheImpl::ChildrenChangedWithCleanLayout, ax_obj);
  }
  document.Lifecycle().AdvanceTo(DocumentLifecycle::kInAccessibility);
  ax_object_cache->ProcessCleanLayoutCallbacks(document);

  ASSERT_EQ(0u, MockAXObject::num_children_changed_calls_);
}

class AXDocumentTransitionTest : public testing::Test,
                                 private ScopedDocumentTransitionForTest {
 public:
  AXDocumentTransitionTest() : ScopedDocumentTransitionForTest(true) {}

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize();
    web_view_helper_->Resize(gfx::Size(200, 200));
  }

  void TearDown() override { web_view_helper_.reset(); }

  Document& GetDocument() {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->GetDocument();
  }

  void UpdateAllLifecyclePhasesAndFinishDirectives() {
    UpdateAllLifecyclePhasesForTest();
    for (auto& callback :
         LayerTreeHost()->TakeDocumentTransitionCallbacksForTesting()) {
      std::move(callback).Run();
    }
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  void SetHtmlInnerHTML(const String& content) {
    GetDocument().body()->setInnerHTML(content);
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    web_view_helper_->GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  using State = DocumentTransition::State;

  State GetState(DocumentTransition* transition) const {
    return transition->state_;
  }

 protected:
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

TEST_F(AXDocumentTransitionTest, TransitionPseudoNotRelevant) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .shared {
        width: 100px;
        height: 100px;
        page-transition-tag: shared;
        contain: layout;
        background: green;
      }
    </style>
    <div id=target class=shared></div>
  )HTML");

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  UpdateAllLifecyclePhasesAndFinishDirectives();

  // We should have a transition pseudo
  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdPageTransition);
  ASSERT_TRUE(transition_pseudo);
  auto* container_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdPageTransitionContainer, "shared");
  ASSERT_TRUE(container_pseudo);
  auto* image_wrapper_pseudo = container_pseudo->GetPseudoElement(
      kPseudoIdPageTransitionImageWrapper, "shared");
  ASSERT_TRUE(image_wrapper_pseudo);
  auto* incoming_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdPageTransitionIncomingImage, "shared");
  ASSERT_TRUE(incoming_image_pseudo);
  auto* outgoing_image_pseudo = image_wrapper_pseudo->GetPseudoElement(
      kPseudoIdPageTransitionOutgoingImage, "shared");
  ASSERT_TRUE(outgoing_image_pseudo);

  ASSERT_TRUE(transition_pseudo->GetLayoutObject());
  ASSERT_TRUE(container_pseudo->GetLayoutObject());
  ASSERT_TRUE(image_wrapper_pseudo->GetLayoutObject());
  ASSERT_TRUE(incoming_image_pseudo->GetLayoutObject());
  ASSERT_TRUE(outgoing_image_pseudo->GetLayoutObject());

  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*transition_pseudo));
  EXPECT_FALSE(AXObjectCacheImpl::IsRelevantPseudoElement(*container_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*image_wrapper_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*incoming_image_pseudo));
  EXPECT_FALSE(
      AXObjectCacheImpl::IsRelevantPseudoElement(*outgoing_image_pseudo));
}

}  // namespace blink
