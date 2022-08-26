// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_

#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
}

class PictureInPictureBrowserFrameView : public BrowserNonClientFrameView,
                                         public ChromeLocationBarModelDelegate,
                                         public LocationIconView::Delegate,
                                         public IconLabelBubbleView::Delegate {
 public:
  METADATA_HEADER(PictureInPictureBrowserFrameView);

  PictureInPictureBrowserFrameView(BrowserFrame* frame,
                                   BrowserView* browser_view);
  PictureInPictureBrowserFrameView(const PictureInPictureBrowserFrameView&) =
      delete;
  PictureInPictureBrowserFrameView& operator=(
      const PictureInPictureBrowserFrameView&) = delete;
  ~PictureInPictureBrowserFrameView() override;

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override {}
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void Layout() override;

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;
  bool GetURL(GURL* url) const override;
  bool ShouldTrimDisplayUrlAfterHostName() const override;
  bool ShouldDisplayURL() const override;

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override;
  bool IsEditingOrEmpty() const override;
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override;
  bool ShowPageInfoDialog() override;
  LocationBarModel* GetLocationBarModel() const override;
  ui::ImageModel GetLocationIcon(LocationIconView::Delegate::IconFetchedCallback
                                     on_icon_fetched) const override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // Gets the bounds of the controls.
  gfx::Rect GetBackToTabControlsBounds() const;
  gfx::Rect GetCloseControlsBounds() const;

 private:
  // A model required to use LocationIconView.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  raw_ptr<views::View> window_background_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> controls_container_view_ = nullptr;

  // An icon to the left of the window title, which reuses the location icon in
  // the location bar of a normal browser. Since the web contents to PiP is
  // guaranteed to be secure, this icon should always be the HTTPS lock.
  raw_ptr<LocationIconView> location_icon_view_ = nullptr;

  raw_ptr<views::Label> window_title_ = nullptr;
  raw_ptr<CloseImageButton> close_image_button_ = nullptr;
  raw_ptr<views::View> back_to_tab_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_H_
