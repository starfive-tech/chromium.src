// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_observer_manager.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_observer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_pressure_source.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// static
const char PressureObserverManager::kSupplementName[] =
    "PressureObserverManager";

// static
PressureObserverManager* PressureObserverManager::From(LocalDOMWindow& window) {
  PressureObserverManager* manager =
      Supplement<LocalDOMWindow>::From<PressureObserverManager>(window);
  if (!manager) {
    manager = MakeGarbageCollected<PressureObserverManager>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, manager);
  }
  return manager;
}

PressureObserverManager::PressureObserverManager(LocalDOMWindow& window)
    : ExecutionContextLifecycleStateObserver(&window),
      Supplement<LocalDOMWindow>(window),
      pressure_service_(GetSupplementable()->GetExecutionContext()),
      receiver_(this, GetSupplementable()->GetExecutionContext()) {
  UpdateStateIfNeeded();
}

PressureObserverManager::~PressureObserverManager() = default;

ScriptPromise PressureObserverManager::AddObserver(
    V8PressureSource source,
    blink::PressureObserver* observer,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(script_state->ContextIsValid());

  if (IsRegistering(source, observer) || IsRegistered(source, observer))
    return ScriptPromise::CastUndefined(script_state);

  const wtf_size_t source_index = static_cast<wtf_size_t>(source.AsEnum());
  registering_observers_[source_index].insert(observer);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureServiceConnection();
  if (!receiver_.is_bound()) {
    // Not connected to the browser process yet. Make the binding.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    pressure_service_->BindObserver(
        receiver_.BindNewPipeAndPassRemote(std::move(task_runner)),
        resolver->WrapCallbackInScriptScope(WTF::Bind(
            &PressureObserverManager::DidBindObserver, WrapWeakPersistent(this),
            source, WrapPersistent(observer))));
    receiver_.set_disconnect_handler(
        WTF::Bind(&PressureObserverManager::Reset, WrapWeakPersistent(this)));
  } else {
    // Already connected to the browser process, just change the quantization
    // options if necessary.
    auto mojo_options = mojom::blink::PressureQuantization::New(
        observer->normalized_options()->cpuUtilizationThresholds());
    pressure_service_->SetQuantization(
        std::move(mojo_options),
        resolver->WrapCallbackInScriptScope(WTF::Bind(
            &PressureObserverManager::DidSetQuantization,
            WrapWeakPersistent(this), source, WrapPersistent(observer))));
  }
  return resolver->Promise();
}

void PressureObserverManager::RemoveObserver(
    V8PressureSource source,
    blink::PressureObserver* observer) {
  const wtf_size_t source_index = static_cast<wtf_size_t>(source.AsEnum());
  registering_observers_[source_index].erase(observer);
  registered_observers_[source_index].erase(observer);

  // Disconnected from the browser process only when PressureObserverManager is
  // active and there is no other observers.
  if (receiver_.is_bound() && registered_observers_[source_index].IsEmpty() &&
      registering_observers_[source_index].IsEmpty()) {
    // TODO(crbug.com/1342184): Consider other sources.
    // For now, "cpu" is the only source, so disconnect directly.
    receiver_.reset();
  }
}

void PressureObserverManager::RemoveObserverFromAllSources(
    blink::PressureObserver* observer) {
  // TODO(crbug.com/1342184): Consider other sources.
  // For now, "cpu" is the only source.
  auto source = V8PressureSource(V8PressureSource::Enum::kCpu);
  RemoveObserver(source, observer);
}

void PressureObserverManager::ContextDestroyed() {
  Reset();
}

void PressureObserverManager::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  // TODO(https://crbug.com/1186433): Disconnect and re-establish a connection
  // when frozen or send a disconnect event.
}

void PressureObserverManager::OnUpdate(
    device::mojom::blink::PressureStatePtr state) {
  // TODO(crbug.com/1342184): Consider other sources.
  // For now, "cpu" is the only source.
  const wtf_size_t source_index =
      static_cast<wtf_size_t>(V8PressureSource::Enum::kCpu);

  // New observers may be created and added. Take a snapshot so as
  // to safely iterate.
  HeapVector<Member<blink::PressureObserver>> observers;
  CopyToVector(registered_observers_[source_index], observers);
  for (const auto& observer : observers)
    observer->OnUpdate(state.Clone());
}

