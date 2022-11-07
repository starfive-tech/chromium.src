// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"

namespace {

borealis::BorealisSplashScreenView* g_delegate = nullptr;

const SkColor background_color = SkColorSetARGB(255, 53, 51, 50);
const SkColor text_color = SkColorSetARGB(255, 209, 208, 207);
const int icon_width = 252;
const int icon_height = 77;

gfx::Image ReadImageFile(std::string dlc_path) {
  base::FilePath image_path =
      base::FilePath(dlc_path.append("/splash_logo.png"));
  std::string image_data;

  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read borealis logo from disk path " << image_path;
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
}

}  // namespace

namespace borealis {

// Declared in chrome/browser/ash/borealis/borealis_util.h.
void ShowBorealisSplashScreenView(Profile* profile) {
  return BorealisSplashScreenView::Show(profile);
}

void CloseBorealisSplashScreenView() {
  if (g_delegate) {
    g_delegate->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void BorealisSplashScreenView::Show(Profile* profile) {
  if (!g_delegate) {
    auto delegate = std::make_unique<BorealisSplashScreenView>(profile);
    g_delegate = delegate.get();
    views::DialogDelegate::CreateDialogWidget(std::move(delegate), nullptr,
                                              nullptr);
    g_delegate->GetWidget()->GetNativeWindow()->SetProperty(
        ash::kShelfIDKey, ash::ShelfID(borealis::kInstallerAppId).Serialize());
  }
}

BorealisSplashScreenView::BorealisSplashScreenView(Profile* profile)
    : weak_factory_(this) {
  profile_ = profile;
  borealis::BorealisService::GetForProfile(profile_)
      ->WindowManager()
      .AddObserver(this);

  SetShowCloseButton(false);
  SetHasWindowSizeControls(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets().set_top(150).set_bottom(100), 100);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  set_use_custom_frame(true);
  SetBackground(
      views::CreateThemedSolidBackground(kColorBorealisSplashScreenBackground));

  set_corner_radius(15);
  set_fixed_width(600);

  views::BoxLayoutView* text_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  text_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  text_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  text_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  std::unique_ptr<views::Throbber> spinner =
      std::make_unique<views::Throbber>();
  spinner->Start();
  text_view->AddChildView(std::move(spinner));

  starting_label_ = text_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BOREALIS_SPLASHSCREEN_MESSAGE),
      views::Label::CustomFont{gfx::FontList({"Google Sans"}, gfx::Font::NORMAL,
                                             18, gfx::Font::Weight::NORMAL)}));
  starting_label_->SetEnabledColor(text_color);
  starting_label_->SetBackgroundColor(background_color);

  // Get logo path and add it to view.
  borealis::GetDlcPath(base::BindOnce(&BorealisSplashScreenView::OnGetRootPath,
                                      weak_factory_.GetWeakPtr()));
}

void BorealisSplashScreenView::OnSessionStarted() {
  DCHECK(GetWidget() != nullptr);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void BorealisSplashScreenView::OnWindowManagerDeleted(
    borealis::BorealisWindowManager* window_manager) {
  DCHECK(window_manager ==
         &borealis::BorealisService::GetForProfile(profile_)->WindowManager());
  window_manager->RemoveObserver(this);
}

BorealisSplashScreenView::~BorealisSplashScreenView() {
  if (profile_)
    borealis::BorealisService::GetForProfile(profile_)
        ->WindowManager()
        .RemoveObserver(this);
  g_delegate = nullptr;
}

BorealisSplashScreenView* BorealisSplashScreenView::GetActiveViewForTesting() {
  return g_delegate;
}

void BorealisSplashScreenView::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();

  const auto* const color_provider = GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(kColorBorealisSplashScreenBackground);
  const SkColor foreground_color =
      color_provider->GetColor(kColorBorealisSplashScreenForeground);
  GetBubbleFrameView()->SetBackgroundColor(background_color);
  starting_label_->SetBackgroundColor(background_color);
  starting_label_->SetEnabledColor(foreground_color);
}

void BorealisSplashScreenView::OnGetRootPath(const std::string& path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadImageFile, path),
      base::BindOnce(&BorealisSplashScreenView::CreateImageView,
                     weak_factory_.GetWeakPtr()));
}

void BorealisSplashScreenView::CreateImageView(gfx::Image image) {
  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>();
  constexpr gfx::Size kRegularImageSize(icon_width, icon_height);
  image_view->SetImage(image.AsImageSkia());
  image_view->SetImageSize(kRegularImageSize);

  std::unique_ptr<views::BoxLayoutView> image_container =
      std::make_unique<views::BoxLayoutView>();
  image_container->AddChildView(std::move(image_view));
  AddChildViewAt(std::move(image_container), 0);
  // The logo height doesn't seem to get taken into account
  // so the splash screen gets displayed too low. Subtracting the
  // icon's height to compensate for this.
  gfx::Rect rect = GetBoundsInScreen();
  g_delegate->GetWidget()->SetBounds(
      gfx::Rect(rect.x(), rect.y() - icon_height, rect.width(), rect.height()));

  // This is the last method to run so calling Show here.
  g_delegate->GetWidget()->Show();
}

}  // namespace borealis
