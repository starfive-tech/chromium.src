// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/bind.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace {

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

// static
void ProjectorClientImpl::InitForProjectorAnnotator(views::WebView* web_view) {
  if (!ash::features::IsProjectorAnnotatorEnabled())
    return;
  web_view->LoadInitialURL(GURL(ash::kChromeUITrustedAnnotatorAppUrl));
}

// Using base::Unretained for callback is safe since the ProjectorClientImpl
// owns `drive_helper_`.
ProjectorClientImpl::ProjectorClientImpl(ash::ProjectorController* controller)
    : controller_(controller),
      drive_helper_(base::BindRepeating(
          &ProjectorClientImpl::MaybeSwitchDriveIntegrationServiceObservation,
          base::Unretained(this))) {
  controller_->SetClient(this);
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);
}

ProjectorClientImpl::ProjectorClientImpl()
    : ProjectorClientImpl(ash::ProjectorController::Get()) {}

ProjectorClientImpl::~ProjectorClientImpl() {
  controller_->SetClient(nullptr);
}

void ProjectorClientImpl::StartSpeechRecognition() {
  // ProjectorController should only request for speech recognition after it
  // has been informed that recognition is available.
  // TODO(crbug.com/1165437): Dynamically determine language code.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
      GetLocale()));

  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetActiveUserProfile(),
      GetLocale(), /*recognition_mode_ime=*/false,
      /*enable_formatting=*/true);
}

void ProjectorClientImpl::StopSpeechRecognition() {
  speech_recognizer_->Stop();
}

bool ProjectorClientImpl::GetBaseStoragePath(base::FilePath* result) const {
  if (!IsDriveFsMounted())
    return false;

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    auto* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);

    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    *result = download_prefs->GetDefaultDownloadDirectoryForProfile();
    return true;
  }

  *result = ProjectorDriveFsProvider::GetDriveFsMountPointPath();
  return true;
}

bool ProjectorClientImpl::IsDriveFsMounted() const {
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    return false;

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    // Return true when extended projector features are disabled. Use download
    // folder for Projector storage.
    return true;
  }
  return ProjectorDriveFsProvider::IsDriveFsMounted();
}

bool ProjectorClientImpl::IsDriveFsMountFailed() const {
  return ProjectorDriveFsProvider::IsDriveFsMountFailed();
}

void ProjectorClientImpl::OpenProjectorApp() const {
  LaunchProjectorAppWithFiles(/*files=*/{});
}

void ProjectorClientImpl::MinimizeProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
  if (browser)
    browser->window()->Minimize();
}

void ProjectorClientImpl::CloseProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
  if (browser)
    browser->window()->Close();
}

void ProjectorClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) const {
  ash::ProjectorAppClient* app_client = ash::ProjectorAppClient::Get();
  if (app_client)
    app_client->OnNewScreencastPreconditionChanged(precondition);
}

void ProjectorClientImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const absl::optional<media::SpeechRecognitionResult>& full_result) {
  DCHECK(full_result.has_value());
  controller_->OnTranscription(full_result.value());
}

void ProjectorClientImpl::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_ERROR) {
    speech_recognizer_.reset();
    recognizer_status_ = SPEECH_RECOGNIZER_OFF;
    controller_->OnTranscriptionError();
  } else if (new_state == SPEECH_RECOGNIZER_READY) {
    if (recognizer_status_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer was initialized after being created, and
      // is ready to start recognizing speech.
      speech_recognizer_->Start();
    }
  }

  recognizer_status_ = new_state;
}

void ProjectorClientImpl::OnSpeechRecognitionStopped() {
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  controller_->OnSpeechRecognitionStopped();
}

void ProjectorClientImpl::SetTool(const ash::AnnotatorTool& tool) {
  ash::ProjectorAppClient::Get()->SetTool(tool);
}

// TODO(b/220202359): Implement undo.
void ProjectorClientImpl::Undo() {}

// TODO(b/220202359): Implement redo.
void ProjectorClientImpl::Redo() {}

void ProjectorClientImpl::Clear() {
  ash::ProjectorAppClient::Get()->Clear();
}

void ProjectorClientImpl::OnFileSystemMounted() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnFileSystemBeingUnmounted() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnFileSystemMountFailed() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user || !pref_change_registrar_.IsEmpty())
    return;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  pref_change_registrar_.Init(profile->GetPrefs());
  // TOOD(b/232043809): Consider using the disabled system feature policy
  // instead.
  pref_change_registrar_.Add(
      ash::prefs::kProjectorAllowByPolicy,
      base::BindRepeating(&ProjectorClientImpl::OnEnablementPolicyChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kProjectorDogfoodForFamilyLinkEnabled,
      base::BindRepeating(&ProjectorClientImpl::OnEnablementPolicyChanged,
                          base::Unretained(this)));
}

void ProjectorClientImpl::MaybeSwitchDriveIntegrationServiceObservation() {
  drive::DriveIntegrationService* drive_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  if (!drive_service || drive_observation_.IsObservingSource(drive_service))
    return;

  drive_observation_.Reset();
  drive_observation_.Observe(drive_service);
}

void ProjectorClientImpl::OnEnablementPolicyChanged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  // TODO(b/240497023): convert to dcheck once confirm that the pointer is
  // always available at this point.
  if (!swa_manager) {
    RecordPolicyChangeHandlingError(
        ash::ProjectorPolicyChangeHandlingError::kSwaManager);
    return;
  }
  const bool is_installed =
      swa_manager->IsSystemWebApp(ash::kChromeUITrustedProjectorSwaAppId);
  // We can't enable or disable the app if it's not already installed.
  if (!is_installed)
    return;

  const bool is_enabled = IsProjectorAppEnabled(profile);
  // The policy has changed to disallow the Projector app. Since we can't
  // uninstall the Projector SWA until the user signs out and back in, we should
  // close and disable the app for this current session.
  if (!is_enabled)
    CloseProjectorApp();

  auto* web_app_provider = ash::SystemWebAppManager::GetWebAppProvider(profile);
  // TODO(b/240497023): convert to dcheck once confirm that the pointer is
  // always available at this point.
  if (!web_app_provider) {
    RecordPolicyChangeHandlingError(
        ash::ProjectorPolicyChangeHandlingError::kWebAppProvider);
    return;
  }
  web_app_provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&ProjectorClientImpl::SetAppIsDisabled,
                                weak_ptr_factory_.GetWeakPtr(), !is_enabled));
}

void ProjectorClientImpl::SetAppIsDisabled(bool disabled) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  auto* web_app_provider = ash::SystemWebAppManager::GetWebAppProvider(profile);
  // TODO(b/240497023): convert to dcheck once confirm that the pointer is
  // always available at this point.
  if (!web_app_provider) {
    RecordPolicyChangeHandlingError(ash::ProjectorPolicyChangeHandlingError::
                                        kWebAppProviderOnRegistryReady);
    return;
  }
  auto* sync_bridge = &web_app_provider->sync_bridge();
  // TODO(b/240497023): convert to dcheck once confirm that the pointer is
  // always available at this point.
  if (!sync_bridge) {
    RecordPolicyChangeHandlingError(
        ash::ProjectorPolicyChangeHandlingError::kSyncBridge);
    return;
  }

  sync_bridge->SetAppIsDisabled(ash::kChromeUITrustedProjectorSwaAppId,
                                disabled);
}
