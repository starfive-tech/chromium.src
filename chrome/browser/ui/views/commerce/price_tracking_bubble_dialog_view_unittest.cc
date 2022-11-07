// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/views/chrome_test_widget.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {
const char kTestURL[] = "http://www.google.com";
const char16_t kTestBookmarkTitle[] = u"Bookmark title";
}  // namespace

class PriceTrackingBubbleDialogViewUnitTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    anchor_widget_ =
        views::UniqueWidgetPtr(std::make_unique<ChromeTestWidget>());
    views::Widget::InitParams widget_params;
    widget_params.context = GetContext();
    anchor_widget_->Init(std::move(widget_params));

    bubble_coordinator_ = std::make_unique<PriceTrackingBubbleCoordinator>(
        anchor_widget_->GetContentsView());

    SetUpDependencies();
  }

  void TearDown() override {
    // Make sure the bubble is destroyed before the profile to avoid a crash.
    if (bubble_coordinator_->GetBubble()) {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          bubble_coordinator_->GetBubble()->GetWidget());
      bubble_coordinator_->GetBubble()->GetWidget()->Close();
      destroyed_waiter.Wait();
    }

    anchor_widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories = {
        {BookmarkModelFactory::GetInstance(),
         BookmarkModelFactory::GetDefaultFactory()}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  void SetUpDependencies() {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kTestURL),
                                  kTestBookmarkTitle);
  }

  void CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type type) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bubble_coordinator_->Show(browser()->tab_strip_model()->GetWebContentsAt(0),
                              profile(), GURL(kTestURL),
                              ui::ImageModel::FromImage(gfx::Image(
                                  gfx::ImageSkia::CreateFrom1xBitmap(bitmap))),
                              Callback().Get(), type);
  }

  base::MockCallback<PriceTrackingBubbleDialogView::OnTrackPriceCallback>&
  Callback() {
    return callback_;
  }

  PriceTrackingBubbleCoordinator* BubbleCoordinator() {
    return bubble_coordinator_.get();
  }

  const std::u16string& GetMostRecentlyModifiedUserBookmarkFolderName() {
    bookmarks::BookmarkModel* const model =
        BookmarkModelFactory::GetForBrowserContext(profile());
    std::vector<const bookmarks::BookmarkNode*> nodes =
        bookmarks::GetMostRecentlyModifiedUserFolders(model, 1);
    return nodes[0]->GetTitle();
  }

 private:
  views::UniqueWidgetPtr anchor_widget_;
  base::MockCallback<PriceTrackingBubbleDialogView::OnTrackPriceCallback>
      callback_;
  std::unique_ptr<PriceTrackingBubbleCoordinator> bubble_coordinator_;
};

TEST_F(PriceTrackingBubbleDialogViewUnitTest, FUEBubble) {
  CreateBubbleViewAndShow(
      PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE_FIRST_RUN));

  EXPECT_TRUE(bubble->GetBodyLabelForTesting());
  EXPECT_EQ(bubble->GetBodyLabelForTesting()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_OMNIBOX_TRACK_PRICE_DIALOG_DESCRIPTION_FIRST_RUN,
                GetMostRecentlyModifiedUserBookmarkFolderName()));

  EXPECT_EQ(
      bubble->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_ACTION_BUTTON));
  EXPECT_EQ(
      bubble->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL),
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_CANCEL_BUTTON));
}

TEST_F(PriceTrackingBubbleDialogViewUnitTest, NormalBubble) {
  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  EXPECT_EQ(bubble->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE_DIALOG_TITLE));

  EXPECT_TRUE(bubble->GetBodyLabelForTesting());
  EXPECT_EQ(bubble->GetBodyLabelForTesting()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_OMNIBOX_TRACKING_PRICE_DIALOG_DESCRIPTION,
                GetMostRecentlyModifiedUserBookmarkFolderName()));
  EXPECT_TRUE(bubble->GetBodyLabelForTesting()->GetFirstLinkForTesting());

  EXPECT_EQ(bubble->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACKING_PRICE_DIALOG_ACTION_BUTTON));
  EXPECT_EQ(bubble->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL),
            l10n_util::GetStringUTF16(
                IDS_OMNIBOX_TRACKING_PRICE_DIALOG_UNTRACK_BUTTON));
}

TEST_F(PriceTrackingBubbleDialogViewUnitTest, AcceptFUEBubble) {
  CreateBubbleViewAndShow(
      PriceTrackingBubbleDialogView::Type::TYPE_FIRST_USE_EXPERIENCE);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);
  EXPECT_CALL(Callback(), Run(true));
  bubble->Accept();
}

TEST_F(PriceTrackingBubbleDialogViewUnitTest, CancelNormalBubble) {
  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);
  EXPECT_CALL(Callback(), Run(false));
  bubble->Cancel();
}

TEST_F(PriceTrackingBubbleDialogViewUnitTest, ClickLinkInTheNormalBubble) {
  CreateBubbleViewAndShow(PriceTrackingBubbleDialogView::Type::TYPE_NORMAL);

  auto* bubble = BubbleCoordinator()->GetBubble();
  EXPECT_TRUE(bubble);

  auto bookmark_editor_waiter = views::NamedWidgetShownWaiter(
      views::test::AnyWidgetTestPasskey{}, BookmarkEditorView::kViewClassName);

  bubble->GetBodyLabelForTesting()->ClickFirstLinkForTesting();
  EXPECT_TRUE(bookmark_editor_waiter.WaitIfNeededAndGet());

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(BubbleCoordinator()->GetBubble());
}
