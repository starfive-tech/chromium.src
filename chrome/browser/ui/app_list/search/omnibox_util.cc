// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_util.h"

#include <algorithm>
#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "url/gurl.h"

namespace app_list {

bool IsDriveUrl(const GURL& url) {
  // Returns true if the |url| points to a Drive Web host.
  const std::string& host = url.host();
  return host == "drive.google.com" || host == "docs.google.com";
}

void RemoveDuplicateResults(
    std::vector<std::unique_ptr<OmniboxResult>>& results) {
  // Sort the results by deduplication priority and then filter from left to
  // right. This ensures that higher priority results are retained.
  sort(results.begin(), results.end(),
       [](const std::unique_ptr<OmniboxResult>& a,
          const std::unique_ptr<OmniboxResult>& b) {
         return a->dedup_priority() > b->dedup_priority();
       });

  base::flat_set<std::string> seen_ids;
  for (auto iter = results.begin(); iter != results.end();) {
    bool inserted = seen_ids.insert((*iter)->id()).second;
    if (!inserted) {
      // C++11:: The return value of erase(iter) is an iterator pointing to the
      // next element in the container.
      iter = results.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace app_list
