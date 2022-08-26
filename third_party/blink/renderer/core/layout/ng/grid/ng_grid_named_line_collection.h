// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NAMED_LINE_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NAMED_LINE_COLLECTION_H_

#include "third_party/blink/renderer/core/style/grid_enums.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ComputedStyle;

class NGGridNamedLineCollection {
 public:
  NGGridNamedLineCollection(const ComputedStyle&,
                            const String& named_line,
                            GridTrackSizingDirection,
                            wtf_size_t last_line,
                            wtf_size_t auto_repeat_tracks_count,
                            bool is_parent_grid_container = false);

  NGGridNamedLineCollection(const NGGridNamedLineCollection&) = delete;
  NGGridNamedLineCollection& operator=(const NGGridNamedLineCollection&) =
      delete;

  bool HasNamedLines();
  wtf_size_t FirstPosition();

  bool Contains(wtf_size_t line);

 private:
  bool HasExplicitNamedLines();
  wtf_size_t FirstExplicitPosition();
  const Vector<wtf_size_t>* named_lines_indexes_ = nullptr;
  const Vector<wtf_size_t>* auto_repeat_named_lines_indexes_ = nullptr;
  const Vector<wtf_size_t>* implicit_named_lines_indexes_ = nullptr;

  bool is_standalone_grid_;
  wtf_size_t insertion_point_;
  wtf_size_t last_line_;
  wtf_size_t auto_repeat_total_tracks_;
  wtf_size_t auto_repeat_track_list_length_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_NAMED_LINE_COLLECTION_H_
