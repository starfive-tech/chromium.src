// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/unwind_util.h"

#include <memory>
#include <vector>

#include "base/android/library_loader/anchor_functions.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"
#include "components/metrics/call_stack_profile_params.h"

#if BUILDFLAG(IS_ANDROID) && \
    (defined(ARCH_CPU_ARMEL) || BUILDFLAG(ENABLE_ARM_CFI_TABLE))
#include "chrome/android/modules/stack_unwinder/public/module.h"
#endif

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#include "base/android/apk_assets.h"
#include "base/files/memory_mapped_file.h"
#include "base/profiler/arm_cfi_table.h"

extern "C" {
// The address of |__executable_start| is the base address of the executable or
// shared library.
extern char __executable_start;
}

#if BUILDFLAG(USE_ANDROID_UNWINDER_V2)
#include "base/profiler/chrome_unwinder_android_v2.h"
#else
#include "base/profiler/chrome_unwinder_android.h"
#endif
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)

namespace {

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#if BUILDFLAG(USE_ANDROID_UNWINDER_V2)
class ChromeUnwinderCreator {
 public:
  ChromeUnwinderCreator() {
    constexpr char kCfiFileName[] = "assets/unwind_cfi_32_v2";
    base::MemoryMappedFile::Region cfi_region;
    int fd = base::android::OpenApkAsset(kCfiFileName, &cfi_region);
    DCHECK_GE(fd, 0);
    bool mapped_file_ok =
        chrome_cfi_file_.Initialize(base::File(fd), cfi_region);
    DCHECK(mapped_file_ok);
  }
  ChromeUnwinderCreator(const ChromeUnwinderCreator&) = delete;
  ChromeUnwinderCreator& operator=(const ChromeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return std::make_unique<base::ChromeUnwinderAndroidV2>(
        base::CreateChromeUnwindInfoAndroid(
            {chrome_cfi_file_.data(), chrome_cfi_file_.length()}),
        /* chrome_module_base_address= */
        reinterpret_cast<uintptr_t>(&__executable_start),
        /* text_section_start_address= */ base::android::kStartOfText);
  }

 private:
  base::MemoryMappedFile chrome_cfi_file_;
};
#else   // BUILDFLAG(USE_ANDROID_UNWINDER_V2)
// Encapsulates the setup required to create the Chrome unwinder on Android.
class ChromeUnwinderCreator {
 public:
  ChromeUnwinderCreator() {
    constexpr char kCfiFileName[] = "assets/unwind_cfi_32";

    base::MemoryMappedFile::Region cfi_region;
    int fd = base::android::OpenApkAsset(kCfiFileName, &cfi_region);
    DCHECK_GE(fd, 0);
    bool mapped_file_ok =
        chrome_cfi_file_.Initialize(base::File(fd), cfi_region);
    DCHECK(mapped_file_ok);
    chrome_cfi_table_ = base::ArmCFITable::Parse(
        {chrome_cfi_file_.data(), chrome_cfi_file_.length()});
    DCHECK(chrome_cfi_table_);
  }

  ChromeUnwinderCreator(const ChromeUnwinderCreator&) = delete;
  ChromeUnwinderCreator& operator=(const ChromeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return std::make_unique<base::ChromeUnwinderAndroid>(
        chrome_cfi_table_.get(),
        reinterpret_cast<uintptr_t>(&__executable_start));
  }

 private:
  base::MemoryMappedFile chrome_cfi_file_;
  std::unique_ptr<base::ArmCFITable> chrome_cfi_table_;
};
#endif  // BUILDFLAG(USE_ANDROID_UNWINDER_V2)

// Encapsulates the setup required to create the Android native unwinder.
class NativeUnwinderCreator {
 public:
  explicit NativeUnwinderCreator(stack_unwinder::Module* stack_unwinder_module)
      : module_(stack_unwinder_module),
        memory_regions_map_(module_->CreateMemoryRegionsMap()) {}
  NativeUnwinderCreator(const NativeUnwinderCreator&) = delete;
  NativeUnwinderCreator& operator=(const NativeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return module_->CreateNativeUnwinder(
        memory_regions_map_.get(),
        reinterpret_cast<uintptr_t>(&__executable_start));
  }

 private:
  const raw_ptr<stack_unwinder::Module> module_;
  const std::unique_ptr<stack_unwinder::MemoryRegionsMap> memory_regions_map_;
};

std::vector<std::unique_ptr<base::Unwinder>> CreateCoreUnwinders(
    stack_unwinder::Module* const stack_unwinder_module) {
  DCHECK_NE(getpid(), gettid());

  static base::NoDestructor<NativeUnwinderCreator> native_unwinder_creator(
      stack_unwinder_module);
  static base::NoDestructor<ChromeUnwinderCreator> chrome_unwinder_creator;

  // Note order matters: the more general unwinder must appear first in the
  // vector.
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(native_unwinder_creator->Create());
  unwinders.push_back(chrome_unwinder_creator->Create());
  return unwinders;
}
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)

}  // namespace

// static
void UnwindPrerequisites::RequestInstallation() {
  CHECK_EQ(metrics::CallStackProfileParams::Process::kBrowser,
           GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()));
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // The install occurs asynchronously, with the module available at the first
  // run of Chrome following install.
  stack_unwinder::Module::RequestInstallation();
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
}

// static
bool UnwindPrerequisites::Available() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  return stack_unwinder::Module::IsInstalled();
#else
  return true;
#endif
}

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  if (!UnwindPrerequisites::Available()) {
    return base::StackSamplingProfiler::UnwindersFactory();
  }
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
  static base::NoDestructor<std::unique_ptr<stack_unwinder::Module>>
      stack_unwinder_module(stack_unwinder::Module::Load());
  return base::BindOnce(CreateCoreUnwinders, stack_unwinder_module->get());
#else
  return base::StackSamplingProfiler::UnwindersFactory();
#endif
}
