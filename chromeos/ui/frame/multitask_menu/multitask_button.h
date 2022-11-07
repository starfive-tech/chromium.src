// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {

// The base button for multitask menu to create Full Screen and Float buttons.
class MultitaskBaseButton : public views::Button {
 public:
  METADATA_HEADER(MultitaskBaseButton);

  // The types of single operated multitask button.
  enum class Type {
    kFull,   // The button that turn the window to full screen mode.
    kFloat,  // The button that float the window.
  };

  MultitaskBaseButton(PressedCallback callback,
                      Type type,
                      bool is_portrait_mode,
                      const std::u16string& name);

  MultitaskBaseButton(const MultitaskBaseButton&) = delete;
  MultitaskBaseButton& operator=(const MultitaskBaseButton&) = delete;
  ~MultitaskBaseButton() override = default;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const Type type_;
  // The display orientation. This determines whether button is in
  // landscape/portrait mode.
  const bool is_portrait_mode_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
