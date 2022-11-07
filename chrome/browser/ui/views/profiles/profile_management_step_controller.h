// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_

#include "base/callback.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "url/gurl.h"

class Profile;
class ProfilePickerDiceSignInProvider;
class ProfilePickerWebContentsHost;
class ProfilePickerSignedInFlowController;

namespace content {
class WebContents;
}

// Represents a portion of a profile management flow.
// The step controller owns data and helpers necessary for the implementation of
// this specific portion. It also implements shared helpers that make it easier
// to combine steps together and navigate between them..
class ProfileManagementStepController {
 public:
  static std::unique_ptr<ProfileManagementStepController>
  CreateForProfilePickerApp(ProfilePickerWebContentsHost* host,
                            const GURL& initial_url);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  static std::unique_ptr<ProfileManagementStepController> CreateForDiceSignIn(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerDiceSignInProvider> dice_sign_in_provider,
      base::OnceCallback<void(Profile* profile,
                              std::unique_ptr<content::WebContents>,
                              bool is_saml)> signed_in_callback);
#endif

  static std::unique_ptr<ProfileManagementStepController>
  CreateForPostSignInFlow(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerSignedInFlowController> signed_in_flow);

  explicit ProfileManagementStepController(ProfilePickerWebContentsHost* host);
  virtual ~ProfileManagementStepController();

  // Attempts to show the current step in the `host_`.
  // `step_shown_callback` will be executed when the attempt is completed, with
  // `true` if it succeeded.
  // `reset_state` indicates that the step should reset its internal state
  // before showing itself.
  virtual void Show(base::OnceCallback<void(bool)> step_shown_callback,
                    bool reset_state = false) = 0;

  // Frees up unneeded resources. `Show()` will be called if it's needed again.
  virtual void OnHidden() = 0;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Method to be called if the user is attempting to reload this step.
  virtual void OnReloadRequested();
#endif

  // Method to be called if the user is attempting to navigate back.
  virtual void OnNavigateBackRequested() = 0;

  void set_pop_step_callback(base::OnceClosure callback) {
    pop_step_callback_ = std::move(callback);
  }

 protected:
  // Returns whether we can navigate back from this step.
  // If it returns true, we expect that a `pop_step_callback_` is set (by
  // calling `set_pop_step_callback()`) before we attempt to navigate back.
  virtual bool CanPopStep() const;

  // Helper to implement back navigations for `OnNavigateBackRequested()`.
  // `contents` is expected to be the `WebContents` in which the current step
  // is rendered, to determine whether some internal history can be popped.
  // If we are allowed to navigate back to the previous step (see
  // `CanPopStep()`), we expect `set_pop_step_callback()` to have been called
  // before.
  void NavigateBackInternal(content::WebContents* contents);

  ProfilePickerWebContentsHost* host() const { return host_; }

 private:
  raw_ptr<ProfilePickerWebContentsHost> host_;

  base::OnceClosure pop_step_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MANAGEMENT_STEP_CONTROLLER_H_
