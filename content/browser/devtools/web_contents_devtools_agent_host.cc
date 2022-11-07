// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/web_contents_devtools_agent_host.h"

#include "base/unguessable_token.h"
#include "content/browser/devtools/protocol/target_auto_attacher.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/portal/portal.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

namespace {
using WebContentsDevToolsMap =
    std::map<WebContents*, WebContentsDevToolsAgentHost*>;
base::LazyInstance<WebContentsDevToolsMap>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

WebContentsDevToolsAgentHost* FindAgentHost(WebContents* wc) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  auto it = g_agent_host_instances.Get().find(wc);
  return it == g_agent_host_instances.Get().end() ? nullptr : it->second;
}

bool ShouldCreateDevToolsAgentHost(WebContents* wc) {
  return wc == wc->GetResponsibleWebContents();
}

}  // namespace

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetOrCreateForTab(
    WebContents* wc) {
  return WebContentsDevToolsAgentHost::GetOrCreateFor(wc);
}

class WebContentsDevToolsAgentHost::AutoAttacher
    : public protocol::TargetAutoAttacher {
 public:
  explicit AutoAttacher(WebContents* web_contents)
      : web_contents_(web_contents) {}

  void PortalActivated(const Portal& portal) {
    if (web_contents_ == portal.GetPortalHostContents())
      web_contents_ = portal.GetPortalContents();
    if (auto_attach())
      UpdateAssociatedPages();
  }

  void PortalUpdated() {
    if (auto_attach())
      UpdateAssociatedPages();
  }

 private:
  void UpdateAutoAttach(base::OnceClosure callback) override {
    UpdateAssociatedPages();
    protocol::TargetAutoAttacher::UpdateAutoAttach(std::move(callback));
  }

  void UpdateAssociatedPages() {
    base::flat_set<scoped_refptr<DevToolsAgentHost>> hosts;
    if (auto_attach()) {
      auto* rfh = static_cast<RenderFrameHostImpl*>(
          web_contents_->GetPrimaryMainFrame());
      for (auto* portal : rfh->GetPortals()) {
        WebContentsImpl* wc = portal->GetPortalContents();
        // If the portal's WC is attached, we should get it through normal
        // WC tree traversal. For this loop, we're only interested in the
        // ones that are orphaned.
        if (wc->GetOuterWebContents())
          break;
        hosts.insert(RenderFrameDevToolsAgentHost::GetOrCreateFor(
            wc->GetPrimaryFrameTree().root()));
      }
      web_contents_->ForEachRenderFrameHost(
          [&hosts](RenderFrameHost* rfh) { AddFrameAndPortals(hosts, rfh); });
    }
    DispatchSetAttachedTargetsOfType(hosts, DevToolsAgentHost::kTypePage);
  }

  static void AddFrameAndPortals(
      base::flat_set<scoped_refptr<DevToolsAgentHost>>& hosts,
      RenderFrameHost* rfh) {
    RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
    // We do not expose cached hosts as separate targets for now.
    if (rfhi->IsInBackForwardCache())
      return;
    FrameTreeNode* ftn = rfhi->frame_tree_node();
    // We're interested only in main frames, with the expcetion of fenced frames
    // that are reported as regular subframes via FrameAutoAttacher.
    if (!ftn->IsMainFrame())
      return;
    if (ftn->IsFencedFrameRoot())
      return;
    hosts.insert(RenderFrameDevToolsAgentHost::GetOrCreateFor(ftn));
  }

  WebContents* web_contents_;
};

// static
WebContentsDevToolsAgentHost* WebContentsDevToolsAgentHost::GetFor(
    WebContents* web_contents) {
  return FindAgentHost(web_contents->GetResponsibleWebContents());
}

// static
WebContentsDevToolsAgentHost* WebContentsDevToolsAgentHost::GetOrCreateFor(
    WebContents* web_contents) {
  web_contents = web_contents->GetResponsibleWebContents();
  if (auto* host = FindAgentHost(web_contents))
    return host;
  return new WebContentsDevToolsAgentHost(web_contents);
}

// static
void WebContentsDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    if (ShouldCreateDevToolsAgentHost(wc))
      result->push_back(GetOrCreateFor(wc));
  }
}

WebContentsDevToolsAgentHost::WebContentsDevToolsAgentHost(WebContents* wc)
    : DevToolsAgentHostImpl(base::UnguessableToken::Create().ToString()),
      WebContentsObserver(wc),
      auto_attacher_(std::make_unique<AutoAttacher>(wc)) {
  DCHECK(web_contents());
  bool inserted =
      g_agent_host_instances.Get().insert(std::make_pair(wc, this)).second;
  DCHECK(inserted);
  // Once created, persist till underlying WC is destroyed, so that
  // the target id is retained.
  AddRef();
  NotifyCreated();
}

