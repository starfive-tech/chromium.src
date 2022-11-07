// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/embedder/embedder.h"

#include <stdint.h>
#include <atomic>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/core/channel.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/core_ipcz.h"
#include "mojo/core/embedder/features.h"
#include "mojo/core/entrypoints.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/base_shared_memory_service.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/core/node_controller.h"
#include "mojo/public/c/system/thunks.h"

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "mojo/core/channel_linux.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_NACL)

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace mojo {
namespace core {

namespace {

std::atomic<bool> g_mojo_ipcz_enabled{false};

}  // namespace

// InitFeatures will be called as soon as the base::FeatureList is initialized.
void InitFeatures() {
  CHECK(base::FeatureList::GetInstance());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)
  Channel::set_posix_use_writev(
      base::FeatureList::IsEnabled(kMojoPosixUseWritev));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  bool shared_mem_enabled =
      base::FeatureList::IsEnabled(kMojoLinuxChannelSharedMem);
  bool use_zero_on_wake = kMojoLinuxChannelSharedMemEfdZeroOnWake.Get();
  int num_pages = kMojoLinuxChannelSharedMemPages.Get();
  if (num_pages < 0) {
    num_pages = 4;
  } else if (num_pages > 128) {
    num_pages = 128;
  }

  ChannelLinux::SetSharedMemParameters(shared_mem_enabled,
                                       static_cast<unsigned int>(num_pages),
                                       use_zero_on_wake);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_MAC)

  Channel::set_use_trivial_messages(
      base::FeatureList::IsEnabled(kMojoInlineMessagePayloads));

  Core::set_avoid_random_pipe_id(
      base::FeatureList::IsEnabled(kMojoAvoidRandomPipeId));

#if BUILDFLAG(IS_WIN)
  // TODO(https://crbug.com/1299283): Sandboxed processes on Windows versions
  // older than 8.1 require some extra (not yet implemented) setup for ipcz to
  // work properly. This is omitted for early experimentation.
  const bool kIsIpczSupported =
      base::win::GetVersion() >= base::win::Version::WIN8_1;
#else
  const bool kIsIpczSupported = true;
#endif
  if (base::FeatureList::IsEnabled(kMojoIpcz) && kIsIpczSupported) {
    g_mojo_ipcz_enabled.store(true, std::memory_order_release);
  }
}

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;
  if (IsMojoIpczEnabled()) {
    CHECK(InitializeIpczNodeForProcess({
        .is_broker = configuration.is_broker_process,
        .use_local_shared_memory_allocation =
            configuration.is_broker_process ||
            configuration.force_direct_shared_memory_allocation,
    }));
    MojoEmbedderSetSystemThunks(GetMojoIpczImpl());
  } else {
    InitializeCore();
    MojoEmbedderSetSystemThunks(&GetSystemThunks());
  }
}

void Init() {
  Init(Configuration());
}

void ShutDown() {
  if (IsMojoIpczEnabled()) {
    DestroyIpczNodeForProcess();
  } else {
    ShutDownCore();
  }
}

scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
  if (IsMojoIpczEnabled()) {
    return ipcz_driver::Transport::GetIOTaskRunner();
  } else {
    return Core::Get()->GetNodeController()->io_task_runner();
  }
}

bool IsMojoIpczEnabled() {
  // Because Mojo and FeatureList are both brought up early in many binaries, it
  // can be tricky to ensure there aren't races that would lead to two different
  // Mojo implementations being selected at different points throughout the
  // process's lifetime. We cache the result of the first atomic load of this
  // flag; but we also DCHECK that any subsequent loads would match the cached
  // value, as a way to detect initialization races.
  static bool enabled = g_mojo_ipcz_enabled.load(std::memory_order_acquire);
  DCHECK_EQ(enabled, g_mojo_ipcz_enabled.load(std::memory_order_acquire));
  return enabled;
}

void InstallMojoIpczBaseSharedMemoryHooks() {
  DCHECK(IsMojoIpczEnabled());
  mojo::core::ipcz_driver::BaseSharedMemoryService::InstallHooks();
}

}  // namespace core
}  // namespace mojo
