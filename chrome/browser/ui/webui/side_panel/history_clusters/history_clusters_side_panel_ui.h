// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom-forward.h"

namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryClustersSidePanelUI : public ui::MojoBubbleWebUIController {
 public:
  explicit HistoryClustersSidePanelUI(content::WebUI* web_ui);
  HistoryClustersSidePanelUI(const HistoryClustersSidePanelUI&) = delete;
  HistoryClustersSidePanelUI& operator=(const HistoryClustersSidePanelUI&) =
      delete;
  ~HistoryClustersSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<history_clusters::mojom::PageHandler>
                         pending_page_handler);

  // Gets a weak pointer to this object.
  base::WeakPtr<HistoryClustersSidePanelUI> GetWeakPtr();

  // Sets the side panel UI to show `query`.
  void SetQuery(const std::string& query);

 private:
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;

  // Used for `GetWeakPtr()`.
  base::WeakPtrFactory<HistoryClustersSidePanelUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_SIDE_PANEL_UI_H_
