// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_view.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {
constexpr int kProductImageSize = 56;
constexpr int kLableSpacing = 4;
constexpr int kHorizontalSpacing = 16;
}  // namespace

PriceTrackingView::PriceTrackingView(Profile* profile,
                                     GURL page_url,
                                     ui::ImageModel product_image,
                                     bool is_price_track_enabled)
    : profile_(profile), is_price_track_enabled_(is_price_track_enabled) {
  // image column
  auto* product_image_containter =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  product_image_containter->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  product_image_containter->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kHorizontalSpacing));
  // Set product image.
  product_image_containter->AddChildView(
      views::Builder<views::ImageView>()
          .SetImageSize(gfx::Size(kProductImageSize, kProductImageSize))
          .SetPreferredSize(gfx::Size(kProductImageSize, kProductImageSize))
          // TODO(meiliang@): Verify color and corner radius with UX.
          .SetBorder(views::CreateRoundedRectBorder(
              1, 4, SkColorSetA(gfx::kGoogleGrey900, 0x24)))
          .SetImage(product_image)
          .Build());

  // Text column
  auto text_container = std::make_unique<views::FlexLayoutView>();
  text_container->SetOrientation(views::LayoutOrientation::kVertical);
  // Title label
  auto* title_label =
      text_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  // Body label
  body_label_ = text_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  body_label_->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(kLableSpacing, 0, 0, 0));
  body_label_->SetAllowCharacterBreak(true);
  body_label_->SetMultiLine(true);
  body_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_label_->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  AddChildView(std::move(text_container));

  // Toggle button column
  toggle_button_ = AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&PriceTrackingView::OnToggleButtonPressed,
                          weak_ptr_factory_.GetWeakPtr(), page_url)));
  toggle_button_->SetIsOn(is_price_track_enabled_);
  toggle_button_->SetAccessibleName(GetToggleAccessibleName());
  toggle_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, kHorizontalSpacing, 0, 0));

  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  const int label_width = bubble_width - kHorizontalSpacing * 4 -
                          kProductImageSize -
                          toggle_button_->GetPreferredSize().width();
  body_label_->SizeToFit(label_width);

  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.BookmarkDialogPriceTrackViewShown"));
}

PriceTrackingView::~PriceTrackingView() = default;

bool PriceTrackingView::IsToggleOn() {
  return toggle_button_->GetIsOn();
}

std::u16string PriceTrackingView::GetToggleAccessibleName() {
  return l10n_util::GetStringUTF16(
      IsToggleOn() ? IDS_PRICE_TRACKING_UNTRACK_PRODUCT_ACCESSIBILITY
                   : IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY);
}

void PriceTrackingView::OnToggleButtonPressed(const GURL url) {
  is_price_track_enabled_ = !is_price_track_enabled_;
  if (is_price_track_enabled_) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.BookmarkDialogPriceTrackViewTrackedPrice"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.BookmarkDialogPriceTrackViewUntrackedPrice"));
  }

  toggle_button_->SetAccessibleName(GetToggleAccessibleName());
  UpdatePriceTrackingState(url);
}

void PriceTrackingView::UpdatePriceTrackingState(const GURL& url) {
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const bookmarks::BookmarkNode* node =
      model->GetMostRecentlyAddedUserNodeForURL(url);
  if (profile_ && is_price_track_enabled_) {
    commerce::MaybeEnableEmailNotifications(profile_->GetPrefs());
  }
  commerce::SetPriceTrackingStateForBookmark(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_), model,
      node, is_price_track_enabled_,
      base::BindOnce(&PriceTrackingView::OnPriceTrackingStateUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PriceTrackingView::OnPriceTrackingStateUpdated(bool success) {
  // TODO(crbug.com/1346612): Record latency for the update status.
  if (!success) {
    is_price_track_enabled_ = !is_price_track_enabled_;
    toggle_button_->SetIsOn(is_price_track_enabled_);
    toggle_button_->SetAccessibleName(GetToggleAccessibleName());
    body_label_->SetText(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_TRACK_PRICE_DIALOG_ERROR_DESCRIPTION));
  }
}
