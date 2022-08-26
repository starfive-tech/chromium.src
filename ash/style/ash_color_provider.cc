// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace ash {

using ColorName = cros_styles::ColorName;

namespace {

// Opacity of the light/dark indrop.
constexpr float kLightInkDropOpacity = 0.08f;
constexpr float kDarkInkDropOpacity = 0.06f;

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// Different alpha values that can be used by Shield and Base layers.
constexpr int kAlpha20 = 51;   // 20%
constexpr int kAlpha40 = 102;  // 40%
constexpr int kAlpha60 = 153;  // 60%
constexpr int kAlpha80 = 204;  // 80%
constexpr int kAlpha90 = 230;  // 90%
constexpr int kAlpha95 = 242;  // 95%

// Alpha value that is used to calculate themed color. Please see function
// GetBackgroundThemedColor() about how the themed color is calculated.
constexpr int kDarkBackgroundBlendAlpha = 127;   // 50%
constexpr int kLightBackgroundBlendAlpha = 127;  // 50%

AshColorProvider* g_instance = nullptr;

bool IsDarkModeEnabled() {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return true;
  return Shell::Get()->dark_light_mode_controller()->IsDarkModeEnabled();
}

}  // namespace

AshColorProvider::AshColorProvider() {
  DCHECK(!g_instance);
  g_instance = this;
}

AshColorProvider::~AshColorProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AshColorProvider* AshColorProvider::Get() {
  return g_instance;
}

// static
SkColor AshColorProvider::GetDisabledColor(SkColor enabled_color) {
  return SkColorSetA(enabled_color, std::round(SkColorGetA(enabled_color) *
                                               kDisabledColorOpacity));
}

// static
SkColor AshColorProvider::GetSecondToneColor(SkColor color_of_first_tone) {
  return SkColorSetA(
      color_of_first_tone,
      std::round(SkColorGetA(color_of_first_tone) * kSecondToneOpacity));
}

SkColor AshColorProvider::GetShieldLayerColor(ShieldLayerType type) const {
  // TODO(crbug.com/1348365): Delete this function after all clients migrate.
  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  switch (type) {
    case ShieldLayerType::kShield20:
      return color_provider->GetColor(kColorAshShieldAndBase20);
    case ShieldLayerType::kShield40:
      return color_provider->GetColor(kColorAshShieldAndBase40);
    case ShieldLayerType::kShield60:
      return color_provider->GetColor(kColorAshShieldAndBase60);
    case ShieldLayerType::kShield80:
      return color_provider->GetColor(kColorAshShieldAndBase80);
    case ShieldLayerType::kShield90:
      return color_provider->GetColor(kColorAshShieldAndBase90);
    case ShieldLayerType::kShield95:
      return color_provider->GetColor(kColorAshShieldAndBase95);
  }
}

SkColor AshColorProvider::GetBaseLayerColor(BaseLayerType type) const {
  // TODO(minch): Get all the colors from `GetColorProvider` as
  // `kInvertedTransparent80`.
  const auto color = GetBackgroundColor();
  switch (type) {
    case BaseLayerType::kTransparent20:
      return SkColorSetA(color, kAlpha20);
    case BaseLayerType::kTransparent40:
      return SkColorSetA(color, kAlpha40);
    case BaseLayerType::kTransparent60:
      return SkColorSetA(color, kAlpha60);
    case BaseLayerType::kTransparent80:
      return SkColorSetA(color, kAlpha80);
    case BaseLayerType::kInvertedTransparent80:
      return GetColorProvider()->GetColor(kColorAshInvertedShieldAndBase80);
    case BaseLayerType::kTransparent90:
      return SkColorSetA(color, kAlpha90);
    case BaseLayerType::kTransparent95:
      return SkColorSetA(color, kAlpha95);
    case BaseLayerType::kOpaque:
      return SkColorSetA(color, SK_AlphaOPAQUE);
  }
}

