// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_files_request_handler.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace file_manager {

// FilesAppBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* const name) : name(name) {
    CHECK(name && *name) << "no test case name";
  }

  TestCase& InGuestMode() {
    options.guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    options.guest_mode = IN_INCOGNITO;
    return *this;
  }

  TestCase& TabletMode() {
    options.tablet_mode = true;
    return *this;
  }

  TestCase& EnableGenericDocumentsProvider() {
    options.arc = true;
    options.generic_documents_provider = true;
    return *this;
  }

  TestCase& DisableGenericDocumentsProvider() {
    options.generic_documents_provider = false;
    return *this;
  }

  TestCase& EnablePhotosDocumentsProvider() {
    options.arc = true;
    options.photos_documents_provider = true;
    return *this;
  }

  TestCase& DisablePhotosDocumentsProvider() {
    options.photos_documents_provider = false;
    return *this;
  }

  TestCase& EnableArc() {
    options.arc = true;
    return *this;
  }

  TestCase& ExtractArchive() {
    options.extract_archive = true;
    return *this;
  }

  TestCase& Offline() {
    options.offline = true;
    return *this;
  }

  TestCase& FilesExperimental() {
    options.files_experimental = true;
    return *this;
  }

  TestCase& DisableNativeSmb() {
    options.native_smb = false;
    return *this;
  }

  TestCase& DontMountVolumes() {
    options.mount_volumes = false;
    return *this;
  }

  TestCase& DontObserveFileTasks() {
    options.observe_file_tasks = false;
    return *this;
  }

  TestCase& EnableSinglePartitionFormat() {
    options.single_partition_format = true;
    return *this;
  }

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser() {
    options.browser = true;
    return *this;
  }

  TestCase& EnableDriveDssPin() {
    options.drive_dss_pin = true;
    return *this;
  }

  TestCase& EnableFiltersInRecentsV2() {
    options.enable_filters_in_recents_v2 = true;
    return *this;
  }

  TestCase& EnableTrash() {
    options.enable_trash = true;
    return *this;
  }

  TestCase& EnableDlp() {
    options.enable_dlp_files_restriction = true;
    return *this;
  }

  TestCase& EnableWebDriveOffice() {
    options.enable_web_drive_office = true;
    return *this;
  }

  TestCase& EnableUploadOfficeToCloud() {
    options.enable_upload_office_to_cloud = true;
    return *this;
  }

  TestCase& EnableGuestOsFiles() {
    options.enable_guest_os_files = true;
    return *this;
  }

  TestCase& EnableVirtioBlkForData() {
    options.enable_virtio_blk_for_data = true;
    return *this;
  }

  TestCase& EnableMirrorSync() {
    options.enable_mirrorsync = true;
    return *this;
  }

  TestCase& EnableFileTransferConnector() {
    options.enable_file_transfer_connector = true;
    return *this;
  }

  std::string GetFullName() const {
    std::string full_name = name;

    if (options.guest_mode == IN_GUEST_MODE)
      full_name += "_GuestMode";

    if (options.guest_mode == IN_INCOGNITO)
      full_name += "_Incognito";

    if (options.tablet_mode)
      full_name += "_TabletMode";

    if (options.files_experimental)
      full_name += "_FilesExperimental";

    if (!options.native_smb)
      full_name += "_DisableNativeSmb";

    if (options.generic_documents_provider)
      full_name += "_GenericDocumentsProvider";

    if (options.photos_documents_provider)
      full_name += "_PhotosDocumentsProvider";

    if (options.drive_dss_pin)
      full_name += "_DriveDssPin";

    if (options.single_partition_format)
      full_name += "_SinglePartitionFormat";

    if (options.enable_trash)
      full_name += "_Trash";

    if (options.enable_filters_in_recents_v2)
      full_name += "_FiltersInRecentsV2";

    if (options.enable_mirrorsync)
      full_name += "_MirrorSync";

    return full_name;
  }

  const char* const name;
  FileManagerBrowserTestBase::Options options;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  return out << test_case.options;
}

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

  FilesAppBrowserTest(const FilesAppBrowserTest&) = delete;
  FilesAppBrowserTest& operator=(const FilesAppBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Default mode is clamshell: force Ash into tablet mode if requested,
    // and enable the Ash virtual keyboard sub-system therein.
    if (GetOptions().tablet_mode) {
      command_line->AppendSwitchASCII("force-tablet-mode", "touch_view");
      command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
    }
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  Options GetOptions() const override { return GetParam().options; }
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// A version of the FilesAppBrowserTest that supports spanning browser restart
// to allow testing prefs and other things.
class ExtendedFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  ExtendedFilesAppBrowserTest() = default;

  ExtendedFilesAppBrowserTest(const ExtendedFilesAppBrowserTest&) = delete;
  ExtendedFilesAppBrowserTest& operator=(const ExtendedFilesAppBrowserTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, PRE_Test) {
  profile()->GetPrefs()->SetBoolean(prefs::kNetworkFileSharesAllowed,
                                    GetOptions().native_smb);
}

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, Test) {
  StartTest();
}

