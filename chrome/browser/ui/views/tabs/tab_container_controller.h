// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/range/range.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

// Model/Controller for the TabContainer.
// NOTE: All indices used by this class are in model coordinates.
class TabContainerController {
 public:
  virtual ~TabContainerController() = default;

  // Returns true if |index| is a valid model index.
  virtual bool IsValidModelIndex(int index) const = 0;

  // Returns the index of the active tab.
  virtual int GetActiveIndex() const = 0;

  // Notifies controller of a drop index update.
  virtual void OnDropIndexUpdate(int index, bool drop_before) = 0;

  // Returns the |group| collapsed state. Returns false if the group does not
  // exist or is not collapsed.
  // NOTE: This method signature is duplicated in TabStripController; the
  // methods are intended to have equivalent semantics so they can share an
  // implementation.
  virtual bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const = 0;

  // Gets the first tab index in |group|, or nullopt if the group is
  // currently empty. This is always safe to call unlike
  // ListTabsInGroup().
  virtual absl::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the range of tabs in the given |group|. This must not be
  // called during intermediate states where the group is not
  // contiguous. For example, if tabs elsewhere in the tab strip are
  // being moved into |group| it may not be contiguous; this method
  // cannot be called.
  virtual gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const = 0;

  // Whether the window drag handle area can be extended to include the top of
  // inactive tabs.
  virtual bool CanExtendDragHandle() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_CONTROLLER_H_
