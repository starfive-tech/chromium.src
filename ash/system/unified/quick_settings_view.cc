// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_view.h"

#include <numeric>

#include "ash/constants/ash_features.h"
#include "ash/system/media/unified_media_controls_container.h"
#include "ash/system/tray/interacted_by_tap_recorder.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pods_container_view.h"
#include "ash/system/unified/page_indicator_view.h"
#include "ash/system/unified/top_shortcuts_view.h"
#include "ash/system/unified/unified_system_info_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

class DetailedViewContainer : public views::View {
 public:
  METADATA_HEADER(DetailedViewContainer);

  DetailedViewContainer() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  }

  DetailedViewContainer(const DetailedViewContainer&) = delete;
  DetailedViewContainer& operator=(const DetailedViewContainer&) = delete;

  ~DetailedViewContainer() override = default;

  // views::View:
  void Layout() override {
    for (auto* child : children())
      child->SetBoundsRect(GetContentsBounds());
    views::View::Layout();
  }
};

BEGIN_METADATA(DetailedViewContainer, views::View)
END_METADATA

class AccessibilityFocusHelperView : public views::View {
 public:
  explicit AccessibilityFocusHelperView(UnifiedSystemTrayController* controller)
      : controller_(controller) {}

  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    GetFocusManager()->ClearFocus();
    GetFocusManager()->SetStoredFocusView(nullptr);
    controller_->FocusOut(false);
    return true;
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kListItem;
  }

 private:
  UnifiedSystemTrayController* controller_;
};

}  // namespace

SlidersContainerView::SlidersContainerView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

SlidersContainerView::~SlidersContainerView() = default;

int SlidersContainerView::GetHeight() const {
  return std::accumulate(children().cbegin(), children().cend(), 0,
                         [](int height, const auto* v) {
                           return height + v->GetHeightForWidth(kTrayMenuWidth);
                         });
}

gfx::Size SlidersContainerView::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetHeight());
}

BEGIN_METADATA(SlidersContainerView, views::View)
END_METADATA

// The container view for the system tray, i.e. the panel containing settings
// buttons and sliders (e.g. sign out, lock, volume slider, etc.).
class QuickSettingsView::SystemTrayContainer : public views::View {
 public:
  METADATA_HEADER(SystemTrayContainer);

  SystemTrayContainer()
      : layout_manager_(SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical))) {}
  SystemTrayContainer(const SystemTrayContainer&) = delete;
  SystemTrayContainer& operator=(const SystemTrayContainer&) = delete;

  ~SystemTrayContainer() override = default;

  void SetFlexForView(views::View* view) {
    DCHECK_EQ(view->parent(), this);
    layout_manager_->SetFlexForView(view, 1);
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  views::BoxLayout* const layout_manager_;
};

BEGIN_METADATA(QuickSettingsView, SystemTrayContainer, views::View)
END_METADATA

QuickSettingsView::QuickSettingsView(UnifiedSystemTrayController* controller)
    : controller_(controller),
      interacted_by_tap_recorder_(
          std::make_unique<InteractedByTapRecorder>(this)) {
  DCHECK(controller_);

  system_tray_container_ =
      AddChildView(std::make_unique<SystemTrayContainer>());
  top_shortcuts_view_ = system_tray_container_->AddChildView(
      std::make_unique<TopShortcutsView>(controller_));
  top_shortcuts_view_->SetExpandedAmount(1.0f);

  feature_pods_container_ = system_tray_container_->AddChildView(
      std::make_unique<FeaturePodsContainerView>(controller_, true));
  page_indicator_view_ = system_tray_container_->AddChildView(
      std::make_unique<PageIndicatorView>(controller_, true));

  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForChromeOS)) {
    media_controls_container_ = system_tray_container_->AddChildView(
        std::make_unique<UnifiedMediaControlsContainer>());
    media_controls_container_->SetExpandedAmount(1.0f);
  }

  sliders_container_ = system_tray_container_->AddChildView(
      std::make_unique<SlidersContainerView>());
  system_info_view_ = system_tray_container_->AddChildView(
      std::make_unique<UnifiedSystemInfoView>(controller_));

  system_tray_container_->SetFlexForView(page_indicator_view_);

  detailed_view_container_ =
      AddChildView(std::make_unique<DetailedViewContainer>());
  detailed_view_container_->SetVisible(false);

  system_tray_container_->AddChildView(
      std::make_unique<AccessibilityFocusHelperView>(controller_));
}

