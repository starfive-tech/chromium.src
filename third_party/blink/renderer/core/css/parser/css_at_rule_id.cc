// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSAtRuleID CssAtRuleID(StringView name) {
  if (EqualIgnoringASCIICase(name, "charset"))
    return CSSAtRuleID::kCSSAtRuleCharset;
  if (EqualIgnoringASCIICase(name, "font-face"))
    return CSSAtRuleID::kCSSAtRuleFontFace;
  if (EqualIgnoringASCIICase(name, "font-palette-values")) {
    if (RuntimeEnabledFeatures::FontPaletteEnabled())
      return CSSAtRuleID::kCSSAtRuleFontPaletteValues;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "import"))
    return CSSAtRuleID::kCSSAtRuleImport;
  if (EqualIgnoringASCIICase(name, "keyframes"))
    return CSSAtRuleID::kCSSAtRuleKeyframes;
  if (EqualIgnoringASCIICase(name, "layer")) {
    return CSSAtRuleID::kCSSAtRuleLayer;
  }
  if (EqualIgnoringASCIICase(name, "media"))
    return CSSAtRuleID::kCSSAtRuleMedia;
  if (EqualIgnoringASCIICase(name, "namespace"))
    return CSSAtRuleID::kCSSAtRuleNamespace;
  if (EqualIgnoringASCIICase(name, "page"))
    return CSSAtRuleID::kCSSAtRulePage;
  if (EqualIgnoringASCIICase(name, "position-fallback")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled())
      return CSSAtRuleID::kCSSAtRulePositionFallback;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "property"))
    return CSSAtRuleID::kCSSAtRuleProperty;
  if (EqualIgnoringASCIICase(name, "container")) {
    if (RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
      return CSSAtRuleID::kCSSAtRuleContainer;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "counter-style"))
    return CSSAtRuleID::kCSSAtRuleCounterStyle;
  if (EqualIgnoringASCIICase(name, "scroll-timeline")) {
    if (RuntimeEnabledFeatures::CSSScrollTimelineEnabled())
      return CSSAtRuleID::kCSSAtRuleScrollTimeline;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "scope")) {
    if (RuntimeEnabledFeatures::CSSScopeEnabled())
      return CSSAtRuleID::kCSSAtRuleScope;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "supports"))
    return CSSAtRuleID::kCSSAtRuleSupports;
  if (EqualIgnoringASCIICase(name, "try")) {
    if (RuntimeEnabledFeatures::CSSAnchorPositioningEnabled())
      return CSSAtRuleID::kCSSAtRuleTry;
    return CSSAtRuleID::kCSSAtRuleInvalid;
  }
  if (EqualIgnoringASCIICase(name, "viewport"))
    return CSSAtRuleID::kCSSAtRuleViewport;
  if (EqualIgnoringASCIICase(name, "-webkit-keyframes"))
    return CSSAtRuleID::kCSSAtRuleWebkitKeyframes;
  return CSSAtRuleID::kCSSAtRuleInvalid;
}

namespace {

absl::optional<WebFeature> AtRuleFeature(CSSAtRuleID rule_id) {
  switch (rule_id) {
    case CSSAtRuleID::kCSSAtRuleCharset:
      return WebFeature::kCSSAtRuleCharset;
    case CSSAtRuleID::kCSSAtRuleFontFace:
      return WebFeature::kCSSAtRuleFontFace;
    case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
      return WebFeature::kCSSAtRuleFontPaletteValues;
    case CSSAtRuleID::kCSSAtRuleImport:
      return WebFeature::kCSSAtRuleImport;
    case CSSAtRuleID::kCSSAtRuleKeyframes:
      return WebFeature::kCSSAtRuleKeyframes;
    case CSSAtRuleID::kCSSAtRuleLayer:
      return WebFeature::kCSSCascadeLayers;
    case CSSAtRuleID::kCSSAtRuleMedia:
      return WebFeature::kCSSAtRuleMedia;
    case CSSAtRuleID::kCSSAtRuleNamespace:
      return WebFeature::kCSSAtRuleNamespace;
    case CSSAtRuleID::kCSSAtRulePage:
      return WebFeature::kCSSAtRulePage;
    case CSSAtRuleID::kCSSAtRuleProperty:
      return WebFeature::kCSSAtRuleProperty;
    case CSSAtRuleID::kCSSAtRuleContainer:
      return WebFeature::kCSSAtRuleContainer;
    case CSSAtRuleID::kCSSAtRuleCounterStyle:
      return WebFeature::kCSSAtRuleCounterStyle;
    case CSSAtRuleID::kCSSAtRuleScope:
      return WebFeature::kCSSAtRuleScope;
    case CSSAtRuleID::kCSSAtRuleScrollTimeline:
      return WebFeature::kCSSAtRuleScrollTimeline;
    case CSSAtRuleID::kCSSAtRuleSupports:
      return WebFeature::kCSSAtRuleSupports;
    case CSSAtRuleID::kCSSAtRuleViewport:
      return WebFeature::kCSSAtRuleViewport;
    case CSSAtRuleID::kCSSAtRulePositionFallback:
    case CSSAtRuleID::kCSSAtRuleTry:
      // TODO(crbug.com/1309178): Add use counter.
      return absl::nullopt;
    case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
      return WebFeature::kCSSAtRuleWebkitKeyframes;
    case CSSAtRuleID::kCSSAtRuleInvalid:
      NOTREACHED();
      return absl::nullopt;
  }
}

}  // namespace

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  if (absl::optional<WebFeature> feature = AtRuleFeature(rule_id))
    context->Count(*feature);
}

}  // namespace blink