// A version of FilesAppBrowserTest that supports DLP files restrictions.
class DlpFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  DlpFilesAppBrowserTest(const DlpFilesAppBrowserTest&) = delete;
  DlpFilesAppBrowserTest& operator=(const DlpFilesAppBrowserTest&) = delete;

 protected:
  DlpFilesAppBrowserTest() = default;
  ~DlpFilesAppBrowserTest() override = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ =
        std::make_unique<policy::DlpFilesController>(*mock_rules_manager_);
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  void SetUpOnMainThread() override {
    FilesAppBrowserTest::SetUpOnMainThread();
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpFilesAppBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
  }

  bool HandleDlpCommands(const std::string& name,
                         const base::Value::Dict& value,
                         std::string* output) override {
    if (name == "setIsRestrictedDestinationRestriction") {
      EXPECT_CALL(*mock_rules_manager_, IsRestrictedDestination)
          .WillRepeatedly(
              ::testing::Return(policy::DlpRulesManager::Level::kBlock));
      return true;
    }
    if (name == "setIsRestrictedByAnyRuleRestrictions") {
      EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule)
          .WillOnce(::testing::Return(policy::DlpRulesManager::Level::kWarn))
          .WillOnce(::testing::Return(policy::DlpRulesManager::Level::kAllow))
          .WillOnce(::testing::Return(policy::DlpRulesManager::Level::kNotSet))
          .WillRepeatedly(
              ::testing::Return(policy::DlpRulesManager::Level::kBlock));
      return true;
    }
    if (name == "setIsRestrictedByAnyRuleBlocked") {
      EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule)
          .WillRepeatedly(
              ::testing::Return(policy::DlpRulesManager::Level::kBlock));
      return true;
    }
    return false;
  }

  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  policy::MockDlpRulesManager* mock_rules_manager_ = nullptr;

  std::unique_ptr<policy::DlpFilesController> files_controller_;
};

IN_PROC_BROWSER_TEST_P(DlpFilesAppBrowserTest, Test) {
  chromeos::DlpClient::Get()->GetTestInterface()->SetFakeSource("example1.com");

  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsRestricted)
      .WillByDefault(::testing::Return(policy::DlpRulesManager::Level::kAllow));
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(nullptr));

  StartTest();
}

constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "%s"
          }],
          "destinations": [{
            "file_system_type": "%s"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

base::TimeDelta kResponseDelay = base::Seconds(0);

// A version of FilesAppBrowserTest that supports the file transfer enterprise
// connector.
class FileTransferConnectorFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  FileTransferConnectorFilesAppBrowserTest(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;
  FileTransferConnectorFilesAppBrowserTest& operator=(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;

 protected:
  FileTransferConnectorFilesAppBrowserTest() = default;
  ~FileTransferConnectorFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FilesAppBrowserTest::SetUpOnMainThread();

    // Set a device management token. It is required to enable scanning.
    // Without it, FileTransferAnalysisDelegate::IsEnabled() always
    // returns absl::nullopt.
    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));

    enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::FakeFilesRequestHandler::Create,
            base::BindRepeating(&FileTransferConnectorFilesAppBrowserTest::
                                    FakeFileUploadCallback,
                                base::Unretained(this))));
  }

  bool HandleEnterpriseConnectorCommands(const std::string& name,
                                         const base::Value::Dict& value,
                                         std::string* output) override {
    if (name == "setupFileTransferPolicy") {
      // Set the analysis connector (enterprise_connectors) for FILE_TRANSFER.
      // It is also required for FileTransferAnalysisDelegate::IsEnabled() to
      // return a meaningful result.
      const std::string* source = value.FindString("source");
      CHECK(source);
      const std::string* destination = value.FindString("destination");
      CHECK(destination);
      LOG(INFO) << "Setting file transfer policy for transfers from " << *source
                << " to " << *destination;
      safe_browsing::SetAnalysisConnector(
          profile()->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
          base::StringPrintf(kBlockingScansForDlpAndMalware, source->c_str(),
                             destination->c_str()));
      return true;
    }
    if (name == "issueFileTransferResponses") {
      // Issue all saved responses and issue all future responses directly.
      IssueResponses();
      return true;
    }

    return false;
  }

  // Upload callback to issue responses.
  void FakeFileUploadCallback(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      enterprise_connectors::FakeFilesRequestHandler::FakeFileRequestCallback
          callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(request->device_token(), "dm_token");
    // Simulate a response.
    base::OnceClosure response =
        base::BindOnce(std::move(callback), path,
                       safe_browsing::BinaryUploadService::Result::SUCCESS,
                       ConnectorStatusCallback(path));
    if (save_response_for_later_) {
      // We save the responses for later such that we can check the scanning
      // label.
      // `await sendTestMessage({name: 'issueFileTransferResponses'})` is
      // required from the test to issue the requests.
      saved_responses_.push_back(std::move(response));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, std::move(response), kResponseDelay);
    }
  }

  // Issues the saved responses and sets `save_response_for_later_` to `false`.
  // After this method is called, no more responses will be saved. Instead, the
  // responses will be issued directly.
  void IssueResponses() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    save_response_for_later_ = false;
    for (auto&& response : saved_responses_) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, std::move(response), kResponseDelay);
    }
    saved_responses_.clear();
  }

  enterprise_connectors::ContentAnalysisResponse ConnectorStatusCallback(
      const base::FilePath& path) {
    // We return a block verdict if the basename contains "blocked".
    if (base::Contains(path.BaseName().value(), "blocked")) {
      return enterprise_connectors::FakeContentAnalysisDelegate::
          FakeContentAnalysisDelegate::MalwareAndDlpResponse(
              enterprise_connectors::TriggeredRule::BLOCK,
              enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
              "rule", enterprise_connectors::TriggeredRule::BLOCK);
    } else {
      return enterprise_connectors::FakeContentAnalysisDelegate::
          SuccessfulResponse([]() {
            std::set<std::string> tags;
            tags.insert("dlp");
            tags.insert("malware");
            return tags;
          }());
    }
  }

  // The saved scanning responses.
  std::vector<base::OnceClosure> saved_responses_;
  // Determines whether a current scanning response should be saved for later or
  // issued directly.
  bool save_response_for_later_ = true;
};

IN_PROC_BROWSER_TEST_P(FileTransferConnectorFilesAppBrowserTest, Test) {
  StartTest();
}