QuickSettingsView::~QuickSettingsView() = default;

void QuickSettingsView::AddFeaturePodButton(FeaturePodButton* button) {
  feature_pods_container_->AddFeaturePodButton(button);
}

void QuickSettingsView::AddSliderView(views::View* slider_view) {
  sliders_container_->AddChildView(slider_view);
}

void QuickSettingsView::AddMediaControlsView(views::View* media_controls) {
  DCHECK(media_controls);
  DCHECK(media_controls_container_);

  media_controls->SetPaintToLayer();
  media_controls->layer()->SetFillsBoundsOpaquely(false);
  media_controls_container_->AddChildView(media_controls);
}

void QuickSettingsView::ShowMediaControls() {
  media_controls_container_->SetShouldShowMediaControls(true);

  if (detailed_view_container_->GetVisible())
    return;

  if (media_controls_container_->MaybeShowMediaControls())
    PreferredSizeChanged();
}

void QuickSettingsView::SetDetailedView(views::View* detailed_view) {
  auto system_tray_size = system_tray_container_->GetPreferredSize();
  system_tray_container_->SetVisible(false);

  detailed_view_container_->RemoveAllChildViews();
  detailed_view_container_->AddChildView(detailed_view);
  detailed_view_container_->SetVisible(true);
  detailed_view_container_->SetPreferredSize(system_tray_size);
  detailed_view->InvalidateLayout();
  Layout();
}

void QuickSettingsView::ResetDetailedView() {
  detailed_view_container_->RemoveAllChildViews();
  detailed_view_container_->SetVisible(false);
  if (media_controls_container_)
    media_controls_container_->MaybeShowMediaControls();
  system_tray_container_->SetVisible(true);
  PreferredSizeChanged();
  Layout();
}

void QuickSettingsView::SaveFocus() {
  auto* focus_manager = GetFocusManager();
  if (!focus_manager)
    return;

  saved_focused_view_ = focus_manager->GetFocusedView();
}

void QuickSettingsView::RestoreFocus() {
  if (saved_focused_view_)
    saved_focused_view_->RequestFocus();
}

int QuickSettingsView::GetCurrentHeight() const {
  return GetPreferredSize().height();
}

int QuickSettingsView::GetVisibleFeaturePodCount() const {
  return feature_pods_container_->GetVisibleCount();
}

std::u16string QuickSettingsView::GetDetailedViewAccessibleName() const {
  return controller_->detailed_view_controller()->GetAccessibleName();
}

bool QuickSettingsView::IsDetailedViewShown() const {
  return detailed_view_container_->GetVisible();
}

gfx::Size QuickSettingsView::CalculatePreferredSize() const {
  int media_controls_container_height =
      media_controls_container_ ? media_controls_container_->GetExpandedHeight()
                                : 0;
  return gfx::Size(kTrayMenuWidth,
                   top_shortcuts_view_->GetPreferredSize().height() +
                       feature_pods_container_->GetExpandedHeight() +
                       page_indicator_view_->GetExpandedHeight() +
                       sliders_container_->GetHeight() +
                       media_controls_container_height +
                       system_info_view_->GetPreferredSize().height());
}

void QuickSettingsView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_START)
    controller_->Fling(event->details().velocity_y());
}

void QuickSettingsView::Layout() {
  if (system_tray_container_->GetVisible())
    system_tray_container_->SetBoundsRect(GetContentsBounds());
  else if (detailed_view_container_->GetVisible())
    detailed_view_container_->SetBoundsRect(GetContentsBounds());
}

void QuickSettingsView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

BEGIN_METADATA(QuickSettingsView, views::View)
END_METADATA

}  // namespace ash
