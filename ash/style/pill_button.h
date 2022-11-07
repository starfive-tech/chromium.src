// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_PILL_BUTTON_H_
#define ASH_STYLE_PILL_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

// A label button with a rounded rectangle background. It can have an icon
// inside as well, and its text and background colors will be different based on
// the type of the button.
class ASH_EXPORT PillButton : public views::LabelButton {
 public:
  METADATA_HEADER(PillButton);

  static constexpr int kPillButtonHorizontalSpacing = 16;

  // The PillButton style is defined with 4 features:
  // 1. Color variant defines which background, text, and icon color scheme to
  // be used, for example Default, Floating, Alert, etc.
  // 2. Button size indicates whether we should use the default size 32 or a
  // large size 36.
  // 3. With/without an icon.
  // 4. Icon position: leading or following.
  // For ease of extracting features from a button type, each feature is
  // represented by a different bit mask.
  using TypeFlag = int;

  static constexpr TypeFlag kDefault = 1;
  static constexpr TypeFlag kDefaultElevated = 1 << 1;
  static constexpr TypeFlag kPrimary = 1 << 2;
  static constexpr TypeFlag kSecondary = 1 << 3;
  static constexpr TypeFlag kFloating = 1 << 4;
  static constexpr TypeFlag kAlert = 1 << 5;
  // TODO(crbug.com/1355517): Get rid of `kAccent` after CrosNext is fully
  // launched.
  static constexpr TypeFlag kAccent = 1 << 6;
  static constexpr TypeFlag kLarge = 1 << 7;
  static constexpr TypeFlag kIconLeading = 1 << 8;
  static constexpr TypeFlag kIconFollowing = 1 << 9;

  // Types of the PillButton. Each type is represented as the bitwise OR
  // operation of the feature bit masks. The naming rule of the button type is
  // k{Color Variant}{Button Size}{Icon}{Icon Position}.
  enum Type {
    // PillButton with default text and background colors, a leading icon.
    kDefaultWithIconLeading = kDefault | kIconLeading,
    // PillButton with default text and background colors, a following icon.
    kDefaultWithIconFollowing = kDefault | kIconFollowing,
    // PillButton with default text and background colors, a large button size,
    // a leading icon.
    kDefaultLargeWithIconLeading = kDefault | kLarge | kIconLeading,
    // PillButton with default text and background colors, a large button size,
    // a following icon.
    kDefaultLargeWithIconFollowing = kDefault | kLarge | kIconFollowing,
    // PillButton with default text and background colors, no icon.
    kDefaultWithoutIcon = kDefault,
    // PillButton with default text and background colors, a large button size,
    // no icon.
    kDefaultLargeWithoutIcon = kDefault | kLarge,

    // PillButton with default-elevated text and background colors, a leading
    // icon.
    kDefaultElevatedWithIconLeading = kDefault | kIconLeading,
    // PillButton with default-elevated text and background colors, a following
    // icon.
    kDefaultElevatedWithIconFollowing = kDefault | kIconFollowing,
    // PillButton with default-elevated text and background colors, a large
    // button size, a leading icon.
    kDefaultElevatedLargeWithIconLeading = kDefault | kLarge | kIconLeading,
    // PillButton with default-elevated text and background colors, a large
    // button size, a following icon.
    kDefaultElevatedLargeWithIconFollowing = kDefault | kLarge | kIconFollowing,
    // PillButton with default-elevated text and background colors, no icon.
    kDefaultElevatedWithoutIcon = kDefault,
    // PillButton with default-elevated text and background colors, a large
    // button size,
    // no icon.
    kDefaultElevatedLargeWithoutIcon = kDefault | kLarge,

    // PillButton with primary text and background colors, a leading icon.
    kPrimaryWithIconLeading = kPrimary | kIconLeading,
    // PillButton with primary text and background colors, a following icon.
    kPrimaryWithIconFollowing = kPrimary | kIconFollowing,
    // PillButton with primary text and background colors, a large button size,
    // a leading icon.
    kPrimaryLargeWithIconLeading = kPrimary | kLarge | kIconLeading,
    // PillButton with primary text and background colors, a large button size,
    // a following icon.
    kPrimaryLargeWithIconFollowing = kPrimary | kLarge | kIconFollowing,
    // PillButton with primary text and background colors, no icon.
    kPrimaryWithoutIcon = kPrimary,
    // PillButton with primary text and background colors, a large button size,
    // no icon.
    kPrimaryLargeWithoutIcon = kPrimary | kLarge,

    // PillButton with secondary text and background colors, a leading icon.
    kSecondaryWithIconLeading = kSecondary | kIconLeading,
    // PillButton with secondary text and background colors, a following icon.
    kSecondaryWithIconFollowing = kSecondary | kIconFollowing,
    // PillButton with secondary text and background colors, a large button
    // size, a leading icon.
    kSecondaryLargeWithIconLeading = kSecondary | kLarge | kIconLeading,
    // PillButton with secondary text and background colors, a large button
    // size, a following icon.
    kSecondaryLargeWithIconFollowing = kSecondary | kLarge | kIconFollowing,
    // PillButton with secondary text and background colors, no icon.
    kSecondaryWithoutIcon = kSecondary,
    // PillButton with secondary text and background colors, a large button
    // size, no icon.
    kSecondaryLargeWithoutIcon = kSecondary | kLarge,

