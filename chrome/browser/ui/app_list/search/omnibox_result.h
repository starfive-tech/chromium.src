// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class AppListControllerDelegate;
class BitmapFetcher;
class Profile;

namespace app_list {

class OmniboxResult : public ChromeSearchResult,
                      public BitmapFetcherDelegate,
                      public ash::ColorModeObserver,
                      public crosapi::mojom::SearchResultConsumer {
 public:
  // `remove_closure` must remove this result from the results list; it is
  // called when the "x" button next to the search result is pressed.
  //
  // TODO(1272361): remove this argument once result deletions are handled via
  //                their own ranker.
  OmniboxResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                base::RepeatingClosure remove_closure,
                crosapi::mojom::SearchResultPtr search_result,
                const std::u16string& query,
                bool is_zero_suggestion);
  ~OmniboxResult() override;

  OmniboxResult(const OmniboxResult&) = delete;
  OmniboxResult& operator=(const OmniboxResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  void InvokeAction(ash::SearchResultActionType action) override;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int dedup_priority() const { return dedup_priority_; }

 private:
  // ash::ColorModeObserver:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  // crosapi::mojom::SearchResultConsumer:
  void OnFaviconReceived(const gfx::ImageSkia& icon) override;

  void UpdateIcon();
  // Creates a generic backup icon: used when rich icons are not available.
  void SetGenericIcon();
  void UpdateTitleAndDetails();

  void Remove();

  // Returns true if match indicates a url search result with non-empty
  // description.
  bool IsUrlResultWithDescription() const;

  // Returns true if match has an image url.
  bool IsRichEntity() const;
  void FetchRichEntityImage(const GURL& url);

  void InitializeButtonActions(
      const std::vector<ash::SearchResultActionType>& button_actions);

  ash::SearchResultType GetSearchResultType() const;

  // Indicates the priority of a result for deduplicatin. Results with the same
  // ID but lower |dedup_priority| are removed.
  int dedup_priority_ = 0;

  // Handle used to receive asynchronous data about this search result over
  // Mojo.
  const mojo::Receiver<crosapi::mojom::SearchResultConsumer> consumer_receiver_;

  Profile* const profile_;
  AppListControllerDelegate* const list_controller_;
  crosapi::mojom::SearchResultPtr search_result_;
  const base::RepeatingClosure remove_closure_;
  const std::u16string query_;
  const bool is_zero_suggestion_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
  // Whether this omnibox result uses a generic backup icon.
  bool uses_generic_icon_ = false;

  // Cached unwrapped search result fields.
  const std::u16string contents_;
  const std::u16string description_;

  base::WeakPtrFactory<OmniboxResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