SkColor AshColorProvider::GetControlsLayerColor(ControlsLayerType type) const {
  // TODO(crbug.com/1292244): Delete this function after all callers migrate.
  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);

  switch (type) {
    case ControlsLayerType::kHairlineBorderColor:
      return color_provider->GetColor(kColorAshHairlineBorderColor);
    case ControlsLayerType::kControlBackgroundColorActive:
      return color_provider->GetColor(kColorAshControlBackgroundColorActive);
    case ControlsLayerType::kControlBackgroundColorInactive:
      return color_provider->GetColor(kColorAshControlBackgroundColorInactive);
    case ControlsLayerType::kControlBackgroundColorAlert:
      return color_provider->GetColor(kColorAshControlBackgroundColorAlert);
    case ControlsLayerType::kControlBackgroundColorWarning:
      return color_provider->GetColor(kColorAshControlBackgroundColorWarning);
    case ControlsLayerType::kControlBackgroundColorPositive:
      return color_provider->GetColor(kColorAshControlBackgroundColorPositive);
    case ControlsLayerType::kFocusAuraColor:
      return color_provider->GetColor(kColorAshFocusAuraColor);
    case ControlsLayerType::kFocusRingColor:
      return color_provider->GetColor(ui::kColorAshFocusRing);
    case ControlsLayerType::kHighlightColor1:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight1);
    case ControlsLayerType::kHighlightColor2:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight2);
    case ControlsLayerType::kHighlightColor3:
      return color_provider->GetColor(ui::kColorHighlightBorderHighlight3);
    case ControlsLayerType::kBorderColor1:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder1);
    case ControlsLayerType::kBorderColor2:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder2);
    case ControlsLayerType::kBorderColor3:
      return color_provider->GetColor(ui::kColorHighlightBorderBorder3);
  }
}

SkColor AshColorProvider::GetContentLayerColor(ContentLayerType type) const {
  bool use_dark_color = IsDarkModeEnabled();
  auto* color_provider = GetColorProvider();
  switch (type) {
    case ContentLayerType::kSeparatorColor:
      return color_provider->GetColor(kColorAshSeparatorColor);
    case ContentLayerType::kShelfHandleColor:
      return color_provider->GetColor(kColorAshShelfHandleColor);
    case ContentLayerType::kIconColorSecondary:
      return color_provider->GetColor(kColorAshIconColorSecondary);
    case ContentLayerType::kIconColorSecondaryBackground:
      return color_provider->GetColor(kColorAshIconColorSecondaryBackground);
    case ContentLayerType::kScrollBarColor:
      return color_provider->GetColor(kColorAshScrollBarColor);
    case ContentLayerType::kSliderColorInactive:
      return color_provider->GetColor(kColorAshSliderColorInactive);
    case ContentLayerType::kRadioColorInactive:
      return color_provider->GetColor(kColorAshRadioColorInactive);
    case ContentLayerType::kSwitchKnobColorInactive:
      return color_provider->GetColor(kColorAshSwitchKnobColorInactive);
    case ContentLayerType::kSwitchTrackColorInactive:
      return color_provider->GetColor(kColorAshSwitchTrackColorInactive);
    case ContentLayerType::kButtonLabelColorBlue:
      return color_provider->GetColor(kColorAshButtonLabelColorBlue);
    case ContentLayerType::kTextColorURL:
      return color_provider->GetColor(kColorAshTextColorURL);
    case ContentLayerType::kSliderColorActive:
      return color_provider->GetColor(kColorAshSliderColorActive);
    case ContentLayerType::kRadioColorActive:
      return color_provider->GetColor(kColorAshRadioColorActive);
    case ContentLayerType::kSwitchKnobColorActive:
      return color_provider->GetColor(kColorAshSwitchKnobColorActive);
    case ContentLayerType::kProgressBarColorForeground:
      return color_provider->GetColor(kColorAshProgressBarColorForeground);
    case ContentLayerType::kProgressBarColorBackground:
      return color_provider->GetColor(kColorAshProgressBarColorBackground);
    case ContentLayerType::kCaptureRegionColor:
      return color_provider->GetColor(kColorAshCaptureRegionColor);
    case ContentLayerType::kSwitchTrackColorActive:
      return color_provider->GetColor(kColorAshSwitchTrackColorActive);
    case ContentLayerType::kButtonLabelColorPrimary:
      return color_provider->GetColor(kColorAshButtonLabelColorPrimary);
    case ContentLayerType::kButtonIconColorPrimary:
      return color_provider->GetColor(kColorAshButtonIconColorPrimary);
    case ContentLayerType::kBatteryBadgeColor:
      return color_provider->GetColor(kColorAshBatteryBadgeColor);
    case ContentLayerType::kAppStateIndicatorColorInactive:
      return color_provider->GetColor(kColorAshAppStateIndicatorColorInactive);
    case ContentLayerType::kCurrentDeskColor:
      return color_provider->GetColor(kColorAshCurrentDeskColor);
    case ContentLayerType::kSwitchAccessInnerStrokeColor:
      return color_provider->GetColor(kColorAshSwitchAccessInnerStrokeColor);
    case ContentLayerType::kSwitchAccessOuterStrokeColor:
      return color_provider->GetColor(kColorAshSwitchAccessOuterStrokeColor);
    case ContentLayerType::kHighlightColorHover:
      return color_provider->GetColor(kColorAshHighlightColorHover);
    case ContentLayerType::kAppStateIndicatorColor:
      return color_provider->GetColor(kColorAshAppStateIndicatorColor);
    case ContentLayerType::kButtonIconColor:
      return color_provider->GetColor(kColorAshButtonIconColor);
    case ContentLayerType::kButtonLabelColor:
      return color_provider->GetColor(kColorAshButtonLabelColor);
    case ContentLayerType::kBatterySystemInfoBackgroundColor:
      return color_provider->GetColor(
          kColorAshBatterySystemInfoBackgroundColor);
    case ContentLayerType::kBatterySystemInfoIconColor:
      return color_provider->GetColor(kColorAshBatterySystemInfoIconColor);
    case ContentLayerType::kInvertedTextColorPrimary:
      return color_provider->GetColor(kColorAshInvertedTextColorPrimary);
    case ContentLayerType::kInvertedButtonLabelColor:
      return color_provider->GetColor(kColorAshInvertedButtonLabelColor);
    case ContentLayerType::kTextColorSuggestion:
      return color_provider->GetColor(kColorAshTextColorSuggestion);
    case ContentLayerType::kTextColorPrimary:
      // TODO(crbug.com/1346394): Change to `color_provider` when relevant
      // callers are fixed.
      return cros_styles::ResolveColor(ColorName::kTextColorPrimary,
                                       use_dark_color);
    case ContentLayerType::kTextColorSecondary:
      // TODO(crbug.com/1346394): Change to `color_provider` when relevant
      // callers are fixed.
      return cros_styles::ResolveColor(ColorName::kTextColorSecondary,
                                       use_dark_color);
    case ContentLayerType::kTextColorAlert:
      return color_provider->GetColor(kColorAshTextColorAlert);
    case ContentLayerType::kTextColorWarning:
      return color_provider->GetColor(kColorAshTextColorWarning);
    case ContentLayerType::kTextColorPositive:
      return color_provider->GetColor(kColorAshTextColorPositive);
    case ContentLayerType::kIconColorPrimary:
      return color_provider->GetColor(kColorAshIconColorPrimary);
    case ContentLayerType::kIconColorAlert:
      return color_provider->GetColor(kColorAshIconColorAlert);
    case ContentLayerType::kIconColorWarning:
      return color_provider->GetColor(kColorAshIconColorWarning);
    case ContentLayerType::kIconColorPositive:
      return color_provider->GetColor(kColorAshIconColorPositive);
    case ContentLayerType::kIconColorProminent:
      return color_provider->GetColor(kColorAshIconColorProminent);
  }
}