// INSTANTIATE_TEST_SUITE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_SUITE_P,
// as WRAPPED_INSTANTIATE_TEST_SUITE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#define WRAPPED_INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator, &PostTestCaseName)

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return test.param.GetFullName();
}

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayDownloads"),
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFile").DontObserveFileTasks(),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayDriveOnlineNewWindow").DontObserveFileTasks(),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartition").EnableSinglePartitionFormat(),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes(),
        // Test is failing (crbug.com/1097013)
        // TestCase("fileDisplayWithoutDriveThenDisable").DontMountVolumes(),
        TestCase("fileDisplayWithHiddenVolume"),
        TestCase("fileDisplayMountWithFakeItemSelected"),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected"),
        TestCase("fileDisplayUnmountRemovableRoot"),
        TestCase("fileDisplayUnmountFirstPartition"),
        TestCase("fileDisplayUnmountLastPartition"),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchNotFound"),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner"),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected"),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnGuestOs")
            .EnableGuestOsFiles()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoMediaApp, /* open_video_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioMediaApp, /* open_audio_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenMediaAppDownloads").InGuestMode(),
                      TestCase("imageOpenMediaAppDownloads"),
                      TestCase("imageOpenMediaAppDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive"),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("zipFileOpenDownloads"),
                      TestCase("zipFileOpenDownloads").InGuestMode(),
                      TestCase("zipFileOpenDrive"),
                      TestCase("zipFileOpenUsb"),
                      TestCase("zipNotifyFileTasks"),
                      TestCase("zipCreateFileDownloads"),
                      TestCase("zipCreateFileDownloads").InGuestMode(),
                      TestCase("zipCreateFileDrive"),
                      TestCase("zipCreateFileDriveOffice"),
                      TestCase("zipCreateFileUsb"),
                      TestCase("zipExtractA11y").ExtractArchive(),
                      TestCase("zipExtractCheckContent").ExtractArchive(),
                      TestCase("zipExtractCheckDuplicates").ExtractArchive(),
                      TestCase("zipExtractCheckEncodings").ExtractArchive(),
                      TestCase("zipExtractNotEnoughSpace").ExtractArchive(),
                      TestCase("zipExtractFromReadOnly").ExtractArchive(),
                      TestCase("zipExtractShowPanel").ExtractArchive(),
                      TestCase("zipExtractShowMultiPanel").ExtractArchive(),
                      TestCase("zipExtractSelectionMenus").ExtractArchive()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads"),
                      TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("keyboardDeleteDownloads").InGuestMode(),
        TestCase("keyboardDeleteDownloads"),
        TestCase("keyboardDeleteDownloads").EnableTrash(),
        TestCase("keyboardDeleteDrive"),
        TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
        TestCase("keyboardDeleteFolderDownloads"),
        TestCase("keyboardDeleteFolderDownloads").EnableTrash(),
        TestCase("keyboardDeleteFolderDrive"),
        TestCase("keyboardCopyDownloads").InGuestMode(),
        TestCase("keyboardCopyDownloads"),
        TestCase("keyboardCopyDownloads").EnableTrash(),
        TestCase("keyboardCopyDrive"),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("keyboardFocusOutlineVisible"),
        TestCase("keyboardFocusOutlineVisible").EnableTrash(),
        TestCase("keyboardFocusOutlineVisibleMouse"),
        TestCase("keyboardFocusOutlineVisibleMouse").EnableTrash(),
#endif
        TestCase("keyboardSelectDriveDirectoryTree"),
        TestCase("keyboardDisableCopyWhenDialogDisplayed"),
        TestCase("keyboardOpenNewWindow"),
        TestCase("keyboardOpenNewWindow").InGuestMode(),
        TestCase("noPointerActiveOnTouch"),
        TestCase("pointerActiveRemovedByTouch"),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDrive"),
        TestCase("renameNewFolderDownloads"),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameRemovableWithKeyboardOnFileList")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile"),
        TestCase("checkDeleteDisabledForReadOnlyDocument"),
        TestCase("checkDeleteDisabledForReadOnlyFile"),
        TestCase("checkDeleteDisabledForReadOnlyFolder"),
        TestCase("checkRenameEnabledForReadWriteFile"),
        TestCase("checkRenameDisabledForReadOnlyDocument"),
        TestCase("checkRenameDisabledForReadOnlyFile"),
        TestCase("checkRenameDisabledForReadOnlyFolder"),
        TestCase("checkContextMenuForRenameInput"),
        TestCase("checkShareEnabledForReadWriteFile"),
        TestCase("checkShareEnabledForReadOnlyDocument"),
        TestCase("checkShareDisabledForStrictReadOnlyDocument"),
        TestCase("checkShareEnabledForReadOnlyFile"),
        TestCase("checkShareEnabledForReadOnlyFolder"),
        TestCase("checkCopyEnabledForReadWriteFile"),
        TestCase("checkCopyEnabledForReadOnlyDocument"),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
        TestCase("checkCopyEnabledForReadOnlyFile"),
        TestCase("checkCopyEnabledForReadOnlyFolder"),
        TestCase("checkCutEnabledForReadWriteFile"),
        TestCase("checkCutDisabledForReadOnlyDocument"),
        TestCase("checkCutDisabledForReadOnlyFile"),
        TestCase("checkDlpRestrictionDetailsDisabledForNonDlpFiles"),
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        // TODO(b/189173190): Enable
        // TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkImportCrostiniImageEnabled"),
        // TODO(b/189173190): Enable
        // TestCase("checkImportCrostiniImageDisabled"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkDeleteDisabledInRecents"),
        TestCase("checkGoToFileLocationEnabledInRecents"),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("toolbarAltACommand"),
                      TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
                      TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
                      TestCase("toolbarDeleteButtonKeepFocus"),
                      TestCase("toolbarDeleteEntry"),
                      TestCase("toolbarDeleteEntry").InGuestMode(),
                      TestCase("toolbarDeleteEntry").EnableTrash(),
                      TestCase("toolbarMultiMenuFollowsButton"),
                      TestCase("toolbarRefreshButtonHiddenInRecents"),
                      TestCase("toolbarRefreshButtonHiddenForWatchableVolume"),
                      TestCase("toolbarRefreshButtonShownForNonWatchableVolume")
                          .EnableGenericDocumentsProvider(),
                      TestCase("toolbarRefreshButtonWithSelection")
                          .EnableGenericDocumentsProvider(),
                      TestCase("toolbarSharesheetButtonWithSelection"),
                      TestCase("toolbarSharesheetContextMenuWithSelection"),
                      TestCase("toolbarSharesheetNoEntrySelected")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView"),
        TestCase("openQuickViewDialog"),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickViewViaContextMenuSingleSelection"),
        TestCase("openQuickViewViaContextMenuCheckSelections"),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewAudioOnDrive"),
        TestCase("openQuickViewAudioWithImageMetadata"),
        TestCase("openQuickViewImageJpg"),
        TestCase("openQuickViewImageJpeg"),
        TestCase("openQuickViewImageJpeg").InGuestMode(),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewImageRawWithOrientation"),
        TestCase("openQuickViewImageWebp"),
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdfPopup"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdfPreviewsDisabled"),
#endif
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewTextFileWithUnknownMimeType"),
        TestCase("openQuickViewUtf8Text"),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewMhtml"),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewDrive"),
        TestCase("openQuickViewSmbfs"),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewAndroidGuestOs")
            .EnableGuestOsFiles()
            .EnableVirtioBlkForData(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewGuestOs").EnableGuestOsFiles(),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions").EnableTrash(),
        TestCase("openQuickViewTrash").EnableTrash(),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewTabIndexImage"),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexHtml"),
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexVideo"),
        TestCase("openQuickViewTabIndexDeleteDialog"),
        TestCase("openQuickViewTabIndexDeleteDialog").EnableTrash(),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection"),
        TestCase("openQuickViewAndDeleteSingleSelection").EnableTrash(),
        TestCase("openQuickViewAndDeleteCheckSelection"),
        TestCase("openQuickViewAndDeleteCheckSelection").EnableTrash(),
        TestCase("openQuickViewDeleteEntireCheckSelection"),
        TestCase("openQuickViewDeleteEntireCheckSelection").EnableTrash(),
        TestCase("openQuickViewClickDeleteButton"),
        TestCase("openQuickViewClickDeleteButton").EnableTrash(),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory"),
        TestCase("directoryTreeSelectedDirectory"),
        // TODO(b/189173190): Enable
        // TestCase("directoryTreeRecentsSubtypeScroll"),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeExpandFolder"),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff"),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu").InGuestMode(),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToExisting"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode(),
#endif
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
        TestCase("dirCreateMultipleFolders"),
        TestCase("dirContextMenuZip"),
        TestCase("dirContextMenuZipEject"),
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFiles").EnableTrash(),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuMyFilesWithPaste").EnableTrash(),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuUsbDcim").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuTrash").EnableTrash(),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline").EnableGenericDocumentsProvider(),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("driveAutoCompleteQuery"),
        TestCase("drivePinMultiple"),
        TestCase("drivePinHosted"),
        // TODO(b/189173190): Enable
        // TestCase("drivePinFileMobileNetwork"),
        TestCase("drivePinToggleUpdatesInFakeEntries"),
        TestCase("driveClickFirstSearchResult"),
        TestCase("drivePressEnterToSearch").FilesExperimental(),
        TestCase("drivePressClearSearch"),
        TestCase("driveSearchAlwaysDisplaysMyDrive"),
        TestCase("driveSearchAlwaysDisplaysMyDrive").FilesExperimental(),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkOpenFileThroughLinkedDirectory"),
        TestCase("driveLinkOpenFileThroughTransitiveLink"),
        TestCase("driveWelcomeBanner"),
        TestCase("driveOfflineInfoBanner").EnableDriveDssPin(),
        TestCase("driveOfflineInfoBannerWithoutFlag")
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialog"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBanner"),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferOfficeFileFromDriveToDownloads"),
        TestCase("transferFromDownloadsToMyFiles"),
        TestCase("transferFromDownloadsToMyFilesMove"),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromSharedWithMeToDownloads"),
        TestCase("transferFromSharedWithMeToDrive"),
        TestCase("transferFromDownloadsToSharedFolder"),
        TestCase("transferFromDownloadsToSharedFolderMove"),
        TestCase("transferFromSharedFolderToDownloads"),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDrive"),
        TestCase("transferFromTeamDriveToDrive"),
        TestCase("transferFromDriveToTeamDrive"),
        TestCase("transferFromTeamDriveToDownloads"),
        TestCase("transferHostedFileFromTeamDriveToDownloads"),
        TestCase("transferFromDownloadsToTeamDrive"),
        TestCase("transferBetweenTeamDrives"),
        TestCase("transferDragDropActiveLeave"),
        TestCase("transferDragDropActiveDrop"),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragDropTreeItemDenies"),
#endif
        TestCase("transferDragAndHoverTreeItemEntryList"),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragAndHoverTreeItemFakeEntry"),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat(),
#endif
        TestCase("transferDragFileListItemSelects"),
        TestCase("transferDragAndDrop"),
        TestCase("transferDragAndDropFolder"),
        TestCase("transferDragAndHover"),
        TestCase("transferDropBrowserFile"),
        TestCase("transferFromDownloadsToDownloads"),
        TestCase("transferDeletedFile"),
        TestCase("transferDeletedFile").EnableTrash(),
        // TODO(b/189173190): Enable
        // TestCase("transferInfoIsRemembered"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferToUsbHasDestinationText"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferDismissedErrorIsRemembered"),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText"),
        TestCase("transferUpdateSamePanelItem"),
        TestCase("transferShowPendingMessageForZeroRemainingTime")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DLP, /* dlp.js */
    DlpFilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferShowDlpToast").EnableDlp(),
        TestCase("dlpShowManagedIcon").EnableDlp(),
        TestCase("dlpContextMenuRestrictionDetails").EnableDlp()));

