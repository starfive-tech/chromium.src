// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
#define SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/job.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

class BrokerServicesBase;
class Dispatcher;
class LowLevelPolicy;
class PolicyDiagnostic;
class TargetProcess;
struct PolicyGlobal;

// The members of this class are shared between multiple sandbox::PolicyBase
// objects and must be safe for access from multiple threads once created.
// When shared members will not be destroyed until BrokerServicesBase is
// destroyed at process shutdown.
class ConfigBase final : public TargetConfig {
 public:
  ConfigBase() noexcept;
  ~ConfigBase() override;

  ConfigBase(const ConfigBase&) = delete;
  ConfigBase& operator=(const ConfigBase&) = delete;

  bool IsConfigured() const override;

  ResultCode AddRule(SubSystem subsystem,
                     Semantics semantics,
                     const wchar_t* pattern) override;
  ResultCode AddDllToUnload(const wchar_t* dll_name) override;

 private:
  // Can call Freeze()
  friend class BrokerServicesBase;
  // Can examine private fields.
  friend class PolicyDiagnostic;
  // Can call private accessors.
  friend class PolicyBase;

  // Promise that no further changes will be made to the configuration, and
  // this object can be reused by multiple policies.
  bool Freeze();

  // Use in DCHECK only - returns `true` in non-DCHECK builds.
  bool IsOnCreatingThread() const;

#if DCHECK_IS_ON()
  // Used to sequence-check in DCHECK builds.
  uint32_t creating_thread_id_;
#endif  // DCHECK_IS_ON()

  // Once true the configuration is frozen and can be applied to later policies.
  bool configured_ = false;

  ResultCode AddRuleInternal(SubSystem subsystem,
                             Semantics semantics,
                             const wchar_t* pattern);

  // Should only be called once the object is configured.
  PolicyGlobal* policy();
  std::vector<std::wstring>& blocklisted_dlls();

  // Object in charge of generating the low level policy. Will be reset() when
  // Freeze() is called.
  std::unique_ptr<LowLevelPolicy> policy_maker_;
  // Memory structure that stores the low level policy rules for proxied calls.
  raw_ptr<PolicyGlobal> policy_;
  // The list of dlls to unload in the target process.
  std::vector<std::wstring> blocklisted_dlls_;
};

class PolicyBase final : public TargetPolicy {
 public:
  PolicyBase(base::StringPiece key);
  ~PolicyBase() override;

  PolicyBase(const PolicyBase&) = delete;
  PolicyBase& operator=(const PolicyBase&) = delete;

  // TargetPolicy:
  TargetConfig* GetConfig() override;
  ResultCode SetTokenLevel(TokenLevel initial, TokenLevel lockdown) override;
  TokenLevel GetInitialTokenLevel() const override;
  TokenLevel GetLockdownTokenLevel() const override;
  ResultCode SetJobLevel(JobLevel job_level, uint32_t ui_exceptions) override;
  JobLevel GetJobLevel() const override;
  ResultCode SetJobMemoryLimit(size_t memory_limit) override;
  ResultCode SetAlternateDesktop(bool alternate_winstation) override;
  std::wstring GetAlternateDesktop() const override;
  ResultCode CreateAlternateDesktop(bool alternate_winstation) override;
  void DestroyAlternateDesktop() override;
  ResultCode SetIntegrityLevel(IntegrityLevel integrity_level) override;
  IntegrityLevel GetIntegrityLevel() const override;
  ResultCode SetDelayedIntegrityLevel(IntegrityLevel integrity_level) override;
  ResultCode SetLowBox(const wchar_t* sid) override;
  ResultCode SetProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetProcessMitigations() override;
  ResultCode SetDelayedProcessMitigations(MitigationFlags flags) override;
  MitigationFlags GetDelayedProcessMitigations() const override;
  ResultCode SetDisconnectCsrss() override;
  void SetStrictInterceptions() override;
  ResultCode SetStdoutHandle(HANDLE handle) override;
  ResultCode SetStderrHandle(HANDLE handle) override;
  ResultCode AddKernelObjectToClose(const wchar_t* handle_type,
                                    const wchar_t* handle_name) override;
  void AddHandleToShare(HANDLE handle) override;
  void SetLockdownDefaultDacl() override;
  void AddRestrictingRandomSid() override;
  ResultCode AddAppContainerProfile(const wchar_t* package_name,
                                    bool create_profile) override;
  scoped_refptr<AppContainer> GetAppContainer() override;
  void SetEffectiveToken(HANDLE token) override;
  void SetAllowNoSandboxJob() override;
  bool GetAllowNoSandboxJob() override;

