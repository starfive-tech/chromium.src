// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/remote_router_link.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "ipcz/box.h"
#include "ipcz/node_link.h"
#include "ipcz/node_link_memory.h"
#include "ipcz/node_messages.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "util/log.h"
#include "util/safe_math.h"

namespace ipcz {

RemoteRouterLink::RemoteRouterLink(Ref<NodeLink> node_link,
                                   SublinkId sublink,
                                   FragmentRef<RouterLinkState> link_state,
                                   LinkType type,
                                   LinkSide side)
    : node_link_(std::move(node_link)),
      sublink_(sublink),
      type_(type),
      side_(side) {
  // Central links must be constructed with a valid RouterLinkState fragment.
  // Other links must not.
  ABSL_ASSERT(type.is_central() == !link_state.is_null());

  if (type.is_central()) {
    SetLinkState(std::move(link_state));
  }
}

RemoteRouterLink::~RemoteRouterLink() = default;

// static
Ref<RemoteRouterLink> RemoteRouterLink::Create(
    Ref<NodeLink> node_link,
    SublinkId sublink,
    FragmentRef<RouterLinkState> link_state,
    LinkType type,
    LinkSide side) {
  return AdoptRef(new RemoteRouterLink(std::move(node_link), sublink,
                                       std::move(link_state), type, side));
}

void RemoteRouterLink::SetLinkState(FragmentRef<RouterLinkState> state) {
  ABSL_ASSERT(type_.is_central());
  ABSL_ASSERT(!state.is_null());

  if (state.is_pending()) {
    // Wait for the fragment's buffer to be mapped locally.
    Ref<NodeLinkMemory> memory = WrapRefCounted(&node_link()->memory());
    FragmentDescriptor descriptor = state.fragment().descriptor();
    memory->WaitForBufferAsync(
        descriptor.buffer_id(),
        [self = WrapRefCounted(this), memory, descriptor] {
          self->SetLinkState(memory->AdoptFragmentRef<RouterLinkState>(
              memory->GetFragment(descriptor)));
        });
    return;
  }

  ABSL_ASSERT(state.is_addressable());

  // SetLinkState() must be called with an addressable fragment only once.
  ABSL_ASSERT(link_state_.load(std::memory_order_acquire) == nullptr);

  // The release when storing `link_state_` is balanced by an acquire in
  // GetLinkState().
  link_state_fragment_ = std::move(state);
  link_state_.store(link_state_fragment_.get(), std::memory_order_release);

  // If this side of the link was already marked stable before the
  // RouterLinkState was available, `side_is_stable_` will be true. In that
  // case, set the stable bit in RouterLinkState immediately. This may unblock
  // some routing work. The acquire here is balanced by a release in
  // MarkSideStable().
  if (side_is_stable_.load(std::memory_order_acquire)) {
    MarkSideStable();
  }
  if (Ref<Router> router = node_link()->GetRouter(sublink_)) {
    router->Flush(Router::kForceProxyBypassAttempt);
  }
}

LinkType RemoteRouterLink::GetType() const {
  return type_;
}

RouterLinkState* RemoteRouterLink::GetLinkState() const {
  return link_state_.load(std::memory_order_acquire);
}

Ref<Router> RemoteRouterLink::GetLocalPeer() {
  return nullptr;
}

RemoteRouterLink* RemoteRouterLink::AsRemoteRouterLink() {
  return this;
}

void RemoteRouterLink::AllocateParcelData(size_t num_bytes,
                                          bool allow_partial,
                                          Parcel& parcel) {
  parcel.AllocateData(num_bytes, allow_partial, &node_link()->memory());
}

void RemoteRouterLink::AcceptParcel(Parcel& parcel) {
  const absl::Span<Ref<APIObject>> objects = parcel.objects_view();

  msg::AcceptParcel accept;
  accept.params().sublink = sublink_;
  accept.params().sequence_number = parcel.sequence_number();

  size_t num_portals = 0;
  absl::InlinedVector<DriverObject, 2> driver_objects;
  bool must_relay_driver_objects = false;
  for (Ref<APIObject>& object : objects) {
    switch (object->object_type()) {
      case APIObject::kPortal:
        ++num_portals;
        break;

      case APIObject::kBox: {
        Box* box = Box::FromObject(object.get());
        ABSL_ASSERT(box);
        if (!box->object().CanTransmitOn(*node_link()->transport())) {
          must_relay_driver_objects = true;
        }
        driver_objects.push_back(std::move(box->object()));
        break;
      }

      default:
        break;
    }
  }

  // If driver objects will require relaying through the broker, then the parcel
  // must be split into two separate messages: one for the driver objects (which
  // will be relayed), and one for the rest of the message (which will transmit
  // directly).
  //
  // This ensures that many side effects of message receipt are well-ordered
  // with other transmissions on the same link from the same thread. Namely,
  // since a thread may send a message which introduces a new remote Router on a
  // new sublink, followed immediately by a message which targets that Router,
  // it is critical that both messages arrive in the order they were sent. If
  // one of the messages is relayed while the other is not, ordering could not
  // be guaranteed.
  const bool must_split_parcel = must_relay_driver_objects;

  // Allocate all the arrays in the message. Note that each allocation may
  // relocate the parcel data in memory, so views into these arrays should not
  // be acquired until all allocations are complete.
  if (parcel.data_fragment().is_null() ||
      parcel.data_fragment_memory() != &node_link()->memory()) {
    // Only inline parcel data within the message when we don't have a separate
    // data fragment allocated already, or if the allocated fragment is on the
    // wrong link. The latter case is possible if the transmitting Router
    // switched links since the Parcel's data was allocated.
    accept.params().parcel_data =
        accept.AllocateArray<uint8_t>(parcel.data_view().size());
  } else {
    // The data for this parcel already exists in this link's memory, so we only
    // stash a reference to it in the message. This relinquishes ownership of
    // the fragment, effectively passing it to the recipient.
    accept.params().parcel_fragment = parcel.data_fragment().descriptor();
    parcel.ReleaseDataFragment();
  }
  accept.params().handle_types =
      accept.AllocateArray<HandleType>(objects.size());
  accept.params().new_routers =
      accept.AllocateArray<RouterDescriptor>(num_portals);

  const absl::Span<uint8_t> inline_parcel_data =
      accept.GetArrayView<uint8_t>(accept.params().parcel_data);
  const absl::Span<HandleType> handle_types =
      accept.GetArrayView<HandleType>(accept.params().handle_types);
  const absl::Span<RouterDescriptor> new_routers =
      accept.GetArrayView<RouterDescriptor>(accept.params().new_routers);

  if (!inline_parcel_data.empty()) {
    memcpy(inline_parcel_data.data(), parcel.data_view().data(),
           parcel.data_size());
  }

  // Serialize attached objects. We accumulate the Routers of all attached
  // portals, because we need to reference them again after transmission, with
  // a 1:1 correspondence to the serialized RouterDescriptors.
  absl::InlinedVector<Ref<Router>, 4> routers_to_proxy;
  for (size_t i = 0; i < objects.size(); ++i) {
    APIObject& object = *objects[i];

    switch (object.object_type()) {
      case APIObject::kPortal: {
        handle_types[i] = HandleType::kPortal;

        Ref<Router> router = Portal::FromObject(&object)->router();
        router->SerializeNewRouter(*node_link(), new_routers[i]);
        routers_to_proxy.push_back(std::move(router));
        break;
      }

      case APIObject::kBox:
        handle_types[i] =
            must_split_parcel ? HandleType::kRelayedBox : HandleType::kBox;
        break;

      default:
        DLOG(FATAL) << "Attempted to transmit an invalid object.";
        break;
    }
  }

  if (must_split_parcel) {
    msg::AcceptParcelDriverObjects accept_objects;
    accept_objects.params().sublink = sublink_;
    accept_objects.params().sequence_number = parcel.sequence_number();
    accept_objects.params().driver_objects =
        accept_objects.AppendDriverObjects(absl::MakeSpan(driver_objects));

    DVLOG(4) << "Transmitting objects for " << parcel.Describe() << " over "
             << Describe();
    node_link()->Transmit(accept_objects);
  } else {
    accept.params().driver_objects =
        accept.AppendDriverObjects(absl::MakeSpan(driver_objects));
  }

  DVLOG(4) << "Transmitting " << parcel.Describe() << " over " << Describe();

  node_link()->Transmit(accept);

  // Now that the parcel has been transmitted, it's safe to start proxying from
  // any routers whose routes have just been extended to the destination.
  ABSL_ASSERT(routers_to_proxy.size() == new_routers.size());
  for (size_t i = 0; i < routers_to_proxy.size(); ++i) {
    routers_to_proxy[i]->BeginProxyingToNewRouter(*node_link(), new_routers[i]);
  }

  // Finally, a Parcel will normally close all attached objects when destroyed.
  // Since we've successfully transmitted this parcel and all its objects, we
  // prevent that behavior by taking away all its object references.
  for (Ref<APIObject>& object : objects) {
    Ref<APIObject> released_object = std::move(object);
  }
}

void RemoteRouterLink::AcceptRouteClosure(SequenceNumber sequence_length) {
  msg::RouteClosed route_closed;
  route_closed.params().sublink = sublink_;
  route_closed.params().sequence_length = sequence_length;
  node_link()->Transmit(route_closed);
}

size_t RemoteRouterLink::GetParcelCapacityInBytes(const IpczPutLimits& limits) {
  if (limits.max_queued_bytes == 0 || limits.max_queued_parcels == 0) {
    return 0;
  }

  RouterLinkState* state = GetLinkState();
  if (!state) {
    // This is only a best-effort estimate. With no link state yet, err on the
    // side of more data flow.
    return limits.max_queued_bytes;
  }

  const RouterLinkState::QueueState peer_queue =
      state->GetQueueState(side_.opposite());
  if (peer_queue.num_parcels >= limits.max_queued_parcels ||
      peer_queue.num_bytes >= limits.max_queued_bytes) {
    return 0;
  }

  return limits.max_queued_bytes - peer_queue.num_bytes;
}

RouterLinkState::QueueState RemoteRouterLink::GetPeerQueueState() {
  if (auto* state = GetLinkState()) {
    return state->GetQueueState(side_.opposite());
  }
  return {.num_parcels = 0, .num_bytes = 0};
}

bool RemoteRouterLink::UpdateInboundQueueState(size_t num_parcels,
                                               size_t num_bytes) {
  RouterLinkState* state = GetLinkState();
  return state && state->UpdateQueueState(side_, num_parcels, num_bytes);
}

void RemoteRouterLink::NotifyDataConsumed() {
  msg::NotifyDataConsumed notify;
  notify.params().sublink = sublink_;
  node_link()->Transmit(notify);
}

bool RemoteRouterLink::EnablePeerMonitoring(bool enable) {
  RouterLinkState* state = GetLinkState();
  return state && state->SetSideIsMonitoringPeer(side_, enable);
}

void RemoteRouterLink::AcceptRouteDisconnected() {
  msg::RouteDisconnected route_disconnected;
  route_disconnected.params().sublink = sublink_;
  node_link()->Transmit(route_disconnected);
}

void RemoteRouterLink::MarkSideStable() {
  side_is_stable_.store(true, std::memory_order_release);
  if (RouterLinkState* state = GetLinkState()) {
    state->SetSideStable(side_);
  }
}

bool RemoteRouterLink::TryLockForBypass(const NodeName& bypass_request_source) {
  RouterLinkState* state = GetLinkState();
  if (!state || !state->TryLock(side_)) {
    return false;
  }

  state->allowed_bypass_request_source.StoreRelease(bypass_request_source);
  return true;
}

bool RemoteRouterLink::TryLockForClosure() {
  RouterLinkState* state = GetLinkState();
  return state && state->TryLock(side_);
}

void RemoteRouterLink::Unlock() {
  if (RouterLinkState* state = GetLinkState()) {
    state->Unlock(side_);
  }
}

bool RemoteRouterLink::FlushOtherSideIfWaiting() {
  RouterLinkState* state = GetLinkState();
  if (!state || !state->ResetWaitingBit(side_.opposite())) {
    return false;
  }

  msg::FlushRouter flush;
  flush.params().sublink = sublink_;
  node_link()->Transmit(flush);
  return true;
}

bool RemoteRouterLink::CanNodeRequestBypass(
    const NodeName& bypass_request_source) {
  RouterLinkState* state = GetLinkState();
  if (!state) {
    return false;
  }

  NodeName allowed_source = state->allowed_bypass_request_source.LoadAcquire();
  return state->is_locked_by(side_.opposite()) &&
         allowed_source == bypass_request_source;
}

void RemoteRouterLink::Deactivate() {
  node_link()->RemoveRemoteRouterLink(sublink_);
}

void RemoteRouterLink::BypassPeer(const NodeName& bypass_target_node,
                                  SublinkId bypass_target_sublink) {
  msg::BypassPeer bypass;
  bypass.params().sublink = sublink_;
  bypass.params().reserved0 = 0;
  bypass.params().bypass_target_node = bypass_target_node;
  bypass.params().bypass_target_sublink = bypass_target_sublink;
  node_link()->Transmit(bypass);
}

void RemoteRouterLink::StopProxying(SequenceNumber inbound_sequence_length,
                                    SequenceNumber outbound_sequence_length) {
  msg::StopProxying stop;
  stop.params().sublink = sublink_;
  stop.params().inbound_sequence_length = inbound_sequence_length;
  stop.params().outbound_sequence_length = outbound_sequence_length;
  node_link()->Transmit(stop);
}

void RemoteRouterLink::ProxyWillStop(SequenceNumber inbound_sequence_length) {
  msg::ProxyWillStop will_stop;
  will_stop.params().sublink = sublink_;
  will_stop.params().inbound_sequence_length = inbound_sequence_length;
  node_link()->Transmit(will_stop);
}

void RemoteRouterLink::BypassPeerWithLink(
    SublinkId new_sublink,
    FragmentRef<RouterLinkState> new_link_state,
    SequenceNumber inbound_sequence_length) {
  msg::BypassPeerWithLink bypass;
  bypass.params().sublink = sublink_;
  bypass.params().new_sublink = new_sublink;
  bypass.params().new_link_state_fragment =
      new_link_state.release().descriptor();
  bypass.params().inbound_sequence_length = inbound_sequence_length;
  node_link()->Transmit(bypass);
}

void RemoteRouterLink::StopProxyingToLocalPeer(
    SequenceNumber outbound_sequence_length) {
  msg::StopProxyingToLocalPeer stop;
  stop.params().sublink = sublink_;
  stop.params().outbound_sequence_length = outbound_sequence_length;
  node_link()->Transmit(stop);
}

std::string RemoteRouterLink::Describe() const {
  std::stringstream ss;
  ss << type_.ToString() << " link from "
     << node_link_->local_node_name().ToString() << " to "
     << node_link_->remote_node_name().ToString() << " via sublink "
     << sublink_;
  return ss.str();
}

}  // namespace ipcz
