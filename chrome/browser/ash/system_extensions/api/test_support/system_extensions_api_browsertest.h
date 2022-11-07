// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_TEST_SUPPORT_SYSTEM_EXTENSIONS_API_BROWSERTEST_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_TEST_SUPPORT_SYSTEM_EXTENSIONS_API_BROWSERTEST_H_

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class ScopedContentBrowserClientSetting;
}

namespace ash {

namespace {
class TestRunner;
}

namespace internal {
// ContentBrowserClient that allows to register extra RenderProcessHost
// Mojo interfaces.
class TestChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestChromeContentBrowserClient();

  ~TestChromeContentBrowserClient() override;

  // Should be called on SetUpOnMainThread() to replace the existing
  // ContentBrowserClient.
  void Init();

  // Used to register extra RenderProcessHost interfaces needed for tests.
  template <typename Interface>
  void AddRendererInterface(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>&
          callback) {
    binder_registry_.AddInterface(callback);
  }

  // ChromeContentBrowserClient
  void BindHostReceiverForRenderer(
      content::RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) override;

 private:
  std::unique_ptr<content::ScopedContentBrowserClientSetting>
      scoped_content_browser_client_setting_;

  service_manager::BinderRegistry binder_registry_;
};

}  // namespace internal

// Base test class that can be used to write Testharness.js based System
// Extensions API tests.
//
// Example:
//
// ```c++
// class FooApiBrowserTest : public SystemExtensionsApiBrowserTest {
//  public:
//    FooApiBrowserTest() : SystemExtensionsApiBrowserTest({
//      // Directory where tests are located. Usually in a /test directory next
//      // to the API's implementation.
//      .tests_dir = kTestsDir,
//      // A manifest template that will be used to install a System Extension.
//      // Should contain a `%s` placeholder for the test file name.
//      .manifest_template = kManifestTemplate,
//    }) {}
// }
//
//  IN_PROC_BROWSER_TEST_F(FooApiBrowserTest, Foo) {
//    // `test_name.js` is the name of the test we are going to run.
//    RunTest('test_name.js');
//  }
// ```
// ```js
// // Located in `kTestsDir/test_name.js`
//
// // Import necessary files for testing.
// importScripts(
//   'mojo_bindings_lite.js',
//   'system_extensions_test_runner.test-mojom-lite.js',
//   'testharness.js',
//   'testharness-helpers.js',
//   'testharnessreport.js');
//
// promise_test(async() => {
//  assert_true(await chromeos.foo());
// });
//
// done();  // Needed to tell the framework we finished registering tests.
// ```
// (For more information about testharness.js see:
// https://web-platform-tests.org/writing-tests/testharness-api.html)
//
// RunTest() will copy the required files to a temporary directory, install a
// System Extension from it, and run it. The JS framework is configured to
// notify of failure or success. For example if the test above fails, the
// following will be printed out:
// ```
/// assert_false: expected true got false
//    at Test.<anonymous> (chrome-os-extension://id/test_name.js:12:3)
// ```
//
// We opted for having a IN_PROC_BROWSER_TEST_F per test because most Chromium
// developers are familiar with them and know how to disable them.
class SystemExtensionsApiBrowserTest : public InProcessBrowserTest {
 public:
  struct Args {
    const base::StringPiece tests_dir;
    const base::StringPiece manifest_template;
    const std::vector<std::string>& additional_src_files;
    const std::vector<std::string>& additional_gen_files;
  };

  explicit SystemExtensionsApiBrowserTest(const Args& args);
  ~SystemExtensionsApiBrowserTest() override;

 protected:
  // Runs `test_file_name`.
  void RunTest(base::StringPiece test_file_name);

  // Allows subclasses to register RenderProcessHost interfaces they need for
  // tests.
  template <typename Interface>
  void AddRendererInterface(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>)>&
          callback) {
    test_chrome_content_browser_client_->AddRendererInterface(callback);
  }

  // InProcessBrowserTest
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  testing::AssertionResult RunTestImpl(base::StringPiece test_file_name_expr,
                                       base::StringPiece test_file_name);

  const base::FilePath this_dir_;
  const base::FilePath tests_dir_;
  const std::string manifest_template_;
  const std::vector<std::string> additional_src_files_;
  const std::vector<std::string> additional_gen_files_;

  std::unique_ptr<internal::TestChromeContentBrowserClient>
      test_chrome_content_browser_client_;
  std::unique_ptr<TestRunner> test_runner_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_API_TEST_SUPPORT_SYSTEM_EXTENSIONS_API_BROWSERTEST_H_
