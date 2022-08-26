// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/agent_scheduling_group.h"

#include <map>
#include <utility>

#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/shared_storage_worklet_service.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/services/shared_storage_worklet/shared_storage_worklet_service_impl.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"

namespace content {

using ::IPC::ChannelMojo;
using ::IPC::Listener;
using ::IPC::SyncChannel;
using ::mojo::AssociatedReceiver;
using ::mojo::AssociatedRemote;
using ::mojo::PendingAssociatedReceiver;
using ::mojo::PendingAssociatedRemote;
using ::mojo::PendingReceiver;
using ::mojo::PendingRemote;
using ::mojo::Receiver;
using ::mojo::Remote;

using PassKey = ::base::PassKey<AgentSchedulingGroup>;

namespace {

RenderThreadImpl& ToImpl(RenderThread& render_thread) {
  DCHECK(RenderThreadImpl::current());
  return static_cast<RenderThreadImpl&>(render_thread);
}

static features::MBIMode GetMBIMode() {
  return base::FeatureList::IsEnabled(features::kMBIMode)
             ? features::kMBIModeParam.Get()
             : features::MBIMode::kLegacy;
}

// Blink inappropriately makes decisions if there is a WebViewClient set,
// so currently we need to always create a WebViewClient.
class SelfOwnedWebViewClient : public blink::WebViewClient {
 public:
  void OnDestruct() override { delete this; }
};

// A thread for running shared storage worklet operations. It hosts a worklet
// environment belonging to one Document. The object owns itself, cleaning up
// when the worklet has shut down.
class SelfOwnedSharedStorageWorkletThread {
 public:
  SelfOwnedSharedStorageWorkletThread(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      mojo::PendingReceiver<
          shared_storage_worklet::mojom::SharedStorageWorkletService> receiver)
      : main_thread_runner_(std::move(main_thread_runner)) {
    DCHECK(main_thread_runner_->BelongsToCurrentThread());

    auto disconnect_handler = base::BindPostTask(
        main_thread_runner_,
        base::BindOnce(&SelfOwnedSharedStorageWorkletThread::
                           OnSharedStorageWorkletServiceDestroyed,
                       weak_factory_.GetWeakPtr()));

    auto task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);

    // Initialize the worklet service in a new thread.
    worklet_thread_ = base::SequenceBound<
        shared_storage_worklet::SharedStorageWorkletServiceImpl>(
        task_runner, std::move(receiver), std::move(disconnect_handler));
  }

 private:
  void OnSharedStorageWorkletServiceDestroyed() {
    DCHECK(main_thread_runner_->BelongsToCurrentThread());
    worklet_thread_.Reset();
    delete this;
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  base::SequenceBound<shared_storage_worklet::SharedStorageWorkletServiceImpl>
      worklet_thread_;

  base::WeakPtrFactory<SelfOwnedSharedStorageWorkletThread> weak_factory_{this};
};

}  // namespace

AgentSchedulingGroup::ReceiverData::ReceiverData(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface> receiver)
    : name(std::move(name)), receiver(std::move(receiver)) {}

AgentSchedulingGroup::ReceiverData::ReceiverData(ReceiverData&& other)
    : name(std::move(other.name)), receiver(std::move(other.receiver)) {}

AgentSchedulingGroup::ReceiverData::~ReceiverData() = default;

// AgentSchedulingGroup:
AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              ->CreateAgentGroupScheduler()),
      render_thread_(render_thread),
      // `receiver_` will be bound by `OnAssociatedInterfaceRequest()`.
      receiver_(this) {
  DCHECK(agent_group_scheduler_);
  DCHECK_NE(GetMBIMode(), features::MBIMode::kLegacy);

  agent_group_scheduler_->BindInterfaceBroker(std::move(broker_remote));

  channel_ = SyncChannel::Create(
      /*listener=*/this, /*ipc_task_runner=*/render_thread_.GetIOTaskRunner(),
      /*listener_task_runner=*/agent_group_scheduler_->DefaultTaskRunner(),
      render_thread_.GetShutdownEvent());

  // TODO(crbug.com/1111231): Add necessary filters.
  // Currently, the renderer process has these filters:
  // 1. `UnfreezableMessageFilter` - in the process of being removed,
  // 2. `PnaclTranslationResourceHost` - NaCl is going away, and
  // 3. `AutomationMessageFilter` - needs to be handled somehow.

  channel_->Init(
      ChannelMojo::CreateClientFactory(
          bootstrap.PassPipe(),
          /*ipc_task_runner=*/render_thread_.GetIOTaskRunner(),
          /*proxy_task_runner=*/agent_group_scheduler_->DefaultTaskRunner()),
      /*create_pipe_now=*/true);
}