void WebContentsDevToolsAgentHost::PortalActivated(const Portal& portal) {
  if (web_contents() == portal.GetPortalHostContents()) {
    WebContents* old_wc = web_contents();
    WebContents* new_wc = portal.GetPortalContents();
    // Assure instrumentation calls for the new WC would be routed here.
    DCHECK(new_wc->GetResponsibleWebContents() == new_wc);
    DCHECK(g_agent_host_instances.Get()[old_wc] == this);

    g_agent_host_instances.Get().erase(old_wc);
    g_agent_host_instances.Get()[new_wc] = this;
    Observe(portal.GetPortalContents());
  }
  if (auto_attacher_)
    auto_attacher_->PortalActivated(portal);
}

void WebContentsDevToolsAgentHost::PortalUpdated() {
  if (auto_attacher_)
    auto_attacher_->PortalUpdated();
}

WebContentsDevToolsAgentHost::~WebContentsDevToolsAgentHost() {
  DCHECK(!web_contents());
}

void WebContentsDevToolsAgentHost::DisconnectWebContents() {
  NOTREACHED();
}

void WebContentsDevToolsAgentHost::ConnectWebContents(
    WebContents* web_contents) {
  NOTREACHED();
}

BrowserContext* WebContentsDevToolsAgentHost::GetBrowserContext() {
  return web_contents()->GetBrowserContext();
}

WebContents* WebContentsDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

std::string WebContentsDevToolsAgentHost::GetParentId() {
  return std::string();
}

std::string WebContentsDevToolsAgentHost::GetOpenerId() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerId();
  return "";
}

std::string WebContentsDevToolsAgentHost::GetOpenerFrameId() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetOpenerFrameId();
  return "";
}

bool WebContentsDevToolsAgentHost::CanAccessOpener() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->CanAccessOpener();
  return false;
}

std::string WebContentsDevToolsAgentHost::GetType() {
  return kTypeTab;
}

std::string WebContentsDevToolsAgentHost::GetTitle() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetTitle();
  return "";
}

std::string WebContentsDevToolsAgentHost::GetDescription() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetDescription();
  return "";
}

GURL WebContentsDevToolsAgentHost::GetURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetURL();
  return GURL();
}

GURL WebContentsDevToolsAgentHost::GetFaviconURL() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetFaviconURL();
  return GURL();
}

bool WebContentsDevToolsAgentHost::Activate() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->Activate();
  return false;
}

void WebContentsDevToolsAgentHost::Reload() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    host->Reload();
}

bool WebContentsDevToolsAgentHost::Close() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->Close();
  return false;
}

base::TimeTicks WebContentsDevToolsAgentHost::GetLastActivityTime() {
  if (DevToolsAgentHost* host = GetPrimaryFrameAgent())
    return host->GetLastActivityTime();
  return base::TimeTicks();
}

absl::optional<network::CrossOriginEmbedderPolicy>
WebContentsDevToolsAgentHost::cross_origin_embedder_policy(
    const std::string& id) {
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<network::CrossOriginOpenerPolicy>
WebContentsDevToolsAgentHost::cross_origin_opener_policy(
    const std::string& id) {
  NOTREACHED();
  return absl::nullopt;
}

DevToolsAgentHostImpl* WebContentsDevToolsAgentHost::GetPrimaryFrameAgent() {
  if (WebContents* wc = web_contents()) {
    return RenderFrameDevToolsAgentHost::GetFor(
        static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame()));
  }
  return nullptr;
}

void WebContentsDevToolsAgentHost::WebContentsDestroyed() {
  DCHECK_EQ(this, FindAgentHost(web_contents()));
  ForceDetachAllSessions();
  auto_attacher_.reset();
  g_agent_host_instances.Get().erase(web_contents());
  Observe(nullptr);
  // We may or may not be destruced here, depending on embedders
  // potentially retaining references.
  Release();
}

// DevToolsAgentHostImpl overrides.
DevToolsSession::Mode WebContentsDevToolsAgentHost::GetSessionMode() {
  return DevToolsSession::Mode::kSupportsTabTarget;
}

bool WebContentsDevToolsAgentHost::AttachSession(DevToolsSession* session,
                                                 bool acquire_wake_lock) {
  // TODO(caseq): figure out if this can be a CHECK().
  if (!web_contents())
    return false;
  const bool may_attach_to_brower = session->GetClient()->IsTrusted();
  session->CreateAndAddHandler<protocol::TargetHandler>(
      may_attach_to_brower
          ? protocol::TargetHandler::AccessMode::kRegular
          : protocol::TargetHandler::AccessMode::kAutoAttachOnly,
      GetId(), auto_attacher_.get(), session);
  return true;
}

protocol::TargetAutoAttacher* WebContentsDevToolsAgentHost::auto_attacher() {
  DCHECK(!!auto_attacher_ == !!web_contents());
  return auto_attacher_.get();
}

}  // namespace content