#define FILE_TRANSFER_TEST_CASE(name) \
  TestCase(name).EnableFileTransferConnector()

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileTransferConnector, /* file_transfer_connector.js */
    FileTransferConnectorFilesAppBrowserTest,
    ::testing::Values(
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromAndroidFilesToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromAndroidFilesToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromCrostiniToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromCrostiniToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromDriveToDownloadsMoveDeep"),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromDriveToDownloadsMoveFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromMtpToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromMtpToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromSmbfsToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromSmbfsToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromUsbToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromUsbToDownloadsFlat")));

#undef FILE_TRANSFER_TEST_CASE

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("shareFileDrive"),
                      TestCase("shareDirectoryDrive"),
                      TestCase("shareHostedFileDrive"),
                      TestCase("manageHostedFileDrive"),
                      TestCase("manageFileDrive"),
                      TestCase("manageDirectoryDrive"),
                      TestCase("shareFileTeamDrive"),
                      TestCase("shareDirectoryTeamDrive"),
                      TestCase("shareHostedFileTeamDrive"),
                      TestCase("shareTeamDrive"),
                      TestCase("manageHostedFileTeamDrive"),
                      TestCase("manageFileTeamDrive"),
                      TestCase("manageDirectoryTeamDrive"),
                      TestCase("manageTeamDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive"),
                      TestCase("defaultTaskForPdf"),
                      TestCase("defaultTaskForTextPlain"),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive"),
                      TestCase("changeDefaultDialogScrollList"),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask"),
                      TestCase("noActionBarOpenForDirectories")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts"),
                      TestCase("addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusDirectorySelected"),
        TestCase("tabindexOpenDialogDownloads").WithBrowser()
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        //      ,
        //      TestCase("tabindexSaveFileDialogDrive").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser().InGuestMode()
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        // TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        // TestCase("openFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogAriaMultipleSelect").WithBrowser(),
        TestCase("saveFileDialogAriaSingleSelect").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser(),
        TestCase("saveFileDialogDownloads").WithBrowser().InGuestMode(),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        // TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDrive").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveOfficeFile").WithBrowser(),
        TestCase("openMultiFileDialogDriveOfficeFile").WithBrowser(),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilterKeyNavigation").WithBrowser(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled").WithBrowser(),
        TestCase("saveFileDialogGuestOs").WithBrowser().EnableGuestOsFiles(),
        TestCase("saveFileDialogGuestOs")
            .WithBrowser()
            .EnableGuestOsFiles()
            .InIncognito(),
        TestCase("openFileDialogGuestOs").WithBrowser().EnableGuestOsFiles(),
        TestCase("openFileDialogGuestOs")
            .WithBrowser()
            .EnableGuestOsFiles()
            .InIncognito()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("copyBetweenWindowsLocalToDrive"),
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsDriveToUsb"),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDownloads"),
                      TestCase("showGridViewButtonSwitches"),
                      TestCase("showGridViewKeyboardSelectionA11y"),
                      TestCase("showGridViewTitles"),
                      TestCase("showGridViewMouseSelectionA11y"),
                      TestCase("showGridViewDocumentsProvider")
                          .EnableGenericDocumentsProvider()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMount").DisableNativeSmb(),
                      TestCase("requestMountMultipleMounts"),
                      TestCase("requestMountMultipleMounts").DisableNativeSmb(),
                      TestCase("requestMountSourceDevice"),
                      TestCase("requestMountSourceDevice").DisableNativeSmb(),
                      TestCase("requestMountSourceFile"),
                      TestCase("requestMountSourceFile").DisableNativeSmb(),
                      TestCase("providerEject"),
                      TestCase("providerEject").DisableNativeSmb()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads"),
        TestCase("showHiddenFilesDownloads").InGuestMode(),
        TestCase("showHiddenFilesDrive"),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles"),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showSendFeedbackAction"),
        TestCase("enableDisableStorageSettingsLink"),
        TestCase("showAvailableStorageMyFiles"),
        TestCase("showAvailableStorageDrive"),
        TestCase("showAvailableStorageSmbfs"),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("openHelpPageFromDownloadsVolume"),
        TestCase("openHelpPageFromDriveVolume"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot")
            .EnableMirrorSync()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipLabelChange"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipMouseOverStaysOpen"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides"),
                      TestCase("filesTooltipHidesOnDeleteDialogClosed")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("fileListAriaAttributes"),
                      TestCase("fileListFocusFirstItem"),
                      TestCase("fileListSelectLastFocusedItem"),
                      TestCase("fileListKeyboardSelectionA11y"),
                      TestCase("fileListMouseSelectionA11y"),
                      TestCase("fileListDeleteMultipleFiles"),
                      TestCase("fileListDeleteMultipleFiles").EnableTrash(),
                      TestCase("fileListRenameSelectedItem"),
                      TestCase("fileListRenameFromSelectAll")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostini"),
                      TestCase("enableDisableCrostini"),
                      TestCase("sharePathWithCrostini"),
                      TestCase("pluginVmDirectoryNotSharedErrorDialog"),
                      TestCase("pluginVmFileOnExternalDriveErrorDialog"),
                      TestCase("pluginVmFileDropFailErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh"),
        TestCase("showMyFiles"),
        TestCase("showMyFiles").EnableTrash(),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesDisplaysAndOpensEntries").FilesExperimental(),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts").DontMountVolumes(),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Navigation, /* navigation.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("navigateToParent")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("recentsA11yMessages"),
        TestCase("recentsAllowCutForDownloads").EnableFiltersInRecentsV2(),
        TestCase("recentsAllowCutForDrive").EnableFiltersInRecentsV2(),
        TestCase("recentsAllowCutForPlayFiles")
            .EnableArc()
            .EnableFiltersInRecentsV2(),
        TestCase("recentsAllowDeletion").EnableArc().EnableFiltersInRecentsV2(),
        TestCase("recentsAllowMultipleFilesDeletion")
            .EnableArc()
            .EnableFiltersInRecentsV2(),
        TestCase("recentsAllowRename").EnableArc().EnableFiltersInRecentsV2(),
        TestCase("recentsEmptyFolderMessage").EnableFiltersInRecentsV2(),
        TestCase("recentsEmptyFolderMessageAfterDeletion")
            .EnableFiltersInRecentsV2(),
        TestCase("recentsDownloads"),
        TestCase("recentsDrive"),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentsFilterResetToAll"),
        TestCase("recentsNested"),
        TestCase("recentsNoRenameForPlayFiles")
            .EnableArc()
            .EnableFiltersInRecentsV2(),
        TestCase("recentsPlayFiles").EnableArc(),
        TestCase("recentsReadOnlyHidden").EnableFiltersInRecentsV2(),
        TestCase("recentsRespectSearchWhenSwitchingFilter")
            .EnableFiltersInRecentsV2(),
        TestCase("recentsRespondToTimezoneChangeForGridView")
            .EnableFiltersInRecentsV2(),
        TestCase("recentsRespondToTimezoneChangeForListView")
            .EnableFiltersInRecentsV2(),
        TestCase("recentsTimePeriodHeadings").EnableFiltersInRecentsV2(),
        TestCase("recentAudioDownloads"),
        TestCase("recentAudioDownloadsAndDrive"),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentDocumentsDownloads").EnableFiltersInRecentsV2(),
        TestCase("recentDocumentsDownloadsAndDrive").EnableFiltersInRecentsV2(),
        TestCase("recentDocumentsDownloadsAndDriveAndPlayFiles")
            .EnableArc()
            .EnableFiltersInRecentsV2(),
        TestCase("recentImagesDownloads"),
        TestCase("recentImagesDownloadsAndDrive"),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentVideosDownloads"),
        TestCase("recentVideosDownloadsAndDrive"),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles").EnableArc()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive"),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("searchDownloadsWithResults"),
                      TestCase("searchDownloadsWithNoResults"),
                      TestCase("searchDownloadsClearSearchKeyDown"),
                      TestCase("searchDownloadsClearSearch"),
                      TestCase("searchHidingViaTab"),
                      TestCase("searchHidingTextEntryField"),
                      TestCase("searchButtonToggles")
                      // TODO(b/189173190): Enable
                      // TestCase("searchQueryLaunchParam")
                      ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum"),
                      TestCase("metricsOpenSwa"),
                      TestCase("metricsRecordDirectoryListLoad"),
                      TestCase("metricsRecordUpdateAvailableApps")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate"),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsRenderShortPath"),
                      TestCase("breadcrumbsEliderButtonHidden"),
                      TestCase("breadcrumbsRenderLongPath"),
                      TestCase("breadcrumbsMainButtonClick"),
                      TestCase("breadcrumbsMainButtonEnterKey"),
                      TestCase("breadcrumbsEliderButtonClick"),
                      TestCase("breadcrumbsEliderButtonKeyboard"),
                      TestCase("breadcrumbsEliderMenuClickOutside"),
                      TestCase("breadcrumbsEliderMenuItemClick"),
                      TestCase("breadcrumbsEliderMenuItemTabLeft"),
                      TestCase("breadcrumbNavigateBackToSharedWithMe"),
                      TestCase("breadcrumbsEliderMenuItemTabRight")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("formatDialog"),
        TestCase("formatDialogIsModal"),
        TestCase("formatDialogEmpty"),
        TestCase("formatDialogCancel"),
        TestCase("formatDialogNameLength"),
        TestCase("formatDialogNameInvalid"),
        TestCase("formatDialogGearMenu"),
        TestCase("formatDialog").EnableSinglePartitionFormat(),
        TestCase("formatDialogIsModal").EnableSinglePartitionFormat(),
        TestCase("formatDialogEmpty").EnableSinglePartitionFormat(),
        TestCase("formatDialogCancel").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameLength").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameInvalid").EnableSinglePartitionFormat(),
        TestCase("formatDialogGearMenu").EnableSinglePartitionFormat()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Trash, /* trash.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("trashMoveToTrash").EnableTrash(),
        TestCase("trashPermanentlyDelete").EnableTrash(),
        TestCase("trashRestoreFromToast").EnableTrash(),
        TestCase("trashRestoreFromTrash").EnableTrash(),
        TestCase("trashRestoreFromTrashShortcut").EnableTrash(),
        TestCase("trashEmptyTrash").EnableTrash(),
        TestCase("trashEmptyTrashShortcut").EnableTrash(),
        TestCase("trashDeleteFromTrash").EnableTrash(),
        TestCase("trashNoTasksInTrashRoot").EnableTrash(),
        TestCase("trashDoubleClickOnFileInTrashRootShowsDialog").EnableTrash(),
        TestCase("trashDragDropRootAcceptsEntries").EnableTrash(),
        TestCase("trashDragDropFromDisallowedRootsFails").EnableTrash(),
        TestCase("trashDragDropNonModifiableEntriesCantBeTrashed")
            .EnableTrash(),
        TestCase("trashDragDropRootPerformsTrashAction").EnableTrash(),
        TestCase("trashTraversingFolderShowsDisallowedDialog").EnableTrash(),
        TestCase("trashDontShowTrashRootOnSelectFileDialog").EnableTrash(),
        TestCase("trashDontShowTrashRootWhenOpeningAsAndroidFilePicker")
            .EnableTrash(),
        TestCase("trashEnsureOldEntriesArePeriodicallyRemoved").EnableTrash(),
        TestCase("trashDragDropOutOfTrashPerformsRestoration").EnableTrash(),
        TestCase("trashCopyShouldBeDisabledCutShouldBeEnabled").EnableTrash(),
        TestCase("trashRestorationDialogInProgressDoesntShowUndo")
            .EnableTrash()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    AndroidPhotos, /* android_photos.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("androidPhotosBanner").EnablePhotosDocumentsProvider()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Office, /* office.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openOfficeWordFile").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromMyFiles")
            .EnableWebDriveOffice()
            .EnableUploadOfficeToCloud(),
        TestCase("uploadToDriveRequiresWebDriveOfficeEnabled")
            .EnableUploadOfficeToCloud(),
        TestCase("openMultipleOfficeWordFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficeExcelFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficePowerPointFromDrive").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromDriveNotSynced").EnableWebDriveOffice(),
        TestCase("openOfficeWordFromMyFilesOffline")
            .EnableWebDriveOffice()
            .EnableUploadOfficeToCloud()
            .Offline(),
        TestCase("openOfficeWordFromDriveOffline")
            .EnableWebDriveOffice()
            .Offline()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GuestOs, /* guest_os.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fakesListed").EnableGuestOsFiles(),
        TestCase("listUpdatedWhenGuestsChanged").EnableGuestOsFiles(),
        TestCase("mountGuestSuccess").EnableGuestOsFiles(),
        TestCase("notListedWithoutFlag"),
        TestCase("mountAndroidVolumeSuccess")
            .EnableGuestOsFiles()
            .EnableVirtioBlkForData()));

}  // namespace file_manager
