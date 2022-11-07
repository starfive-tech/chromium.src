// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_

#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TestAppListColorProvider : public AppListColorProvider {
 public:
  TestAppListColorProvider() = default;
  ~TestAppListColorProvider() override = default;

 public:
  // AppListColorProvider:
  SkColor GetAppListBackgroundColor(
      bool is_tablet_mode,
      SkColor default_color,
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxSecondaryTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxSuggestionTextColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const override;
  SkColor GetSuggestionChipBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetSuggestionChipTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetAppListItemTextColor(
      bool is_in_folder,
      const views::Widget* app_list_widget) const override;
  SkColor GetPageSwitcherButtonColor(
      bool is_root_app_grid_page_switcher,
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxIconColor(
      SkColor default_color,
      const views::Widget* app_list_widget) const override;
  SkColor GetSearchBoxCardBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderTitleTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderHintTextColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNameBorderColor(
      bool active,
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNameSelectionColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetFolderNotificationBadgeColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetContentsBackgroundColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardActiveColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetGridBackgroundCardInactiveColor(
      const views::Widget* app_list_widget) const override;
  ui::ColorId GetSeparatorColorId() const override;
  SkColor GetFocusRingColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetInkDropBaseColor(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  float GetInkDropOpacity(
      const views::Widget* app_list_widget,
      SkColor bg_color = gfx::kPlaceholderColor) const override;
  SkColor GetSearchResultViewHighlightColor(
      const views::Widget* app_list_widget) const override;
  SkColor GetTextColorURL(const views::Widget* app_list_widget) const override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_APP_LIST_COLOR_PROVIDER_H_
