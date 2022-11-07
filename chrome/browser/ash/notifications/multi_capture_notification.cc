// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notification.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

const char kMultiCaptureId[] = "multi_capture";
const char kNotifierMultiCapture[] = "ash.multi_capture";

std::unique_ptr<message_center::Notification> CreateNotification(
    const url::Origin& origin) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierMultiCapture,
      ash::NotificationCatalogName::kMultiCapture);

  const std::string host = origin.host();
  std::u16string converted_host;
  if (!base::UTF8ToUTF16(host.c_str(), host.size(), &converted_host)) {
    NOTREACHED();
    return nullptr;
  }

  // TODO(crbug.com/1356101): Add "Don't show again" for managed sessions.
  return ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kMultiCaptureId,
      /*title=*/u"",
      /*message=*/
      l10n_util::GetStringFUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_MESSAGE,
                                 converted_host),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(), notifier_id,
      /*optional_fields=*/message_center::RichNotificationData(),
      /*delegate=*/
      new message_center::HandleNotificationClickDelegate(
          message_center::HandleNotificationClickDelegate::ButtonClickCallback(
              base::DoNothing())),
      ash::kSystemTrayRecordingIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

}  // namespace

namespace ash {

MultiCaptureNotification::MultiCaptureNotification() {
  DCHECK(Shell::HasInstance());
  multi_capture_service_client_observation_.Observe(
      Shell::Get()->multi_capture_service_client());
}

MultiCaptureNotification::~MultiCaptureNotification() = default;

void MultiCaptureNotification::MultiCaptureStarted(const std::string& label,
                                                   const url::Origin& origin) {
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification(origin);
  // TODO(crbug.com/1356102): Make sure the notification does not disappear
  // automatically after some time.
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void MultiCaptureNotification::MultiCaptureStopped(const std::string& label) {}

void MultiCaptureNotification::MultiCaptureServiceClientDestroyed() {
  multi_capture_service_client_observation_.Reset();
}

}  // namespace ash
