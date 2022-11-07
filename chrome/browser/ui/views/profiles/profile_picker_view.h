// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_host.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ProfilePickerDiceSignInToolbar;
#endif

class Profile;
class ProfileManagementStepController;
class ProfilePickerSignedInFlowController;
class ScopedProfileKeepAlive;

namespace base {
class FilePath;
}

namespace content {
struct ContextMenuParams;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::WidgetDelegateView,
                          public ProfilePickerWebContentsHost {
 public:
  // TODO(https://crbug.com/1358843): Split the steps more granularly across
  // logical steps instead of according to implementation details.
  enum class Step {
    kUnknown,
    // Renders the `chrome://profile-picker` app, covering the profile picker,
    // the profile type choice at the beginning of the profile creation
    // flow and the account selection on Lacros.
    kProfilePicker,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // Renders the sign in screen on Dice platforms.
    // TODO(https://crbug.com/1360773): Support the `kAccountSelection` step on
    // Lacros. Picking an account during the `kLacrosSelectAvailableAccount`
    // flow and the profile creation should be implemented as a standalone step.
    kAccountSelection,
#endif
    // Renders all post-sign in screens: enterprise management consent, profile
    // switch, sync opt-in, etc.
    kPostSignInFlow
  };

  METADATA_HEADER(ProfilePickerView);

  ProfilePickerView(const ProfilePickerView&) = delete;
  ProfilePickerView& operator=(const ProfilePickerView&) = delete;

  // Updates the parameters. This calls existing callbacks with error values,
  // and requires `ProfilePicker::Params::CanReusePickerWindow()` to be true.
  void UpdateParams(ProfilePicker::Params&& params);

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  // ProfilePickerWebContentsHost:
  void ShowScreen(content::WebContents* contents,
                  const GURL& url,
                  base::OnceClosure navigation_finished_closure =
                      base::OnceClosure()) override;
  void ShowScreenInPickerContents(
      const GURL& url,
      base::OnceClosure navigation_finished_closure =
          base::OnceClosure()) override;
  void Clear() override;
  bool ShouldUseDarkColors() const override;
  content::WebContents* GetPickerContents() const override;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void SetNativeToolbarVisible(bool visible) override;
  SkColor GetPreferredBackgroundColor() const override;
#endif

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // web_modal::WebContentsModalDialogHost
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  friend class ProfilePicker;
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CreateForceSignedInProfile);

  // To display the Profile picker, use ProfilePicker::Show().
  explicit ProfilePickerView(ProfilePicker::Params&& params);
  ~ProfilePickerView() override;

  enum State { kNotStarted = 0, kInitializing = 1, kReady = 2, kClosing = 3 };

  class NavigationFinishedObserver : public content::WebContentsObserver {
   public:
    NavigationFinishedObserver(const GURL& url,
                               base::OnceClosure closure,
                               content::WebContents* contents);
    NavigationFinishedObserver(const NavigationFinishedObserver&) = delete;
    NavigationFinishedObserver& operator=(const NavigationFinishedObserver&) =
        delete;
    ~NavigationFinishedObserver() override;

    // content::WebContentsObserver:
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

   private:
    const GURL url_;
    base::OnceClosure closure_;
  };

  // If the picker needs to be re-opened, this function schedules the reopening,
  // closes the picker and return true. Otherwise, it returns false.
  bool MaybeReopen(ProfilePicker::Params& params);

  // Displays the profile picker.
  void Display();

  // On picker profile creation success, it initializes the view.
  void OnPickerProfileCreated(Profile* picker_profile);

