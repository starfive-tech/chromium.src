// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"

#include "permission_prompt_chip_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

PermissionPromptChip::PermissionPromptChip(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  DCHECK(delegate_);

  LocationBarView* lbv = GetLocationBarView();
  if (!lbv->chip_controller()->chip()) {
    lbv->CreateChip();
  }

  chip_controller_ = lbv->chip_controller();
  chip_controller_->ShowPermissionPrompt(delegate_);
}

PermissionPromptChip::~PermissionPromptChip() {
  chip_controller_->FinalizePermissionPromptChip();
}

void PermissionPromptChip::UpdateAnchor() {
  UpdateBrowser();

  LocationBarView* lbv = GetLocationBarView();
  const bool is_location_bar_drawn =
      lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen();

  if (chip_controller_->IsPermissionPromptChipVisible() &&
      !is_location_bar_drawn) {
    chip_controller_->FinalizePermissionPromptChip();
    if (delegate_) {
      chip_controller_->UpdateBrowser(browser());
      delegate_->RecreateView();
    }
  }
}

permissions::PermissionPromptDisposition
PermissionPromptChip::GetPromptDisposition() const {
  if (delegate()->ShouldCurrentRequestUseQuietUI()) {
    return permissions::PermissionUiSelector::ShouldSuppressAnimation(
               delegate()->ReasonForUsingQuietUi())
               ? permissions::PermissionPromptDisposition::
                     LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP
               : permissions::PermissionPromptDisposition::
                     LOCATION_BAR_LEFT_QUIET_CHIP;
  }

  return permissions::PermissionUtil::ShouldPermissionBubbleStartOpen(
             delegate())
             ? permissions::PermissionPromptDisposition::
                   LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
             : permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP;
}

views::Widget* PermissionPromptChip::GetPromptBubbleWidgetForTesting() {
  CHECK_IS_TEST();
  LocationBarView* lbv = GetLocationBarView();

  return chip_controller_->IsPermissionPromptChipVisible() &&
                 lbv->chip_controller()->IsBubbleShowing()
             ? lbv->chip_controller()->GetPromptBubbleWidget()
             : nullptr;
}
