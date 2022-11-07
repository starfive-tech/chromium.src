// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"

#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSScrollTimelineTest : public PageTestBase,
                              private ScopedCSSScrollTimelineForTest {
 public:
  CSSScrollTimelineTest() : ScopedCSSScrollTimelineForTest(true) {}

  DocumentAnimations& GetDocumentAnimations() const {
    return GetDocument().GetDocumentAnimations();
  }
};

TEST_F(CSSScrollTimelineTest, SharedTimelines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim1 { to { top: 200px; } }
      @keyframes anim2 { to { left: 200px; } }
      @keyframes anim3 { to { right: 200px; } }
      .scroller {
        height: 100px;
        overflow: scroll;
      }
      .scroller > div {
        height: 200px;
      }
      #scroller1 {
        scroll-timeline: timeline1;
      }
      #scroller2 {
        scroll-timeline: timeline2;
      }
    </style>
    <div id=scroller1 class=scroller><div></div></div>
    <div id=scroller2 class=scroller><div></div></div>
    <main id=main></main>
  )HTML");
  // #scroller[1,2] etc is created in a separate lifecycle phase to ensure that
  // we get a layout box for #scroller[1,2] before the animations are started.

  Element* main = GetDocument().getElementById("main");
  ASSERT_TRUE(main);
  main->setInnerHTML(R"HTML(
    <style>
      #element1, #element2 {
        animation-name: anim1, anim2, anim3;
        animation-duration: 10s;
        animation-timeline: timeline1, timeline1, timeline2;
      }
    </style>
    <div id=element1></div>
    <div id=element2></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element1 = GetDocument().getElementById("element1");
  Element* element2 = GetDocument().getElementById("element2");
  ASSERT_TRUE(element1);
  ASSERT_TRUE(element2);
  HeapVector<Member<Animation>> animations1 = element1->getAnimations();
  HeapVector<Member<Animation>> animations2 = element2->getAnimations();
  EXPECT_EQ(3u, animations1.size());
  EXPECT_EQ(3u, animations2.size());

  // The animations associated with anim1 and anim2 should share the same
  // timeline instance, also across elements.
  EXPECT_EQ(animations1[0]->timeline(), animations1[1]->timeline());
  EXPECT_EQ(animations1[1]->timeline(), animations2[0]->timeline());
  EXPECT_EQ(animations2[0]->timeline(), animations2[1]->timeline());

  // The animation associated with anim3 uses a different timeline
  // from anim1/2.
  EXPECT_EQ(animations1[2]->timeline(), animations2[2]->timeline());

  EXPECT_NE(animations2[2]->timeline(), animations1[0]->timeline());
  EXPECT_NE(animations2[2]->timeline(), animations1[1]->timeline());
}

TEST_F(CSSScrollTimelineTest, MultipleLifecyclePasses) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { color: green; }
        to { color: green; }
      }
      #scroller {
        height: 100px;
        overflow: scroll;
        scroll-timeline: timeline;
      }
      #scroller > div {
        height: 200px;
      }
      #element {
        color: red;
        animation: anim 10s timeline;
      }
    </style>
    <div id=scroller>
      <div id=contents></div>
    </div>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById("element");
  ASSERT_TRUE(element);

  // According to the rules of the spec [1], the timeline is now inactive,
  // because #scroller did not have a layout box at the time style recalc
  // for #element happened.
  //
  // However, we do an additional style/layout pass if we detect new
  // CSSScrollTimelines in this situation, hence we ultimately do expect
  // the animation to apply [2].
  //
  // See also DocumentAnimations::ValidateTimelines.
  //
  // [1] https://drafts.csswg.org/scroll-animations-1/#avoiding-cycles
  // [2] https://github.com/w3c/csswg-drafts/issues/5261
  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            element->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

namespace {

class AnimationTriggeringDelegate : public ResizeObserver::Delegate {
 public:
  explicit AnimationTriggeringDelegate(Element* scroller_element)
      : scroller_element_(scroller_element) {}

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    scroller_element_->SetInlineStyleProperty(CSSPropertyID::kScrollTimeline,
                                              "timeline");
  }

  void Trace(Visitor* visitor) const override {
    ResizeObserver::Delegate::Trace(visitor);
    visitor->Trace(scroller_element_);
  }

 private:
  Member<Element> scroller_element_;
};

}  // namespace

TEST_F(CSSScrollTimelineTest, ResizeObserverTriggeredTimelines) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { width: 100px; }
        to { width: 100px; }
      }
      #scroller {
        height: 100px;
        overflow: scroll;
      }
      #scroller > div {
        height: 200px;
      }
      #element {
        width: 1px;
        animation: anim 10s timeline;
      }
    </style>
    <div id=main></div>
  )HTML");

  ASSERT_TRUE(
      GetDocumentAnimations().GetUnvalidatedTimelinesForTesting().IsEmpty());

  Element* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(blink::html_names::kIdAttr, "element");

  Element* scroller = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  scroller->setAttribute(blink::html_names::kIdAttr, "scroller");
  scroller->AppendChild(MakeGarbageCollected<HTMLDivElement>(GetDocument()));

  Element* main = GetDocument().getElementById("main");
  ASSERT_TRUE(main);
  main->AppendChild(scroller);
  main->AppendChild(element);

  auto* delegate = MakeGarbageCollected<AnimationTriggeringDelegate>(scroller);
  ResizeObserver* observer =
      ResizeObserver::Create(GetDocument().domWindow(), delegate);
  observer->observe(element);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u,
            GetDocumentAnimations().GetUnvalidatedTimelinesForTesting().size());
}

TEST_F(CSSScrollTimelineTest, DocumentScrollerInQuirksMode) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);

  SetBodyInnerHTML(R"HTML(
    <style>
    @keyframes anim {
      from { z-index: 100; }
      to { z-index: 100; }
    }
    #element {
      animation: anim 10s forwards scroll(root);
    }
    </style>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById("element");
  ASSERT_TRUE(element);

  EXPECT_EQ(100, element->GetComputedStyle()->ZIndex());
  // Don't crash.
}

}  // namespace blink