AgentSchedulingGroup::AgentSchedulingGroup(
    RenderThread& render_thread,
    PendingAssociatedReceiver<mojom::AgentSchedulingGroup> receiver,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote)
    : agent_group_scheduler_(
          blink::scheduler::WebThreadScheduler::MainThreadScheduler()
              ->CreateAgentGroupScheduler()),
      render_thread_(render_thread),
      receiver_(this,
                std::move(receiver),
                agent_group_scheduler_->DefaultTaskRunner()) {
  DCHECK(agent_group_scheduler_);
  DCHECK_EQ(GetMBIMode(), features::MBIMode::kLegacy);
  agent_group_scheduler_->BindInterfaceBroker(std::move(broker_remote));
}

AgentSchedulingGroup::~AgentSchedulingGroup() = default;

bool AgentSchedulingGroup::OnMessageReceived(const IPC::Message& message) {
  DCHECK_NE(message.routing_id(), MSG_ROUTING_CONTROL);

  auto* listener = GetListener(message.routing_id());
  if (!listener)
    return false;

  return listener->OnMessageReceived(message);
}

void AgentSchedulingGroup::OnBadMessageReceived(const IPC::Message& message) {
  // Not strictly required, since we don't currently do anything with bad
  // messages in the renderer, but if we ever do then this will "just work".
  return ToImpl(render_thread_).OnBadMessageReceived(message);
}

void AgentSchedulingGroup::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  // The ASG's channel should only be used to bootstrap the ASG mojo interface.
  DCHECK_EQ(interface_name, mojom::AgentSchedulingGroup::Name_);
  DCHECK(!receiver_.is_bound());

  PendingAssociatedReceiver<mojom::AgentSchedulingGroup> pending_receiver(
      std::move(handle));
  receiver_.Bind(std::move(pending_receiver),
                 agent_group_scheduler_->DefaultTaskRunner());
}

bool AgentSchedulingGroup::Send(IPC::Message* message) {
  std::unique_ptr<IPC::Message> msg(message);

  if (GetMBIMode() == features::MBIMode::kLegacy)
    return render_thread_.Send(msg.release());

  // This DCHECK is too idealistic for now - messages that are handled by
  // filters are sent control messages since they are intercepted before
  // routing. It is put here as documentation for now, since this code would not
  // be reached until we activate
  // `features::MBIMode::kEnabledPerRenderProcessHost` or
  // `features::MBIMode::kEnabledPerSiteInstance`.
  DCHECK_NE(message->routing_id(), MSG_ROUTING_CONTROL);

  DCHECK(channel_);
  return channel_->Send(msg.release());
}

void AgentSchedulingGroup::AddRoute(int32_t routing_id, Listener* listener) {
  DCHECK(!listener_map_.Lookup(routing_id));
  listener_map_.AddWithID(listener, routing_id);
  render_thread_.AddRoute(routing_id, listener);

  // See warning in `GetAssociatedInterface`.
  // Replay any `GetAssociatedInterface` calls for this route.
  auto range = pending_receivers_.equal_range(routing_id);
  for (auto iter = range.first; iter != range.second; ++iter) {
    ReceiverData& data = iter->second;
    listener->OnAssociatedInterfaceRequest(data.name,
                                           data.receiver.PassHandle());
  }
  pending_receivers_.erase(range.first, range.second);
}

void AgentSchedulingGroup::AddFrameRoute(
    int32_t routing_id,
    IPC::Listener* listener,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  AddRoute(routing_id, listener);
  render_thread_.AttachTaskRunnerToRoute(routing_id, std::move(task_runner));
}

void AgentSchedulingGroup::RemoveRoute(int32_t routing_id) {
  DCHECK(listener_map_.Lookup(routing_id));
  listener_map_.Remove(routing_id);
  render_thread_.RemoveRoute(routing_id);
}

void AgentSchedulingGroup::DidUnloadRenderFrame(
    const blink::LocalFrameToken& frame_token) {
  host_remote_->DidUnloadRenderFrame(frame_token);
}

void AgentSchedulingGroup::CreateView(mojom::CreateViewParamsPtr params) {
  RenderThreadImpl& renderer = ToImpl(render_thread_);
  renderer.SetScrollAnimatorEnabled(
      params->web_preferences.enable_scroll_animator, PassKey());

  CreateWebView(std::move(params),
                /*was_created_by_renderer=*/false);
}

