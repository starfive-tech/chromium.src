// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/test_utils.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "url/gurl.h"

namespace commerce {

const bookmarks::BookmarkNode* AddProductBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const std::u16string& title,
    const GURL& url,
    uint64_t cluster_id,
    bool is_price_tracked,
    const int64_t price_micros,
    const std::string& currency_code) {
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(), 0, title, url);
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();
  specifics->set_title(base::UTF16ToUTF8(title));
  specifics->set_product_cluster_id(cluster_id);
  specifics->set_is_price_tracked(is_price_tracked);

  specifics->mutable_current_price()->set_currency_code(currency_code);
  specifics->mutable_current_price()->set_amount_micros(price_micros);

  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model, node,
                                            std::move(meta));
  return node;
}

}  // namespace commerce
