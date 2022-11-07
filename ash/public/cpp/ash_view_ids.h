// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_
#define ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_

namespace ash {

enum ViewID {
  VIEW_ID_NONE = 0,

  // Ash IDs start above the range used in Chrome (c/b/ui/view_ids.h).
  VIEW_ID_ASH_START = 10000,

  // Row for the virtual keyboard feature in accessibility detailed view.
  VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD,
  // Icon that indicates the virtual keyboard is enabled.
  VIEW_ID_ACCESSIBILITY_VIRTUAL_KEYBOARD_ENABLED,
  // Accessibility feature pod button in main view.
  VIEW_ID_ACCESSIBILITY_TRAY_ITEM,
  // System tray AddUserButton in UserChooserView.
  VIEW_ID_ADD_USER_BUTTON,
  VIEW_ID_BLUETOOTH_DEFAULT_VIEW,
  // System tray casting row elements.
  VIEW_ID_CAST_CAST_VIEW,
  VIEW_ID_CAST_CAST_VIEW_LABEL,
  VIEW_ID_CAST_MAIN_VIEW,
  VIEW_ID_CAST_SELECT_VIEW,
  VIEW_ID_MEDIA_TRAY_VIEW,

  // System tray quick settings view buttons:
  VIEW_ID_QS_MIN,
  VIEW_ID_QS_BATTERY_BUTTON = VIEW_ID_QS_MIN,
  VIEW_ID_QS_COLLAPSE_BUTTON,
  VIEW_ID_QS_DATE_VIEW_BUTTON,
  VIEW_ID_QS_FEEDBACK_BUTTON,
  VIEW_ID_QS_LOCK_BUTTON,
  VIEW_ID_QS_MANAGED_BUTTON,
  VIEW_ID_QS_POWER_BUTTON,
  VIEW_ID_QS_SETTINGS_BUTTON,
  VIEW_ID_QS_SIGN_OUT_BUTTON,
  VIEW_ID_QS_USER_AVATAR_BUTTON,
  VIEW_ID_QS_VERSION_BUTTON,
  VIEW_ID_QS_MAX = VIEW_ID_QS_VERSION_BUTTON,

  // Sticky header rows in a scroll view.
  VIEW_ID_STICKY_HEADER,
  // System tray up-arrow icon that shows an update is available.
  VIEW_ID_TRAY_UPDATE_ICON,
  // System tray menu item label for updates (e.g. "Restart to update").
  VIEW_ID_TRAY_UPDATE_MENU_LABEL,

  // Start and end of system tray UserItemButton in UserChooserView. First
  // user gets VIEW_ID_USER_ITEM_BUTTON_START. DCHECKs if the number of user
  // is more than 10.
  VIEW_ID_USER_ITEM_BUTTON_START,
  VIEW_ID_USER_ITEM_BUTTON_END = VIEW_ID_USER_ITEM_BUTTON_START + 10,

  VIEW_ID_USER_VIEW_MEDIA_INDICATOR,
  // Keep alphabetized.
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_VIEW_IDS_H_
