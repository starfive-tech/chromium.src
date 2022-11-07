// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_list_handler.h"

#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {
namespace {
shopping_list::mojom::BookmarkProductInfoPtr BookmarkNodeToMojoProduct(
    bookmarks::BookmarkModel& model,
    const bookmarks::BookmarkNode* node,
    const std::string& locale) {
  auto bookmark_info = shopping_list::mojom::BookmarkProductInfo::New();
  bookmark_info->bookmark_id = node->id();

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(&model, node);
  const power_bookmarks::ShoppingSpecifics specifics =
      meta->shopping_specifics();

  bookmark_info->info = shopping_list::mojom::ProductInfo::New();
  bookmark_info->info->title = specifics.title();
  bookmark_info->info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(node->url())));

  bookmark_info->info->product_url = node->url();
  bookmark_info->info->image_url = GURL(meta->lead_image().url());

  const power_bookmarks::ProductPrice price = specifics.current_price();
  std::string current_code = price.currency_code();

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(current_code, locale);
  formatter->SetMaxFractionalDigits(2);

  bookmark_info->info->current_price =
      base::UTF16ToUTF8(formatter->Format(base::NumberToString(
          static_cast<float>(price.amount_micros()) / kToMicroCurrency)));

  // TODO(1346620): Hook up previous price. We might need to fetch new
  //                information when the UI is loaded to handle both
  //                previous and current price.

  return bookmark_info;
}
}  // namespace

using shopping_list::mojom::BookmarkProductInfo;
using shopping_list::mojom::BookmarkProductInfoPtr;

ShoppingListHandler::ShoppingListHandler(
    mojo::PendingRemote<shopping_list::mojom::Page> remote_page,
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    const std::string& locale)
    : remote_page_(std::move(remote_page)),
      receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      locale_(locale) {
  if (base::FeatureList::IsEnabled(kShoppingList)) {
    scoped_observation_.Observe(bookmark_model);
    shopping_service_->ScheduleSavedProductUpdate();
  }
}

ShoppingListHandler::~ShoppingListHandler() = default;

void ShoppingListHandler::GetAllPriceTrackedBookmarkProductInfo(
    GetAllPriceTrackedBookmarkProductInfoCallback callback) {
  if (!base::FeatureList::IsEnabled(kShoppingList)) {
    std::move(callback).Run({});
    return;
  }
  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      GetAllPriceTrackedBookmarks(bookmark_model_);

  std::vector<BookmarkProductInfoPtr> info_list =
      BookmarkListToMojoList(*bookmark_model_, bookmarks, locale_);

  std::move(callback).Run(std::move(info_list));
}

void ShoppingListHandler::TrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), true,
      base::BindOnce(&ShoppingListHandler::onPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_));
}

void ShoppingListHandler::UntrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), false,
      base::BindOnce(&ShoppingListHandler::onPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_));
}

void ShoppingListHandler::BookmarkModelChanged() {}

void ShoppingListHandler::BookmarkMetaInfoChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);
  if (!meta || !meta->has_shopping_specifics())
    return;

  if (commerce::IsBookmarkPriceTracked(model, node)) {
    remote_page_->PriceTrackedForBookmark(
        BookmarkNodeToMojoProduct(*model, node, locale_));
  } else {
    remote_page_->PriceUntrackedForBookmark(node->id());
  }
}

std::vector<BookmarkProductInfoPtr> ShoppingListHandler::BookmarkListToMojoList(
    bookmarks::BookmarkModel& model,
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    const std::string& locale) {
  std::vector<BookmarkProductInfoPtr> info_list;

  for (const bookmarks::BookmarkNode* node : bookmarks) {
    info_list.push_back(BookmarkNodeToMojoProduct(model, node, locale));
  }

  return info_list;
}

void ShoppingListHandler::onPriceTrackResult(int64_t bookmark_id,
                                             bookmarks::BookmarkModel* model,
                                             bool success) {
  if (success)
    return;
  auto* node = bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
  if (commerce::IsBookmarkPriceTracked(bookmark_model_, node)) {
    remote_page_->PriceTrackedForBookmark(
        BookmarkNodeToMojoProduct(*model, node, locale_));
  } else {
    remote_page_->PriceUntrackedForBookmark(bookmark_id);
  }
}
}  // namespace commerce
