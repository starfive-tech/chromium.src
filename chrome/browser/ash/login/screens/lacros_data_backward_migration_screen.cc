// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_paths.h"

namespace ash {

LacrosDataBackwardMigrationScreen::LacrosDataBackwardMigrationScreen(
    base::WeakPtr<LacrosDataBackwardMigrationScreenView> view)
    : BaseScreen(LacrosDataBackwardMigrationScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(std::move(view)) {
  DCHECK(view_);
}

LacrosDataBackwardMigrationScreen::~LacrosDataBackwardMigrationScreen() =
    default;

void LacrosDataBackwardMigrationScreen::ShowImpl() {
  if (!view_)
    return;

  if (!migrator_) {
    const std::string user_id_hash =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBrowserDataBackwardMigrationForUser);

    DCHECK(!user_id_hash.empty()) << "user_id_hash should not be empty.";
    if (user_id_hash.empty()) {
      LOG(ERROR) << "Could not retrieve user_id_hash from switch "
                 << switches::kBrowserDataBackwardMigrationForUser
                 << ". Aborting migration.";
      // TODO(b/245053119): Attempt restart.
      return;
    }

    base::FilePath user_data_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
      LOG(ERROR) << "Could not get the original user data dir path. Aborting "
                    "migration.";
      // TODO(b/245053119): Attempt restart.
      return;
    }

    const base::FilePath profile_data_dir =
        user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

    migrator_ = std::make_unique<BrowserDataBackMigrator>(profile_data_dir);
  }

  migrator_->Migrate(
      base::BindOnce(&LacrosDataBackwardMigrationScreen::OnMigrated,
                     weak_factory_.GetWeakPtr()));

  // Show the screen.
  view_->Show();
}

void LacrosDataBackwardMigrationScreen::OnMigrated(
    BrowserDataBackMigrator::Result result) {
  switch (result) {
    case BrowserDataBackMigrator::Result::kSucceeded:
      // TODO
      break;
    case BrowserDataBackMigrator::Result::kFailed:
      // TODO
      break;
  }
}

void LacrosDataBackwardMigrationScreen::HideImpl() {}

}  // namespace ash
