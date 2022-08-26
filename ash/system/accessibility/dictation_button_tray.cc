// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_button_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

// Helper function that creates an image for the dictation icon.
// |active| means Dictation is actively listening for speech. The icon
// changes to an "on" icon from "off" when Dictation is listening.
// |enabled| indicates whether the tray button is enabled, i.e. clickable.
// A secondary color is used to indicate the icon is not enabled.
gfx::ImageSkia GetIconImage(bool active, bool enabled) {
  const SkColor color =
      enabled
          ? TrayIconColor(Shell::Get()->session_controller()->GetSessionState())
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorSecondary);
  return active ? gfx::CreateVectorIcon(kDictationOnNewuiIcon, color)
                : gfx::CreateVectorIcon(kDictationOffNewuiIcon, color);
}

}  // namespace

DictationButtonTray::DictationButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(new views::ImageView()),
      download_progress_(0) {
  Shell* shell = Shell::Get();
  ui::TextInputClient* client =
      shell->window_tree_host_manager()->input_method()->GetTextInputClient();
  in_text_input_ =
      (client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE);
  const gfx::ImageSkia icon_image =
      GetIconImage(/*active=*/false, /*enabled=*/in_text_input_);
  const int vertical_padding = (kTrayItemSize - icon_image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - icon_image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
  tray_container()->AddChildView(icon_);
  shell->AddShellObserver(this);
  shell->accessibility_controller()->AddObserver(this);
  shell->session_controller()->AddObserver(this);
  shell->window_tree_host_manager()->input_method()->AddObserver(this);
}

DictationButtonTray::~DictationButtonTray() {
  Shell* shell = Shell::Get();
  shell->RemoveShellObserver(this);
  shell->accessibility_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  shell->window_tree_host_manager()->input_method()->RemoveObserver(this);
}

bool DictationButtonTray::PerformAction(const ui::Event& event) {
  Shell::Get()->accessibility_controller()->ToggleDictationFromSource(
      DictationToggleSource::kButton);

  CheckDictationStatusAndUpdateIcon();
  return true;
}

void DictationButtonTray::Initialize() {
  TrayBackgroundView::Initialize();
  UpdateVisibility();
}

void DictationButtonTray::ClickedOutsideBubble() {}

void DictationButtonTray::OnDictationStarted() {
  UpdateIcon(/*dictation_active=*/true);
}

void DictationButtonTray::OnDictationEnded() {
  UpdateIcon(/*dictation_active=*/false);
}

void DictationButtonTray::OnAccessibilityStatusChanged() {
  UpdateVisibility();
  CheckDictationStatusAndUpdateIcon();
}

std::u16string DictationButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_DICTATION_BUTTON_ACCESSIBLE_NAME);
}

void DictationButtonTray::HandleLocaleChange() {
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
}

void DictationButtonTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void DictationButtonTray::OnThemeChanged() {
  TrayBackgroundView::OnThemeChanged();
  icon_->SetImage(
      GetIconImage(Shell::Get()->accessibility_controller()->dictation_active(),
                   GetEnabled()));
  if (progress_indicator_)
    progress_indicator_->InvalidateLayer();
}

void DictationButtonTray::Layout() {
  TrayBackgroundView::Layout();
  UpdateProgressIndicatorBounds();
}

const char* DictationButtonTray::GetClassName() const {
  return "DictationButtonTray";
}

void DictationButtonTray::OnSessionStateChanged(
    session_manager::SessionState state) {
  CheckDictationStatusAndUpdateIcon();
}

void DictationButtonTray::UpdateIcon(bool dictation_active) {
  icon_->SetImage(GetIconImage(dictation_active, GetEnabled()));
  SetIsActive(dictation_active);
}

void DictationButtonTray::UpdateProgressIndicatorBounds() {
  if (progress_indicator_)
    progress_indicator_->layer()->SetBounds(GetBackgroundBounds());
}

void DictationButtonTray::UpdateVisibility() {
  bool is_visible =
      Shell::Get()->accessibility_controller()->dictation().enabled();
  SetVisiblePreferred(is_visible);
}

void DictationButtonTray::CheckDictationStatusAndUpdateIcon() {
  UpdateIcon(Shell::Get()->accessibility_controller()->dictation_active());
}

void DictationButtonTray::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  TextInputChanged(client);
}

void DictationButtonTray::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  TextInputChanged(client);
}

void DictationButtonTray::TextInputChanged(const ui::TextInputClient* client) {
  in_text_input_ =
      client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  SetEnabled((download_progress_ <= 0 || download_progress_ >= 100) &&
             in_text_input_);
  CheckDictationStatusAndUpdateIcon();
}

void DictationButtonTray::UpdateOnSpeechRecognitionDownloadChanged(
    int download_progress) {
  if (!visible_preferred())
    return;

  bool download_in_progress = download_progress > 0 && download_progress < 100;
  SetEnabled(!download_in_progress && in_text_input_);
  icon_->SetTooltipText(l10n_util::GetStringUTF16(
      download_in_progress
          ? IDS_ASH_ACCESSIBILITY_DICTATION_BUTTON_TOOLTIP_SODA_DOWNLOADING
          : IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));

  // Progress indicator.
  download_progress_ = download_progress;
  if (!progress_indicator_) {
    // A progress indicator that is only visible when a SODA download is
    // in-progress and a subscription to receive notification of progress
    // changed events.
    progress_indicator_ =
        ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
            [](DictationButtonTray* tray) -> absl::optional<float> {
              // If download is in-progress, return the progress as a decimal.
              // Otherwise, the progress indicator shouldn't be painted.
              const int progress = tray->download_progress();
              return (progress > 0 && progress < 100)
                         ? progress / 100.f
                         : ProgressIndicator::kProgressComplete;
            },
            base::Unretained(this)));
    progress_indicator_->SetInnerIconVisible(false);
    layer()->Add(progress_indicator_->CreateLayer());
    UpdateProgressIndicatorBounds();
  }
  progress_indicator_->InvalidateLayer();
}

}  // namespace ash
