// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_

#include "base/types/strong_alias.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// A single speculation rule which permits some set of URLs to be speculated,
// subject to some conditions.
//
// https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule
class CORE_EXPORT SpeculationRule final
    : public GarbageCollected<SpeculationRule> {
 public:
  using RequiresAnonymousClientIPWhenCrossOrigin =
      base::StrongAlias<class RequiresAnonymousClientIPWhenCrossOriginTag,
                        bool>;

  SpeculationRule(
      Vector<KURL>,
      RequiresAnonymousClientIPWhenCrossOrigin,
      absl::optional<mojom::blink::SpeculationTargetHint> target_hint);
  ~SpeculationRule();

  const Vector<KURL>& urls() const { return urls_; }
  bool requires_anonymous_client_ip_when_cross_origin() const {
    return requires_anonymous_client_ip_.value();
  }
  absl::optional<mojom::blink::SpeculationTargetHint>
  target_browsing_context_name_hint() const {
    return target_browsing_context_name_hint_;
  }

  void Trace(Visitor*) const;

 private:
  const Vector<KURL> urls_;
  const RequiresAnonymousClientIPWhenCrossOrigin requires_anonymous_client_ip_;
  const absl::optional<mojom::blink::SpeculationTargetHint>
      target_browsing_context_name_hint_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_
