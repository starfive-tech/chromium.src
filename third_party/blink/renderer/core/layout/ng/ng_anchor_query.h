// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_

#include "third_party/blink/renderer/core/core_export.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/anchor_query_enums.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class NGLogicalAnchorQuery;
class NGPhysicalFragment;
class WritingModeConverter;
struct NGLogicalAnchorReference;
struct NGLogicalLink;

struct CORE_EXPORT NGPhysicalAnchorReference
    : public GarbageCollected<NGPhysicalAnchorReference> {
  NGPhysicalAnchorReference(const NGLogicalAnchorReference& logical_reference,
                            const WritingModeConverter& converter);

  void Trace(Visitor* visitor) const;

  PhysicalRect rect;
  Member<const NGPhysicalFragment> fragment;
  bool is_invalid = false;
};

class CORE_EXPORT NGPhysicalAnchorQuery {
  DISALLOW_NEW();

 public:
  bool IsEmpty() const { return anchor_references_.IsEmpty(); }

  const NGPhysicalAnchorReference* AnchorReference(
      const AtomicString& name) const;
  const PhysicalRect* Rect(const AtomicString& name) const;
  const NGPhysicalFragment* Fragment(const AtomicString& name) const;

  using NGPhysicalAnchorReferenceMap =
      HeapHashMap<AtomicString, Member<NGPhysicalAnchorReference>>;
  NGPhysicalAnchorReferenceMap::const_iterator begin() const {
    return anchor_references_.begin();
  }
  NGPhysicalAnchorReferenceMap::const_iterator end() const {
    return anchor_references_.end();
  }

  void SetFromLogical(const NGLogicalAnchorQuery& logical_query,
                      const WritingModeConverter& converter);

  void Trace(Visitor* visitor) const;

 private:
  friend class NGLogicalAnchorQuery;

  NGPhysicalAnchorReferenceMap anchor_references_;
};

struct CORE_EXPORT NGLogicalAnchorReference
    : public GarbageCollected<NGLogicalAnchorReference> {
  NGLogicalAnchorReference(const NGPhysicalFragment& fragment,
                           const LogicalRect& rect,
                           bool is_invalid)
      : rect(rect), fragment(&fragment), is_invalid(is_invalid) {}

  void Trace(Visitor* visitor) const;

  LogicalRect rect;
  Member<const NGPhysicalFragment> fragment;
  bool is_invalid = false;
};

class CORE_EXPORT NGLogicalAnchorQuery {
  STACK_ALLOCATED();

 public:
  bool IsEmpty() const { return anchor_references_.IsEmpty(); }

  const NGLogicalAnchorReference* AnchorReference(
      const AtomicString& name) const;
  const LogicalRect* Rect(const AtomicString& name) const;
  const NGPhysicalFragment* Fragment(const AtomicString& name) const;

  void Set(const AtomicString& name,
           const NGPhysicalFragment& fragment,
           const LogicalRect& rect);
  void SetFromPhysical(const NGPhysicalAnchorQuery& physical_query,
                       const WritingModeConverter& converter,
                       const LogicalOffset& additional_offset,
                       bool is_positioned);
  void SetAsStitched(base::span<const NGLogicalLink> children,
                     WritingDirectionMode writing_direction);

  // Evaluate the |anchor_name| for the |anchor_value|. Returns |nullopt| if
  // the query is invalid (e.g., no targets or wrong axis.)
  absl::optional<LayoutUnit> EvaluateAnchor(
      const AtomicString& anchor_name,
      AnchorValue anchor_value,
      LayoutUnit available_size,
      const WritingModeConverter& container_converter,
      const PhysicalOffset& offset_to_padding_box,
      bool is_y_axis,
      bool is_right_or_bottom) const;
  absl::optional<LayoutUnit> EvaluateSize(const AtomicString& anchor_name,
                                          AnchorSizeValue anchor_size_value,
                                          WritingMode container_writing_mode,
                                          WritingMode self_writing_mode) const;

 private:
  friend class NGPhysicalAnchorQuery;

  void Set(const AtomicString& name, NGLogicalAnchorReference* reference);

  HeapHashMap<AtomicString, Member<NGLogicalAnchorReference>>
      anchor_references_;
};

class CORE_EXPORT NGAnchorEvaluatorImpl : public Length::AnchorEvaluator {
  STACK_ALLOCATED();

 public:
  NGAnchorEvaluatorImpl(const NGLogicalAnchorQuery& anchor_query,
                        const WritingModeConverter& container_converter,
                        const PhysicalOffset& offset_to_padding_box,
                        WritingMode self_writing_mode)
      : anchor_query_(anchor_query),
        container_converter_(container_converter),
        offset_to_padding_box_(offset_to_padding_box),
        self_writing_mode_(self_writing_mode) {}

  // Returns true if this evaluator was invoked for `anchor()` or
  // `anchor-size()` functions.
  bool HasAnchorFunctions() const { return has_anchor_functions_; }

  // This must be set before evaluating `anchor()` function.
  void SetAxis(bool is_y_axis,
               bool is_right_or_bottom,
               LayoutUnit available_size) {
    available_size_ = available_size;
    is_y_axis_ = is_y_axis;
    is_right_or_bottom_ = is_right_or_bottom;
  }

  absl::optional<LayoutUnit> EvaluateAnchor(
      const AtomicString& anchor_name,
      AnchorValue anchor_value) const override;
  absl::optional<LayoutUnit> EvaluateAnchorSize(
      const AtomicString& anchor_name,
      AnchorSizeValue anchor_size_value) const override;

 private:
  const NGLogicalAnchorQuery& anchor_query_;
  const WritingModeConverter& container_converter_;
  PhysicalOffset offset_to_padding_box_;
  WritingMode self_writing_mode_;
  LayoutUnit available_size_;
  bool is_y_axis_ = false;
  bool is_right_or_bottom_ = false;
  mutable bool has_anchor_functions_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ANCHOR_QUERY_H_
