// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view_class_properties.h"

// static
views::BubbleDialogModelHost* BatterySaverBubbleView::CreateBubble(
    Browser* browser,
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    BatterySaverBubbleObserver* observer) {
  auto bubble_delegate_unique =
      std::make_unique<BatterySaverBubbleDelegate>(browser, observer);
  auto* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUBBLE_TITLE))
          .SetIsAlertDialog()
          .SetDialogDestroyingCallback(
              base::BindOnce(&BatterySaverBubbleDelegate::OnWindowClosing,
                             base::Unretained(bubble_delegate)))
          .AddParagraph(
              ui::DialogModelLabel(IDS_BATTERY_SAVER_BUBBLE_DESCRIPTION)
                  .set_is_secondary()
                  .set_allow_character_break())
          .AddExtraButton(
              base::BindRepeating(
                  &BatterySaverBubbleDelegate::OnSessionOffClicked,
                  base::Unretained(bubble_delegate)),
              l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_SESSION_TURN_OFF))
          .Build();

  auto bubble_unique = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, anchor_position);
  auto* bubble = bubble_unique.get();

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_unique));
  widget->Show();

  observer->OnBubbleShown();
  return bubble;
}

// static
void BatterySaverBubbleView::CloseBubble(
    views::BubbleDialogModelHost* bubble_dialog) {
  DCHECK(bubble_dialog);
  bubble_dialog->Close();
}
