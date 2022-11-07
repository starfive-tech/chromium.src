// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser_user_data.h"

class Browser;
class HistoryClustersSidePanelUI;
class SidePanelRegistry;

namespace views {
class View;
}

// HistoryClustersSidePanelCoordinator handles the creation and registration of
// the history clusters SidePanelEntry.
class HistoryClustersSidePanelCoordinator
    : public BrowserUserData<HistoryClustersSidePanelCoordinator> {
 public:
  explicit HistoryClustersSidePanelCoordinator(Browser* browser);
  HistoryClustersSidePanelCoordinator(
      const HistoryClustersSidePanelCoordinator&) = delete;
  HistoryClustersSidePanelCoordinator& operator=(
      const HistoryClustersSidePanelCoordinator&) = delete;
  ~HistoryClustersSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // Shows the Journeys side panel with `query` pre-populated. Returns true if
  // this was successful.
  bool Show(const std::string& query);

 private:
  friend class BrowserUserData<HistoryClustersSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateHistoryClustersWebView();

  // A weak reference to the last-created UI object for this browser.
  base::WeakPtr<HistoryClustersSidePanelUI> history_clusters_ui_;

  // Used to store the initial query for the next-created WebUI instance.
  std::string initial_query_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_COORDINATOR_H_
