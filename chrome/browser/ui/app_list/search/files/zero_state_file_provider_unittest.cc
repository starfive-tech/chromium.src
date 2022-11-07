// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using ::file_manager::file_tasks::FileTasksObserver;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return arg->title() == title;
}

}  // namespace

class ZeroStateFileProviderTest : public testing::Test {
 protected:
  ZeroStateFileProviderTest() = default;
  ~ZeroStateFileProviderTest() override = default;

  void SetUp() override {
    app_list_color_provider_ =
        std::make_unique<ash::TestAppListColorProvider>();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>(temp_dir_.GetPath());

    // The downloads directory depends on whether it is inside or outside
    // chromeos. So this needs to be in scope before |provider_| and
    // |downloads_folder_|.
    base::test::ScopedRunningOnChromeOS running_on_chromeos;

    auto provider = std::make_unique<ZeroStateFileProvider>(profile_.get());
    provider_ = provider.get();
    search_controller_.AddProvider(0, std::move(provider));

    downloads_folder_ =
        file_manager::util::GetDownloadsFolderForProfile(profile_.get());
    ASSERT_TRUE(base::CreateDirectory(downloads_folder_));

    Wait();
  }

  void TearDown() override { app_list_color_provider_.reset(); }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  base::FilePath DownloadsPath(const std::string& filename) {
    return downloads_folder_.AppendASCII(filename);
  }

  void WriteFile(const base::FilePath& path) {
    CHECK(base::WriteFile(path, "abcd"));
    CHECK(base::PathExists(path));
    Wait();
  }

  FileTasksObserver::FileOpenEvent OpenEvent(const base::FilePath& path) {
    FileTasksObserver::FileOpenEvent e;
    e.path = path;
    e.open_type = FileTasksObserver::OpenType::kOpen;
    return e;
  }

  void StartSearch(const std::u16string& query) {
    search_controller_.StartSearch(query);
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_.last_results();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_folder_;

  TestSearchController search_controller_;
  ZeroStateFileProvider* provider_ = nullptr;
  std::unique_ptr<ash::TestAppListColorProvider> app_list_color_provider_;
};

TEST_F(ZeroStateFileProviderTest, NoResultsWithQuery) {
  StartSearch(u"query");
  Wait();
  EXPECT_TRUE(LastResults().empty());
}

TEST_F(ZeroStateFileProviderTest, ResultsProvided) {
  WriteFile(Path("exists_1.txt"));
  WriteFile(Path("exists_2.png"));
  WriteFile(Path("exists_3.pdf"));

  // Results are only added if they have been opened at least once.
  provider_->OnFilesOpened({OpenEvent(Path("exists_1.txt")),
                            OpenEvent(Path("exists_2.png")),
                            OpenEvent(Path("nonexistent.txt"))});

  StartZeroStateSearch();
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"exists_1.txt"),
                                                  Title(u"exists_2.png")));
}

TEST_F(ZeroStateFileProviderTest, OldFilesNotReturned) {
  WriteFile(Path("new.txt"));
  WriteFile(Path("old.png"));
  auto now = base::Time::Now();
  base::TouchFile(Path("old.png"), now, now - base::Days(8));

  provider_->OnFilesOpened(
      {OpenEvent(Path("new.txt")), OpenEvent(Path("old.png"))});

  StartZeroStateSearch();
  Wait();

  EXPECT_THAT(LastResults(), UnorderedElementsAre(Title(u"new.txt")));
}

TEST_F(ZeroStateFileProviderTest, FilterScreenshots) {
  WriteFile(Path("ScreenshotNonDownload.png"));
  WriteFile(DownloadsPath("ScreenshotNonPng.jpg"));
  WriteFile(DownloadsPath("NotScreenshot.png"));
  WriteFile(DownloadsPath("Screenshot123.png"));

  provider_->OnFilesOpened({OpenEvent(Path("ScreenshotNonDownload.png")),
                            OpenEvent(DownloadsPath("ScreenshotNonPng.jpg")),
                            OpenEvent(DownloadsPath("NotScreenshot.png")),
                            OpenEvent(DownloadsPath("Screenshot123.png"))});

  StartZeroStateSearch();
  Wait();

  // Screenshot123 matches the criteria for a screenshot and should be filtered
  // out.
  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title(u"ScreenshotNonDownload.png"),
                                   Title(u"ScreenshotNonPng.jpg"),
                                   Title(u"NotScreenshot.png")));
}

}  // namespace app_list
