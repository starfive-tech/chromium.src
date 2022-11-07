// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/mock_app_home_page.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using web_app::AppId;
using GetAppsCallback =
    base::OnceCallback<void(std::vector<app_home::mojom::AppInfoPtr>)>;

namespace webapps {

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestAppName[] = "Test App";

class TestAppHomePageHandler : public AppHomePageHandler {
 public:
  TestAppHomePageHandler(content::WebUI* web_ui,
                         Profile* profile,
                         mojo::PendingRemote<app_home::mojom::Page> page)
      : AppHomePageHandler(
            web_ui,
            profile,
            mojo::PendingReceiver<app_home::mojom::PageHandler>(),
            std::move(page)) {
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  TestAppHomePageHandler(const TestAppHomePageHandler&) = delete;
  TestAppHomePageHandler& operator=(const TestAppHomePageHandler&) = delete;

  ~TestAppHomePageHandler() override = default;

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  void OnWebAppInstalled(const web_app::AppId& app_id) override {
    run_loop_->Quit();
    AppHomePageHandler::OnWebAppInstalled(app_id);
  }

  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    run_loop_->Quit();
    AppHomePageHandler::OnExtensionLoaded(browser_context, extension);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

std::unique_ptr<WebAppInstallInfo> BuildWebAppInfo() {
  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = base::UTF8ToUTF16(base::StringPiece(kTestAppName));
  app_info->manifest_url = GURL(kTestManifestUrl);

  return app_info;
}

GetAppsCallback WrapGetAppsCallback(
    std::vector<app_home::mojom::AppInfoPtr>* out,
    base::OnceClosure quit_closure) {
  return base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::vector<app_home::mojom::AppInfoPtr>* out,
         std::vector<app_home::mojom::AppInfoPtr> result) {
        *out = std::move(result);
        std::move(quit_closure).Run();
      },
      std::move(quit_closure), out);
}

}  // namespace

class AppHomePageHandlerTest : public WebAppTest {
 public:
  AppHomePageHandlerTest() = default;

  AppHomePageHandlerTest(const AppHomePageHandlerTest&) = delete;
  AppHomePageHandlerTest& operator=(const AppHomePageHandlerTest&) = delete;

  ~AppHomePageHandlerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    extension_service_ = CreateTestExtensionService();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  std::unique_ptr<TestAppHomePageHandler> GetAppHomePageHandler(
      content::TestWebUI* test_web_ui) {
    return std::make_unique<TestAppHomePageHandler>(test_web_ui, profile(),
                                                    page_.BindAndGetRemote());
  }

  AppId InstallTestWebApp() {
    AppId installed_app_id =
        web_app::test::InstallWebApp(profile(), BuildWebAppInfo());

    return installed_app_id;
  }

  scoped_refptr<const extensions::Extension> InstallTestExtensionApp() {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(kTestAppName).Build();
    extension_service_->AddExtension(extension.get());
    return extension;
  }

  std::unique_ptr<content::TestWebUI> CreateTestWebUI() {
    auto test_web_ui = std::make_unique<content::TestWebUI>();
    test_web_ui->set_web_contents(web_contents());
    return test_web_ui;
  }

  extensions::ExtensionService* CreateTestExtensionService() {
    auto* extension_system = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()));
    extensions::ExtensionService* ext_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ext_service->Init();
    return ext_service;
  }

  testing::StrictMock<MockAppHomePage> page_;
  raw_ptr<extensions::ExtensionService> extension_service_;
};

MATCHER_P(MatchAppName, expected_app_name, "") {
  if (expected_app_name == arg->name) {
    return true;
  }
  return false;
}

TEST_F(AppHomePageHandlerTest, GetApps) {
  AppId installed_app_id = InstallTestWebApp();

  std::unique_ptr<content::TestWebUI> test_web_ui = CreateTestWebUI();

  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler(test_web_ui.get());

  std::vector<app_home::mojom::AppInfoPtr> app_infos;
  base::RunLoop run_loop;
  page_handler->GetApps(
      WrapGetAppsCallback(&app_infos, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(1u, app_infos.size());
  EXPECT_EQ(kTestAppUrl, app_infos[0]->start_url);
  EXPECT_EQ(kTestAppName, app_infos[0]->name);
}

TEST_F(AppHomePageHandlerTest, OnWebAppInstalled) {
  std::unique_ptr<content::TestWebUI> test_web_ui = CreateTestWebUI();
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler(test_web_ui.get());

  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  AppId installed_app_id = InstallTestWebApp();
  page_handler->Wait();
}

TEST_F(AppHomePageHandlerTest, OnExtensionLoaded) {
  std::unique_ptr<content::TestWebUI> test_web_ui = CreateTestWebUI();
  std::unique_ptr<TestAppHomePageHandler> page_handler =
      GetAppHomePageHandler(test_web_ui.get());

  EXPECT_CALL(page_, AddApp(MatchAppName(kTestAppName)));
  scoped_refptr<const extensions::Extension> extension =
      InstallTestExtensionApp();
  ASSERT_NE(extension, nullptr);
  page_handler->Wait();
}

}  // namespace webapps