SkColor AshColorProvider::GetActiveDialogTitleBarColor() const {
  return cros_styles::ResolveColor(cros_styles::ColorName::kDialogTitleBarColor,
                                   IsDarkModeEnabled());
}

SkColor AshColorProvider::GetInactiveDialogTitleBarColor() const {
  // TODO(wenbojie): Use a different inactive color in future.
  return GetActiveDialogTitleBarColor();
}

std::pair<SkColor, float> AshColorProvider::GetInkDropBaseColorAndOpacity(
    SkColor background_color) const {
  if (background_color == gfx::kPlaceholderColor)
    background_color = GetBackgroundColor();

  const bool is_dark = color_utils::IsDark(background_color);
  const SkColor base_color = is_dark ? SK_ColorWHITE : SK_ColorBLACK;
  const float opacity = is_dark ? kLightInkDropOpacity : kDarkInkDropOpacity;
  return std::make_pair(base_color, opacity);
}

SkColor AshColorProvider::GetBackgroundColor() const {
  return GetBackgroundThemedColorImpl(
      GetColorProvider()->GetColor(kColorAshShieldAndBaseOpaque),
      IsDarkModeEnabled());
}

SkColor AshColorProvider::GetBackgroundThemedColorImpl(
    SkColor default_color,
    bool use_dark_color) const {
  // May be null in unit tests.
  if (!Shell::HasInstance())
    return default_color;
  WallpaperControllerImpl* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  if (!wallpaper_controller)
    return default_color;

  color_utils::LumaRange luma_range = use_dark_color
                                          ? color_utils::LumaRange::DARK
                                          : color_utils::LumaRange::LIGHT;
  SkColor muted_color =
      wallpaper_controller->GetProminentColor(color_utils::ColorProfile(
          luma_range, color_utils::SaturationRange::MUTED));
  if (muted_color == kInvalidWallpaperColor)
    return default_color;

  return color_utils::GetResultingPaintColor(
      SkColorSetA(use_dark_color ? SK_ColorBLACK : SK_ColorWHITE,
                  use_dark_color ? kDarkBackgroundBlendAlpha
                                 : kLightBackgroundBlendAlpha),
      muted_color);
}

ui::ColorProvider* AshColorProvider::GetColorProvider() const {
  auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      native_theme->GetColorProviderKey(nullptr));
}

}  // namespace ash
