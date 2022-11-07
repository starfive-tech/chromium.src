// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "content/public/test/browser_test.h"

namespace ash {

class CaptivePortalWindowTest : public InProcessBrowserTest {
 protected:
  void ShowIfRedirected() { captive_portal_window_proxy_->ShowIfRedirected(); }

  void Show() { captive_portal_window_proxy_->Show(); }

  void Close() { captive_portal_window_proxy_->Close(); }

  void OnRedirected() { captive_portal_window_proxy_->OnRedirected(); }

  void OnOriginalURLLoaded() {
    captive_portal_window_proxy_->OnOriginalURLLoaded();
  }

  void CheckState(bool is_shown, bool in_progress) {
    bool actual_is_shown = (CaptivePortalWindowProxy::STATE_DISPLAYED ==
                            captive_portal_window_proxy_->GetState());
    ASSERT_EQ(is_shown, actual_is_shown);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kDisableHIDDetectionOnOOBEForTesting);
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        LoginDisplayHost::default_host()->GetOobeWebContents();
    captive_portal_window_proxy_ =
        std::make_unique<CaptivePortalWindowProxy>(web_contents);
  }

  void SetUpInProcessBrowserTestFixture() override {
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);
  }

  void TearDownOnMainThread() override { captive_portal_window_proxy_.reset(); }

 private:
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;
  NetworkPortalDetectorTestImpl* network_portal_detector_;
};

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, Show) {
  Show();
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, ShowClose) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  Show();
  CheckState(/*is_shown=*/true, /*in_progress=*/false);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnRedirected) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected();
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, OnOriginalURLLoaded) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected();
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalWindowTest, MultipleCalls) {
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  ShowIfRedirected();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  Show();
  CheckState(/*is_shown=*/true, /*in_progress=*/false);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/false);

  OnRedirected();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  OnOriginalURLLoaded();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  Show();
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  OnRedirected();
  CheckState(/*is_shown=*/true, /*in_progress=*/true);

  Close();
  // Wait for widget to be destroyed
  base::RunLoop().RunUntilIdle();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);

  OnOriginalURLLoaded();
  CheckState(/*is_shown=*/false, /*in_progress=*/true);
}

class CaptivePortalWindowCtorDtorTest : public LoginManagerTest {
 public:
  CaptivePortalWindowCtorDtorTest() = default;

  CaptivePortalWindowCtorDtorTest(const CaptivePortalWindowCtorDtorTest&) =
      delete;
  CaptivePortalWindowCtorDtorTest& operator=(
      const CaptivePortalWindowCtorDtorTest&) = delete;

  ~CaptivePortalWindowCtorDtorTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->SetDefaultNetworkForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid);
    network_portal_detector_->SetDetectionResultsForTesting(
        FakeShillManagerClient::kFakeEthernetNetworkGuid,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200);
  }

 protected:
  NetworkPortalDetectorTestImpl* network_portal_detector() {
    return network_portal_detector_;
  }

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
  // Use fake GAIA to avoid potential flakiness when real GAIA would not
  // load and Error screen would be shown instead of Login screen.
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

// Flaky on multiple builders, see crbug.com/1244162
IN_PROC_BROWSER_TEST_F(CaptivePortalWindowCtorDtorTest,
                       DISABLED_OpenPortalDialog) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  ASSERT_TRUE(host);
  OobeUI* oobe = host->GetOobeUI();
  ASSERT_TRUE(oobe);

  // Skip to gaia screen.
  host->GetWizardController()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Ensure that error_screen->ShowCaptivePortal() succeeds.
  ErrorScreen* error_screen = oobe->GetErrorScreen();
  ASSERT_TRUE(error_screen);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  error_screen->ShowCaptivePortal();
}

}  // namespace ash