  // Creates a Job object with the level specified in a previous call to
  // SetJobLevel().
  ResultCode InitJob();

  // Returns the handle for this policy's job, or INVALID_HANDLE_VALUE if the
  // job is not initialized.
  HANDLE GetJobHandle();

  // Returns true if a job is associated with this policy.
  bool HasJob();

  // Updates the active process limit on the policy's job to zero.
  // Has no effect if the job is allowed to spawn processes.
  ResultCode DropActiveProcessLimit();

  // Creates the two tokens with the levels specified in a previous call to
  // SetTokenLevel(). Also creates a lowbox token if specified based on the
  // lowbox SID.
  ResultCode MakeTokens(base::win::ScopedHandle* initial,
                        base::win::ScopedHandle* lockdown,
                        base::win::ScopedHandle* lowbox);

  // Applies the sandbox to |target| and takes ownership. Internally a
  // call to TargetProcess::Init() is issued.
  ResultCode ApplyToTarget(std::unique_ptr<TargetProcess> target);

  // Called when there are no more active processes in the policy's Job.
  // If a process is not in a job, call OnProcessFinished().
  bool OnJobEmpty();

  // Called when a process no longer needs to be tracked. Processes in jobs
  // should be notified via OnJobEmpty instead.
  bool OnProcessFinished(DWORD process_id);

  EvalResult EvalPolicy(IpcTag service, CountedParameterSetBase* params);

  HANDLE GetStdoutHandle();
  HANDLE GetStderrHandle();

  // Returns the list of handles being shared with the target process.
  const base::HandlesToInheritVector& GetHandlesBeingShared();

 private:
  // BrokerServicesBase is allowed to set shared backing fields for FixedPolicy.
  friend class sandbox::BrokerServicesBase;
  // Allow PolicyDiagnostic to snapshot PolicyBase for diagnostics.
  friend class PolicyDiagnostic;

  // Sets up interceptions for a new target. This policy must own |target|.
  ResultCode SetupAllInterceptions(TargetProcess& target);

  // Sets up the handle closer for a new target. This policy must own |target|.
  bool SetupHandleCloser(TargetProcess& target);

  // TargetConfig will really be a ConfigBase.
  bool SetConfig(TargetConfig* config);

  // Gets possibly shared data or allocates if it did not already exist.
  ConfigBase* config();
  // Tag provided when this policy was created - mainly for debugging.
  std::string tag_;
  // Backing data if this object was created with an empty tag_.
  std::unique_ptr<ConfigBase> config_;
  // Shared backing data if this object will share fields with other policies.
  raw_ptr<ConfigBase> config_ptr_;

  // Remaining members are unique to this instance and will be configured every
  // time.

  // The policy takes ownership of a target as it is applied to it.
  std::unique_ptr<TargetProcess> target_;
  // The user-defined global policy settings.
  TokenLevel lockdown_level_;
  TokenLevel initial_level_;
  JobLevel job_level_;
  uint32_t ui_exceptions_;
  size_t memory_limit_;
  bool use_alternate_desktop_;
  bool use_alternate_winstation_;
  bool relaxed_interceptions_;
  HANDLE stdout_handle_;
  HANDLE stderr_handle_;
  IntegrityLevel integrity_level_;
  IntegrityLevel delayed_integrity_level_;
  MitigationFlags mitigations_;
  MitigationFlags delayed_mitigations_;
  bool is_csrss_connected_;
  // This is a map of handle-types to names that we need to close in the
  // target process. A null set means we need to close all handles of the
  // given type.
  HandleCloser handle_closer_;
  std::unique_ptr<Dispatcher> dispatcher_;
  bool lockdown_default_dacl_;
  bool add_restricting_random_sid_;

  static HDESK alternate_desktop_handle_;
  static HWINSTA alternate_winstation_handle_;
  static HDESK alternate_desktop_local_winstation_handle_;
  static IntegrityLevel alternate_desktop_integrity_level_label_;
  static IntegrityLevel
      alternate_desktop_local_winstation_integrity_level_label_;

  // Contains the list of handles being shared with the target process.
  // This list contains handles other than the stderr/stdout handles which are
  // shared with the target at times.
  base::HandlesToInheritVector handles_to_share_;

  scoped_refptr<AppContainerBase> app_container_;

  HANDLE effective_token_;
  bool allow_no_sandbox_job_;
  Job job_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_POLICY_BASE_H_
