// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CacheStorageBlobClientList;
class MultiCacheQueryOptions;
class ScriptState;

class CacheStorage final : public ScriptWrappable,
                           public ActiveScriptWrappable<CacheStorage>,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CacheStorage(ExecutionContext*, GlobalFetch::ScopedFetcher*);

  CacheStorage(const CacheStorage&) = delete;
  CacheStorage& operator=(const CacheStorage&) = delete;

  ~CacheStorage() override;

  ScriptPromise open(ScriptState*, const String& cache_name);
  ScriptPromise has(ScriptState*, const String& cache_name);
  ScriptPromise Delete(ScriptState*, const String& cache_name);
  ScriptPromise keys(ScriptState*);
  ScriptPromise match(ScriptState* script_state,
                      const V8RequestInfo* request,
                      const MultiCacheQueryOptions* options,
                      ExceptionState& exception_state);

  bool HasPendingActivity() const override;
  void Trace(Visitor*) const override;

 private:
  void MaybeInit();

  // The callback passed into IsCacheStorageAllowed is invoked upon success,
  // and the resolver is rejected upon failure.
  void IsCacheStorageAllowed(ExecutionContext* context,
                             ScriptPromiseResolver* resolver,
                             base::OnceCallback<void()> callback);
  void OnCacheStorageAllowed(ScriptPromiseResolver* resolver,
                             base::OnceCallback<void()> callback,
                             bool allow_access);

  void OpenImpl(ScriptPromiseResolver* resolver,
                const String& cache_name,
                int64_t trace_id);
  void HasImpl(ScriptPromiseResolver* resolver,
               const String& cache_name,
               int64_t trace_id);
  void DeleteImpl(ScriptPromiseResolver* resolver,
                  const String& cache_name,
                  int64_t trace_id);
  void KeysImpl(ScriptPromiseResolver* resolver, int64_t trace_id);
  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const MultiCacheQueryOptions*);
  void MatchImplHelper(ScriptPromiseResolver* resolver,
                       const MultiCacheQueryOptions* options,
                       mojom::blink::FetchAPIRequestPtr mojo_request,
                       mojom::blink::MultiCacheQueryOptionsPtr mojo_options,
                       bool in_related_fetch_event,
                       bool in_range_fetch_event,
                       int64_t trace_id);

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  Member<CacheStorageBlobClientList> blob_client_list_;

  HeapMojoRemote<mojom::blink::CacheStorage> cache_storage_remote_;
  absl::optional<bool> allowed_;
  bool ever_used_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
