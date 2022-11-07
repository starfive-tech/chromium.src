// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"

#include "base/callback.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

HistoryClustersSidePanelCoordinator::HistoryClustersSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<HistoryClustersSidePanelCoordinator>(*browser) {}

HistoryClustersSidePanelCoordinator::~HistoryClustersSidePanelCoordinator() =
    default;

void HistoryClustersSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kHistoryClusters,
      l10n_util::GetStringUTF16(IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL),
      ui::ImageModel::FromVectorIcon(kJourneysIcon, ui::kColorIcon,
                                     /*icon_size=*/16),
      base::BindRepeating(
          &HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
HistoryClustersSidePanelCoordinator::CreateHistoryClustersWebView() {
  // Construct our URL including our initial query. Other ways of passing the
  // initial query to the WebUI interface are mostly all racy.
  std::string query_string = base::StringPrintf(
      "initial_query=%s",
      base::EscapeQueryParamValue(initial_query_, /*use_plus=*/false).c_str());
  initial_query_.clear();

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  const GURL url = GURL(chrome::kChromeUIHistoryClustersSidePanelURL)
                       .ReplaceComponents(replacements);

  auto side_panel_ui =
      std::make_unique<SidePanelWebUIViewT<HistoryClustersSidePanelUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<HistoryClustersSidePanelUI>>(
              url, GetBrowser().profile(),
              IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));

  history_clusters_ui_ =
      side_panel_ui->contents_wrapper()->GetWebUIController()->GetWeakPtr();

  return std::move(side_panel_ui);
}

bool HistoryClustersSidePanelCoordinator::Show(const std::string& query) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(&GetBrowser());
  if (!browser_view)
    return false;

  if (history_clusters_ui_) {
    history_clusters_ui_->SetQuery(query);
  } else {
    // If the WebUI hasn't been created yet, store it in a member, so that when
    // it is created (by a different callsite) it receives the initial query.
    initial_query_ = query;
  }

  if (auto* side_panel_coordinator = browser_view->side_panel_coordinator()) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kHistoryClusters);
  }

  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersSidePanelCoordinator);