    // PillButton with floating text colors, no background, a leading icon.
    kFloatingWithIconLeading = kFloating | kIconLeading,
    // PillButton with floating text colors, no background, a following icon.
    kFloatingWithIconFollowing = kFloating | kIconFollowing,
    // PillButton with floating text colors, no background, a large button size,
    // a leading icon.
    kFloatingLargeWithIconLeading = kFloating | kLarge | kIconLeading,
    // PillButton with floating text colors, no background, a large button size,
    // a following icon.
    kFloatingLargeWithIconFollowing = kFloating | kLarge | kIconFollowing,
    // PillButton with floating text colors, no background, no icon.
    kFloatingWithoutIcon = kFloating,
    // PillButton with floating text colors, no background, a large button size,
    // no icon.
    kFloatingLargeWithoutIcon = kFloating | kLarge,

    // PillButton with alert text and background colors, a leading icon.
    kAlertWithIconLeading = kAlert | kIconLeading,
    // PillButton with alert text and background colors, a following icon.
    kAlertWithIconFollowing = kAlert | kIconFollowing,
    // PillButton with alert text and background colors, a large button size, a
    // leading icon.
    kAlertLargeWithIconLeading = kAlert | kLarge | kIconLeading,
    // PillButton with alert text and background colors, a large button size, a
    // following icon.
    kAlertLargeWithIconFollowing = kAlert | kLarge | kIconFollowing,
    // PillButton with alert text and background colors, no icon.
    kAlertWithoutIcon = kAlert,
    // PillButton with alert text and background colors, a large button size, no
    // icon.
    kAlertLargeWithoutIcon = kAlert | kLarge,

    // Old button types.
    // TODO(crbug.com/1355517): Get rid of these types after CrosNext is fully
    // launched.
    // PillButton with accent text and background colors, no icon.
    kAccentWithoutIcon = kAccent,
    // PillButton with accent text, no background, no icon.
    kAccentFloatingWithoutIcon = kAccent | kFloating,
  };

  // Keeps the button in light mode if `use_light_colors` is true.
  // InstallRoundRectHighlightPathGenerator for the button only if
  // `rounded_highlight_path` is true. This is special handlings for buttons
  // inside the old notifications UI, might can be removed once
  // `kNotificationsRefresh` is fully launched.
  explicit PillButton(PressedCallback callback = PressedCallback(),
                      const std::u16string& text = std::u16string(),
                      Type type = Type::kDefaultWithoutIcon,
                      const gfx::VectorIcon* icon = nullptr,
                      int horizontal_spacing = kPillButtonHorizontalSpacing,
                      bool use_light_colors = false,
                      bool rounded_highlight_path = true);
  PillButton(const PillButton&) = delete;
  PillButton& operator=(const PillButton&) = delete;
  ~PillButton() override;

  // views::LabelButton:
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnThemeChanged() override;
  gfx::Insets GetInsets() const override;
  void UpdateBackgroundColor() override;

  // Sets the button's background color, text's color or icon's color. Note, do
  // this only when the button wants to have different colors from the default
  // ones.
  void SetBackgroundColor(const SkColor background_color);
  void SetButtonTextColor(const SkColor text_color);
  void SetIconColor(const SkColor icon_color);
  void SetPillButtonType(Type type);

  // Sets the button's label to use the default label font, which is smaller
  // and less heavily weighted.
  void SetUseDefaultLabelFont();

 private:
  // Initializes the button layout, focus ring and background according to the
  // button type.
  void Init();

  void UpdateTextColor();
  void UpdateIconColor();

  // Returns the spacing on the side where the icon locates. The value is set
  // smaller to make the spacing on two sides visually look the same.
  int GetHorizontalSpacingWithIcon() const;

  Type type_;
  const gfx::VectorIcon* const icon_;

  // True if the button wants to use light colors when the D/L mode feature is
  // not enabled. Note, can be removed when D/L mode feature is fully launched.
  bool use_light_colors_;

  // Horizontal spacing of this button. `kPillButtonHorizontalSpacing` will be
  // set as the default value.
  int horizontal_spacing_;

  // The flag that indicates if highlight path is used for focus ring.
  const bool rounded_highlight_path_;

  // Customized value for the button's background color, text's color and icon's
  // color.
  absl::optional<SkColor> background_color_;
  absl::optional<SkColor> text_color_;
  absl::optional<SkColor> icon_color_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PillButton, views::LabelButton)
VIEW_BUILDER_PROPERTY(const SkColor, BackgroundColor)
VIEW_BUILDER_PROPERTY(const SkColor, TextColor)
VIEW_BUILDER_PROPERTY(const SkColor, IconColor)
VIEW_BUILDER_PROPERTY(PillButton::Type, PillButtonType)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PillButton)

#endif  // ASH_STYLE_PILL_BUTTON_H_
