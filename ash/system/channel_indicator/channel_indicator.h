// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_

#include <string>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/memory/weak_ptr.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"

namespace ash {

// A view that resides in the system tray, to make it obvious to the user when a
// device is running on a release track other than "stable."
class ASH_EXPORT ChannelIndicatorView : public TrayItemView,
                                        public SessionObserver,
                                        public ShellObserver {
 public:
  ChannelIndicatorView(Shelf* shelf, version_info::Channel channel);
  ChannelIndicatorView(const ChannelIndicatorView&) = delete;
  ChannelIndicatorView& operator=(const ChannelIndicatorView&) = delete;

  ~ChannelIndicatorView() override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // Introspection methods for testing.
  bool IsLabelVisibleForTesting();
  bool IsImageViewVisibleForTesting();

  // Returns the accessibility name.
  std::u16string GetAccessibleNameString() const;

 private:
  void Update();
  void SetImageOrText();
  void SetAccessibleName();
  void SetTooltip();

  // The localized string used to announce this view in accessibility mode.
  std::u16string accessible_name_;

  // The localized string displayed when this view is hovered-over.
  std::u16string tooltip_;

  // The release track on which this devices resides.
  const version_info::Channel channel_;

  ScopedSessionObserver session_observer_;

  base::ScopedObservation<Shell,
                          ShellObserver,
                          &Shell::AddShellObserver,
                          &Shell::RemoveShellObserver>
      shell_observer_{this};

  base::WeakPtrFactory<ChannelIndicatorView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
