// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/background_color_change_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace {

constexpr SkColor kAppBackgroundColor = SK_ColorBLUE;
constexpr char kAppPath[] = "/web_apps/no_service_worker.html";

}  // namespace
namespace web_app {

class WebAppTabStripBrowserTest : public InProcessBrowserTest {
 public:
  WebAppTabStripBrowserTest() = default;
  ~WebAppTabStripBrowserTest() override = default;

  void SetUp() override {
    features_.InitWithFeatures({features::kDesktopPWAsTabStrip,
                                features::kDesktopPWAsTabStripSettings},
                               {});
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  struct App {
    AppId id;
    raw_ptr<Browser> browser;
    raw_ptr<BrowserView> browser_view;
    content::WebContents* web_contents;
  };

  App InstallAndLaunch() {
    Profile* profile = browser()->profile();
    GURL start_url = embedded_test_server()->GetURL(kAppPath);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test app";
    web_app_info->background_color = kAppBackgroundColor;
    web_app_info->user_display_mode = UserDisplayMode::kTabbed;
    AppId app_id = test::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = LaunchWebAppBrowser(profile, app_id);
    return App{app_id, app_browser,
               BrowserView::GetBrowserViewForBrowser(app_browser),
               app_browser->tab_strip_model()->GetActiveWebContents()};
  }

  SkColor GetTabColor(BrowserView* browser_view) {
    return browser_view->tabstrip()->GetTabBackgroundColor(
        TabActive::kActive, BrowserFrameActiveState::kActive);
  }

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       CustomTabBarUpdateOnTabSwitch) {
  App app = InstallAndLaunch();

  CustomTabBarView* custom_tab_bar =
      app.browser_view->toolbar()->custom_tab_bar();
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Add second tab.
  chrome::NewTab(app.browser);
  ASSERT_EQ(app.browser->tab_strip_model()->count(), 2);

  // Navigate tab out of scope, custom tab bar should appear.
  GURL in_scope_url = embedded_test_server()->GetURL(kAppPath);
  GURL out_of_scope_url =
      embedded_test_server()->GetURL("/banners/theme-color.html");
  ASSERT_TRUE(content::NavigateToURL(
      app.browser->tab_strip_model()->GetActiveWebContents(),
      out_of_scope_url));
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());

  // Custom tab bar should go away for in scope tab.
  chrome::SelectNextTab(app.browser);
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      in_scope_url);
  EXPECT_FALSE(custom_tab_bar->GetVisible());

  // Custom tab bar should return for out of scope tab.
  chrome::SelectNextTab(app.browser);
  EXPECT_EQ(
      app.browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      out_of_scope_url);
  EXPECT_TRUE(custom_tab_bar->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, PopOutTabOnInstall) {
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");

  NavigateToURLAndWait(browser(), start_url);

  // Install the site with the user display mode set to kTabbed.
  AppId app_id;
  {
    base::RunLoop run_loop;
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider);
    test::WaitUntilReady(provider);
    provider->command_manager().ScheduleCommand(
        std::make_unique<FetchManifestAndInstallCommand>(
            &provider->install_finalizer(), &provider->registrar(),
            webapps::WebappInstallSource::MENU_BROWSER_TAB,
            browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
            /*bypass_service_worker_check=*/false,
            base::BindLambdaForTesting(
                [](content::WebContents*,
                   std::unique_ptr<WebAppInstallInfo> web_app_info,
                   WebAppInstallationAcceptanceCallback acceptance_callback) {
                  web_app_info->user_display_mode = UserDisplayMode::kTabbed;
                  std::move(acceptance_callback)
                      .Run(/*user_accepted=*/true, std::move(web_app_info));
                }),
            base::BindLambdaForTesting([&run_loop, &app_id](
                                           const AppId& installed_app_id,
                                           webapps::InstallResultCode code) {
              DCHECK_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
              app_id = installed_app_id;
              run_loop.Quit();
            }),
            /*use_fallback=*/true, WebAppInstallFlow::kInstallSite));
    run_loop.Run();
  }

  // After installing a tabbed display mode app the install page should pop out
  // to a standalone app window.
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(AppBrowserController::IsForWebApp(app_browser, app_id));
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL("chrome://newtab/"));
}