blink::WebView* AgentSchedulingGroup::CreateWebView(
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer) {
  DCHECK(RenderThread::IsMainThread());

  blink::WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame =
        blink::WebFrame::FromFrameToken(params->opener_frame_token.value());

  blink::WebView* web_view = blink::WebView::Create(
      new SelfOwnedWebViewClient(), params->hidden, params->is_prerendering,
      params->type == mojom::ViewWidgetType::kPortal ? true : false,
      params->type == mojom::ViewWidgetType::kFencedFrame
          ? absl::make_optional(params->fenced_frame_mode)
          : absl::nullopt,
      /*compositing_enabled=*/true, params->never_composited,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast), agent_group_scheduler(),
      params->session_storage_namespace_id, params->base_background_color);

  bool local_main_frame = params->main_frame->is_local_params();

  web_view->SetRendererPreferences(params->renderer_preferences);
  web_view->SetWebPreferences(params->web_preferences);

  if (local_main_frame) {
    RenderFrameImpl::CreateMainFrame(
        *this, web_view, opener_frame,
        /*is_for_nested_main_frame=*/params->type !=
            mojom::ViewWidgetType::kTopLevel,
        /*is_for_scalable_page=*/params->type !=
            mojom::ViewWidgetType::kFencedFrame,
        std::move(params->replication_state), params->devtools_main_frame_token,
        std::move(params->main_frame->get_local_params()));
  } else {
    blink::WebRemoteFrame::CreateMainFrame(
        web_view, params->main_frame->get_remote_params()->token,
        params->devtools_main_frame_token, opener_frame,
        std::move(params->main_frame->get_remote_params()
                      ->frame_interfaces->frame_host),
        std::move(params->main_frame->get_remote_params()
                      ->frame_interfaces->frame_receiver),
        std::move(params->replication_state));
    // Root frame proxy has no ancestors to point to their RenderWidget.

    // The WebRemoteFrame created here was already attached to the Page as its
    // main frame, so we can call WebView's DidAttachRemoteMainFrame().
    web_view->DidAttachRemoteMainFrame(
        std::move(params->main_frame->get_remote_params()
                      ->main_frame_interfaces->main_frame_host),
        std::move(params->main_frame->get_remote_params()
                      ->main_frame_interfaces->main_frame));
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_opened_by_another_window)
    web_view->SetOpenedByDOM();

  GetContentClient()->renderer()->WebViewCreated(web_view,
                                                 was_created_by_renderer);
  return web_view;
}

void AgentSchedulingGroup::CreateFrame(mojom::CreateFrameParamsPtr params) {
  RenderFrameImpl::CreateFrame(
      *this, params->token, params->routing_id, std::move(params->frame),
      std::move(params->interface_broker),
      std::move(params->associated_interface_provider_remote),
      params->previous_frame_token, params->opener_frame_token,
      params->parent_frame_token, params->previous_sibling_frame_token,
      params->devtools_frame_token, params->tree_scope_type,
      std::move(params->replication_state), std::move(params->widget_params),
      std::move(params->frame_owner_properties),
      params->is_on_initial_empty_document,
      std::move(params->policy_container));
}

void AgentSchedulingGroup::CreateSharedStorageWorkletService(
    mojo::PendingReceiver<
        shared_storage_worklet::mojom::SharedStorageWorkletService> receiver) {
  new SelfOwnedSharedStorageWorkletThread(
      agent_group_scheduler_->DefaultTaskRunner(), std::move(receiver));
}

void AgentSchedulingGroup::BindAssociatedInterfaces(
    mojo::PendingAssociatedRemote<mojom::AgentSchedulingGroupHost> remote_host,
    mojo::PendingAssociatedReceiver<mojom::RouteProvider>
        route_provider_receiever) {
  host_remote_.Bind(std::move(remote_host),
                    agent_group_scheduler_->DefaultTaskRunner());
  route_provider_receiver_.Bind(std::move(route_provider_receiever),
                                agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(
      this, std::move(receiver), routing_id,
      agent_group_scheduler_->DefaultTaskRunner());
}

void AgentSchedulingGroup::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();

  if (auto* listener = GetListener(routing_id)) {
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
  } else {
    // THIS IS UNSAFE!
    // Associated receivers must be bound immediately or they could drop
    // messages. This is needed short term so the browser side Remote isn't
    // broken even after the corresponding `AddRoute` happens. Browser should
    // avoid calling this before the corresponding `AddRoute`, but this is a
    // short term workaround until that happens.
    pending_receivers_.emplace(routing_id,
                               ReceiverData(name, std::move(receiver)));
  }
}

Listener* AgentSchedulingGroup::GetListener(int32_t routing_id) {
  DCHECK_NE(routing_id, MSG_ROUTING_CONTROL);

  return listener_map_.Lookup(routing_id);
}

}  // namespace content
