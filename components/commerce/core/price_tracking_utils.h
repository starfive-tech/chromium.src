// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

struct ProductInfo;
class ShoppingService;

// Return whether a bookmark is price tracked. This does not check the
// subscriptions backend, only the flag in the bookmark meta.
bool IsBookmarkPriceTracked(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node);

// Return whether the |node| is a product bookmark.
bool IsProductBookmark(bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* node);

// Set the state of price tracking for all bookmarks with the cluster ID of the
// provided bookmark. A subscription update will attempted on the backend and,
// if successful, all bookmarks with the same cluster ID will be updated.
// |callback| will be called with a bool representing whether the operation was
// successful iff all of |service|, |model|, and |node| are non-null and the
// bookmark has been determined to be a product.
void SetPriceTrackingStateForBookmark(ShoppingService* service,
                                      bookmarks::BookmarkModel* model,
                                      const bookmarks::BookmarkNode* node,
                                      bool enabled,
                                      base::OnceCallback<void(bool)> callback);

// Get all bookmarks with the specified product cluster ID.
std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithClusterId(
    bookmarks::BookmarkModel* model,
    uint64_t cluster_id);

// Get all bookmarks that are price tracked. This only checks the bit in the
// bookmark metadata and does not make a call to the backend. The returned
// vector of BookmarkNodes is owned by the caller, but the nodes pointed to
// are not -- those live for as long as the BookmarkModel (|model|) is alive
// which has the same lifetime as the current BrowserContext.
std::vector<const bookmarks::BookmarkNode*> GetAllPriceTrackedBookmarks(
    bookmarks::BookmarkModel* model);

// Get all shopping bookmarks. The returned vector of BookmarkNodes is owned by
// the caller, but the nodes pointed to are not -- those live for as long as
// the BookmarkModel (|model|) is alive which has the same lifetime as the
// current BrowserContext.
std::vector<const bookmarks::BookmarkNode*> GetAllShoppingBookmarks(
    bookmarks::BookmarkModel* model);

// Populate or update the provided |out_meta| with information from |info|. The
// returned boolean indicated whether any information actually changed.
bool PopulateOrUpdateBookmarkMetaIfNeeded(
    power_bookmarks::PowerBookmarkMeta* out_meta,
    const ProductInfo& info);

// Attempts to enable price email notifications for users. This will only set
// the setting to true if it is the first time being called, after that this is
// a noop.
void MaybeEnableEmailNotifications(PrefService* pref_service);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