// TODO(crbug.com/897314) Enabled tab strip for web apps on non-Chrome OS.
#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest,
                       ActiveTabColorIsBackgroundColor) {
  // Ensure we're not using the system theme on Linux.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->UseDefaultTheme();

  App app = InstallAndLaunch();
  EXPECT_EQ(registrar().GetAppBackgroundColor(app.id), kAppBackgroundColor);

  // Expect manifest background color prior to page loading.
  {
    ASSERT_FALSE(
        app.web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              kAppBackgroundColor);
    EXPECT_EQ(GetTabColor(app.browser_view), kAppBackgroundColor);
  }

  // Expect initial page background color to be white.
  {
    content::BackgroundColorChangeWaiter(app.web_contents).Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorWHITE);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorWHITE);
  }

  // Ensure HTML document has loaded before we execute JS in it.
  content::AwaitDocumentOnLoadCompleted(app.web_contents);

  // Set document color to black and read tab background color.
  {
    content::BackgroundColorChangeWaiter waiter(app.web_contents);
    EXPECT_TRUE(content::ExecJs(
        app.web_contents, "document.body.style.backgroundColor = 'black';"));
    waiter.Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorBLACK);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorBLACK);
  }

  // Update document color to cyan and check that the tab color matches.
  {
    content::BackgroundColorChangeWaiter waiter(app.web_contents);
    EXPECT_TRUE(content::ExecJs(
        app.web_contents, "document.body.style.backgroundColor = 'cyan';"));
    waiter.Wait();
    EXPECT_EQ(app.browser->app_controller()->GetBackgroundColor().value(),
              SK_ColorCYAN);
    EXPECT_EQ(GetTabColor(app.browser_view), SK_ColorCYAN);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, AutoNewTabUrl) {
  GURL start_url = embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_tabbed_display_override.json");
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = LaunchWebAppBrowser(browser()->profile(), app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  chrome::NewTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      registrar().GetAppStartUrl(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, NewTabUrl) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = LaunchWebAppBrowser(browser()->profile(), app_id);

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  chrome::NewTab(app_browser);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, InstallingPinsHomeTab) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/tab_strip_customizations.html?some_query#blah");
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  // The URL of the pinned home tab should include the query params.
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, InstallFromNonHomeTabUrl) {
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?tab_strip_customizations.json");
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect the home tab was opened in addition to the launch URL.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, OpeningPinsHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  // Install and close app.
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  CloseAndWait(app_browser);

  // Launch the app to a non home tab URL.
  app_browser = LaunchWebAppToURL(
      browser()->profile(), app_id,
      embedded_test_server()->GetURL("/web_apps/favicon_only.html"));
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  EXPECT_TRUE(registrar().IsTabbedWindowModeEnabled(app_id));

  // Expect the home tab was opened in addition to the launch URL.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Open app at home tab URL.
  EXPECT_EQ(app_browser,
            LaunchWebAppToURL(browser()->profile(), app_id, start_url));

  // Expect the home tab to be focused.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(), start_url);
  EXPECT_EQ(tab_strip->active_index(), 0);
}

IN_PROC_BROWSER_TEST_F(WebAppTabStripBrowserTest, ReparentingPinsHomeTab) {
  GURL start_url =
      embedded_test_server()->GetURL("/web_apps/tab_strip_customizations.html");
  // Install and close app.
  AppId app_id = InstallWebAppFromPage(browser(), start_url);
  Browser* app_browser = FindWebAppBrowser(browser()->profile(), app_id);
  CloseAndWait(app_browser);

  // Navigate to the app URL in the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/web_apps/favicon_only.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Reparent web contents into app browser.
  app_browser =
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, app_id);
  TabStripModel* tab_strip = app_browser->tab_strip_model();

  // Expect the pinned home tab to also be opened.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Navigate to home tab URL in the browser.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/web_apps/tab_strip_customizations.html")));
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Reparent web contents into app browser.
  EXPECT_EQ(app_browser,
            web_app::ReparentWebContentsIntoAppBrowser(web_contents, app_id));
  tab_strip = app_browser->tab_strip_model();

  // Expect home tab to be focused.
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_TRUE(tab_strip->IsTabPinned(0));
  EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetVisibleURL(),
            registrar().GetAppStartUrl(app_id));
  EXPECT_EQ(tab_strip->active_index(), 0);
}

}  // namespace web_app