void PressureObserverManager::Trace(blink::Visitor* visitor) const {
  for (const auto& registering_observers_set : registering_observers_)
    visitor->Trace(registering_observers_set);
  for (const auto& registered_observers_set : registered_observers_)
    visitor->Trace(registered_observers_set);
  visitor->Trace(pressure_service_);
  visitor->Trace(receiver_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void PressureObserverManager::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (pressure_service_.is_bound())
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kUserInteraction);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      pressure_service_.BindNewPipeAndPassReceiver(task_runner));
  pressure_service_.set_disconnect_handler(
      WTF::Bind(&PressureObserverManager::OnServiceConnectionError,
                WrapWeakPersistent(this)));
}

void PressureObserverManager::OnServiceConnectionError() {
  pressure_service_.reset();
  Reset();
}

void PressureObserverManager::Reset() {
  receiver_.reset();
  for (auto& registering_observers_set : registering_observers_)
    registering_observers_set.clear();
  for (auto& registered_observers_set : registered_observers_)
    registered_observers_set.clear();
}

bool PressureObserverManager::IsRegistering(
    V8PressureSource source,
    blink::PressureObserver* observer) const {
  const wtf_size_t source_index = static_cast<wtf_size_t>(source.AsEnum());
  return registering_observers_[source_index].Contains(observer);
}

bool PressureObserverManager::IsRegistered(
    V8PressureSource source,
    blink::PressureObserver* observer) const {
  const wtf_size_t source_index = static_cast<wtf_size_t>(source.AsEnum());
  return registered_observers_[source_index].Contains(observer);
}

void PressureObserverManager::DidBindObserver(
    V8PressureSource source,
    blink::PressureObserver* observer,
    ScriptPromiseResolver* resolver,
    mojom::blink::PressureStatus status) {
  // unobserve/disconnect may be called before this method was called.
  if (!IsRegistering(source, observer)) {
    resolver->Resolve();
    return;
  }

  DCHECK(pressure_service_.is_bound());

  switch (status) {
    case mojom::blink::PressureStatus::kOk: {
      auto mojo_options = mojom::blink::PressureQuantization::New(
          observer->normalized_options()->cpuUtilizationThresholds());
      pressure_service_->SetQuantization(
          std::move(mojo_options),
          resolver->WrapCallbackInScriptScope(WTF::Bind(
              &PressureObserverManager::DidSetQuantization,
              WrapWeakPersistent(this), source, WrapPersistent(observer))));
      break;
    }
    case mojom::blink::PressureStatus::kNotSupported: {
      Reset();
      resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                       "Not available on this platform.");
      break;
    }
    case mojom::blink::PressureStatus::kSecurityError: {
      Reset();
      const char kSecurityError[] =
          "Security error. Make sure the page is visible.";
      resolver->RejectWithSecurityError(kSecurityError, kSecurityError);
      break;
    }
  }
}

void PressureObserverManager::DidSetQuantization(
    V8PressureSource source,
    blink::PressureObserver* observer,
    ScriptPromiseResolver* resolver,
    mojom::blink::SetQuantizationStatus status) {
  // unobserve/disconnect may be called before this method was called.
  if (!IsRegistering(source, observer)) {
    resolver->Resolve();
    return;
  }

  const wtf_size_t source_index = static_cast<wtf_size_t>(source.AsEnum());
  registering_observers_[source_index].erase(observer);

  switch (status) {
    case mojom::blink::SetQuantizationStatus::kChanged:
      // Clear registered observers, then do same steps as kUnchanged.
      registered_observers_[source_index].clear();
      [[fallthrough]];
    case mojom::blink::SetQuantizationStatus::kUnchanged:
      registered_observers_[source_index].insert(observer);
      resolver->Resolve();
      break;
  }
}

}  // namespace blink