  // Creates and shows the dialog.
  void Init(Profile* picker_profile);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Switches the layout to the sign-in screen (and creates a new profile).
  // absl::nullopt `profile_color` corresponds to the default theme.
  void SwitchToDiceSignIn(
      absl::optional<SkColor> profile_color,
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Starts the forced sign-in flow (and creates a new profile).
  // `switch_finished_callback` gets informed whether the creation of the new
  // profile succeeded and the sign-in UI gets displayed.
  void SwitchToForcedSignIn(
      base::OnceCallback<void(bool)> switch_finished_callback);

  // Handles profile creation when forced sign-in is enabled.
  void OnProfileForDiceForcedSigninCreated(
      base::OnceCallback<void(bool)> switch_finished_callback,
      Profile* new_profile);
#endif

  void SwitchToSignedInFlow(Profile* signed_in_profile,
                            std::unique_ptr<content::WebContents> contents,
                            bool is_saml);

  // Cancel the signed-in profile setup and returns back to the main picker
  // screen (if themoriginal EntryPoint was to open the picker).
  void CancelSignedInFlow();

  // views::WidgetDelegate:
  void WindowClosing() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* GetContentsView() override;
  std::u16string GetAccessibleWindowTitle() const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Builds the views hieararchy.
  void BuildLayout();

  void ShowScreenFinished(
      content::WebContents* contents,
      base::OnceClosure navigation_finished_closure = base::OnceClosure());

  void NavigateBack();

  // Register basic keyboard accelerators such as closing the window (Alt-F4
  // on Windows).
  void ConfigureAccelerators();

  // Shows a dialog where the user can auth the profile or see the
  // auth error message. If a dialog is already shown, this destroys the current
  // dialog and creates a new one.
  void ShowDialog(content::BrowserContext* browser_context,
                  const GURL& url,
                  const base::FilePath& profile_path);

  // Hides the dialog if it is showing.
  void HideDialog();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  base::FilePath GetForceSigninProfilePath() const;

  // Getter of the target page url. If not empty and is valid, it opens on
  // profile selection instead of the new tab page.
  GURL GetOnSelectProfileTargetUrl() const;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the user selects an account on the Lacros-specific account
  // selection screen. Only called for existing profiles, not as part of profile
  // creation.
  void NotifyAccountSelected(const std::string& gaia_id);
#endif

  void SwitchToStep(
      Step step,
      bool reset_state = false,
      base::OnceClosure pop_step_callback = base::OnceClosure(),
      base::OnceCallback<void(bool)> step_switch_finished_callback =
          base::OnceCallback<void(bool)>());

  ScopedKeepAlive keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  State state_ = State::kNotStarted;

  // During destruction, `params_` should stay alive longer than
  // `signed_in_flow_` (below) as the flow may want to trigger a callback owned
  // by `params_`.
  ProfilePicker::Params params_;

  // Callback that gets called (if set) when the current window has closed -
  // used to reshow the picker (with different params).
  base::OnceClosure restart_on_window_closing_;

  // A mapping between accelerators and command IDs.
  std::map<ui::Accelerator, int> accelerator_table_;

  // Handler for unhandled key events from renderer.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Owned by the view hierarchy.
  raw_ptr<views::WebView> web_view_ = nullptr;

  // The web contents backed by the picker profile (mostly the system profile).
  // This is used for displaying the WebUI pages.
  std::unique_ptr<content::WebContents> contents_;

  // Observer used for implementing screen switching. Non-null only shorty
  // after switching a screen. Must be below all WebContents instances so that
  // WebContents outlive this observer.
  std::unique_ptr<NavigationFinishedObserver> show_screen_finished_observer_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Toolbar view displayed on top of the WebView for GAIA sign-in, owned by the
  // view hierarchy.
  raw_ptr<ProfilePickerDiceSignInToolbar> toolbar_ = nullptr;
#endif

  Step current_step_ = Step::kUnknown;

  absl::optional<SkColor> profile_color_;

  base::flat_map<Step, std::unique_ptr<ProfileManagementStepController>>
      initialized_steps_;

  // TODO(crbug.com/1359352): To be refactored out.
  // This is used for `ProfilePicker::GetSwitchProfilePath()`. The information
  // should ideally be provided to the handler of the profile switch page once
  // its controller is created instead of relying on static calls.
  base::WeakPtr<ProfilePickerSignedInFlowController>
      weak_signed_in_flow_controller_;

  // Creation time of the picker, to measure performance on startup. Only set
  // when the picker is shown on startup.
  base::TimeTicks creation_time_on_startup_;

  // Hosts dialog displayed when a locked profile is selected in ProfilePicker.
  ProfilePickerForceSigninDialogHost dialog_host_;

  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
