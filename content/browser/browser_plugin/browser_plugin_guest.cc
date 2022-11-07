// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include "content/nw/src/nw_base.h"
#include "content/nw/src/common/shell_switches.h"
#include "base/files/file_util.h"

#include <stddef.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#endif

namespace content {

BrowserPluginGuest::BrowserPluginGuest(WebContentsImpl* web_contents,
                                       BrowserPluginGuestDelegate* delegate)
    : WebContentsObserver(web_contents), delegate_(delegate) {
  DCHECK(web_contents);
  DCHECK(delegate);
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Create"));
}

void BrowserPluginGuest::Init() {
  WebContentsImpl* owner_web_contents = static_cast<WebContentsImpl*>(
      delegate_->GetOwnerWebContents());
  owner_web_contents->CreateBrowserPluginEmbedderIfNecessary();
  InitInternal(owner_web_contents);
}

std::unique_ptr<WebContentsImpl> BrowserPluginGuest::CreateNewGuestWindow(
    const WebContents::CreateParams& params) {
  std::unique_ptr<WebContents> new_contents =
      delegate_->CreateNewGuestWindow(params);
  DCHECK(new_contents);
  return base::WrapUnique(
      static_cast<WebContentsImpl*>(new_contents.release()));
}

void BrowserPluginGuest::InitInternal(WebContentsImpl* owner_web_contents) {
  RenderWidgetHostImpl* rwhi =
      GetWebContents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  DCHECK(rwhi);
  // The initial state will not be focused but the plugin may be active so
  // set that appropriately.
  rwhi->GetWidgetInputHandler()->SetFocus(
      rwhi->is_active() ? blink::mojom::FocusState::kNotFocusedAndActive
                        : blink::mojom::FocusState::kNotFocusedAndNotActive);

  blink::RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  blink::UserAgentOverride guest_user_agent_override =
      renderer_prefs->user_agent_override;
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *owner_web_contents->GetMutableRendererPrefs();
  renderer_prefs->user_agent_override = std::move(guest_user_agent_override);

  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  nw::Package* package = nw::package();
  std::string js_doc_start, js_doc_end;
  std::string* str = package->root()->FindString(::switches::kmInjectJSDocStart);
  if (str)
    js_doc_start = *str;
  if (!js_doc_start.empty()) {
    std::string fpath = base::MakeAbsoluteFilePath(package->path()).AppendASCII(js_doc_start).AsUTF8Unsafe();
    renderer_prefs->nw_inject_js_doc_start = fpath;
  }
  str = package->root()->FindString(::switches::kmInjectJSDocEnd);
  if (str)
    js_doc_end = *str;
  if (!js_doc_end.empty()) {
    std::string fpath = base::MakeAbsoluteFilePath(package->path()).AppendASCII(js_doc_end).AsUTF8Unsafe();
    renderer_prefs->nw_inject_js_doc_end = fpath;
  }

  // TODO(chrishtr): this code is wrong. The navigate_on_drag_drop field will
  // be reset again the next time preferences are updated.
  blink::web_pref::WebPreferences prefs =
      GetWebContents()->GetOrCreateWebPreferences();
  prefs.navigate_on_drag_drop = false;
  GetWebContents()->SetWebPreferences(prefs);
}

BrowserPluginGuest::~BrowserPluginGuest() = default;

// static
void BrowserPluginGuest::CreateInWebContents(
    WebContentsImpl* web_contents,
    BrowserPluginGuestDelegate* delegate) {
  auto guest = base::WrapUnique(new BrowserPluginGuest(web_contents, delegate));
  web_contents->SetBrowserPluginGuest(std::move(guest));
}

WebContentsImpl* BrowserPluginGuest::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

void BrowserPluginGuest::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Originally added to suppress the error page when a navigation is blocked
  // using the webrequest API in a <webview> guest: https://crbug.com/284741.
  //
  // TODO(https://crbug.com/1127132): net::ERR_BLOCKED_BY_CLIENT is used for
  // many other errors. Figure out what suppression policy is desirable here.
  //
  // TODO(mcnee): Investigate moving this out to WebViewGuest.
  NavigationRequest::From(navigation_handle)
      ->SetSilentlyIgnoreBlockedByClient();
}

void BrowserPluginGuest::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted())
    RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  switch (status) {
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.LaunchFailed"));
      break;
    default:
      break;
  }
}

#if BUILDFLAG(IS_MAC)
void BrowserPluginGuest::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr>* menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  gfx::Rect translated_bounds(bounds);
  auto* guest_rwhv = render_frame_host->GetView();
  translated_bounds.set_origin(
      guest_rwhv->TransformPointToRootCoordSpace(translated_bounds.origin()));
  BrowserPluginPopupMenuHelper popup_menu_helper(render_frame_host,
                                                 std::move(*popup_client));
  popup_menu_helper.ShowPopupMenu(translated_bounds, item_height, font_size,
                                  selected_item, std::move(*menu_items),
                                  right_aligned, allow_multiple_selection);
}
#endif

}  // namespace content
