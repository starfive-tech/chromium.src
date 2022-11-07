// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.view.MenuItem;
import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.DetailItemType;
import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.AutofillProfileItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.CreditCardItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.FooterItemProperties;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.components.autofill_assistant.AutofillAssistantPublicTags;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the FastCheckout component. It sets the state of the model and reacts
 * to events like clicks.
 */
public class FastCheckoutMediator {
    private PropertyModel mModel;
    private FastCheckoutComponent.Delegate mDelegate;
    private BottomSheetController mBottomSheetController;
    private BottomSheetObserver mBottomSheetClosedObserver;
    private BottomSheetObserver mBottomSheetDismissedObserver;

    void initialize(FastCheckoutComponent.Delegate delegate, PropertyModel model,
            BottomSheetController bottomSheetController) {
        mModel = model;
        mDelegate = delegate;
        mBottomSheetController = bottomSheetController;

        mBottomSheetClosedObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                if (newContent == null) {
                    mModel.set(FastCheckoutProperties.VISIBLE, true);
                    mBottomSheetController.removeObserver(mBottomSheetClosedObserver);
                }
            }
        };

        mBottomSheetDismissedObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                dismiss(reason);
                mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
            }
        };

        mModel.set(FastCheckoutProperties.HOME_SCREEN_DELEGATE, createHomeScreenDelegate());
        mModel.set(FastCheckoutProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN));
    }

    /** Returns an implementation the {@link HomeScreenCoordinator.Delegate} interface. */
    private HomeScreenCoordinator.Delegate createHomeScreenDelegate() {
        return new HomeScreenCoordinator.Delegate() {
            @Override
            public void onOptionsAccepted() {
                if (!mModel.get(FastCheckoutProperties.VISIBLE)) {
                    return; // Dismiss only if not dismissed yet.
                }
                FastCheckoutAutofillProfile profile =
                        mModel.get(FastCheckoutProperties.SELECTED_PROFILE);
                FastCheckoutCreditCard creditCard =
                        mModel.get(FastCheckoutProperties.SELECTED_CREDIT_CARD);
                assert profile != null && creditCard != null;
                mModel.set(FastCheckoutProperties.VISIBLE, false);
                mDelegate.onOptionsSelected(profile, creditCard);
            };

            @Override
            public void onDismiss() {
                if (!mModel.get(FastCheckoutProperties.VISIBLE)) {
                    return; // Dismiss only if not dismissed yet.
                }
                mModel.set(FastCheckoutProperties.VISIBLE, false);
                mDelegate.onDismissed();
            }

            @Override
            public void onShowAddressesList() {
                setCurrentScreen(FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN);
            }

            @Override
            public void onShowCreditCardList() {
                setCurrentScreen(FastCheckoutProperties.ScreenType.CREDIT_CARD_SCREEN);
            }
        };
    }

    public void showOptions(
            FastCheckoutAutofillProfile[] profiles, FastCheckoutCreditCard[] creditCards) {
        setAutofillProfileItems(profiles);
        setCreditCardItems(creditCards);

        // It is possible that FC onboarding has been just accepted but the bottom sheet is still
        // showing. If that's the case we try hiding it and then show FC bottom sheet.
        if (isOnboardingSheet()) {
            // Delay showing the bottom sheet until the consent sheet is fully closed.
            mBottomSheetController.addObserver(mBottomSheetClosedObserver);
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), /* animate= */ true);
        } else {
            mModel.set(FastCheckoutProperties.VISIBLE, true);
        }
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @param content The bottom sheet content to show/hide.
     * @return True if the request was successful, false otherwise.
     */
    public boolean setVisible(boolean isVisible, BottomSheetContent content) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetDismissedObserver);
            if (!mBottomSheetController.requestShowContent(content, true)) {
                mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(content, true);
        }
        return true;
    }

    /**
     * Dismisses the current bottom sheet.
     */
    public void dismiss(@StateChangeReason int reason) {
        if (!mModel.get(FastCheckoutProperties.VISIBLE)) {
            return; // Dismiss only if not dismissed yet.
        }
        // TODO(crbug.com/1334642): Record dismissal metrics.
        mModel.set(FastCheckoutProperties.VISIBLE, false);
        mDelegate.onDismissed();
    }

    private boolean isOnboardingSheet() {
        if (mBottomSheetController == null
                || mBottomSheetController.getCurrentSheetContent() == null) {
            return false;
        }

        View view = mBottomSheetController.getCurrentSheetContent().getContentView();
        return view.getTag() != null
                && view.getTag().equals(
                        AutofillAssistantPublicTags.AUTOFILL_ASSISTANT_BOTTOM_SHEET_CONTENT_TAG);
    }

    /**
     * Sets the Autofill profile items and creates the corresponding models for the
     * profile item entries on the Autofill profiles page.
     * If there is a selected Autofill profile prior to calling this method, the profile
     * with the same GUID will remain selected. If no prior selection was made or this
     * GUID no longer exists, the first Autofill profile is selected.
     * @param profiles The array of FastCheckoutAutofillProfile to set as Autofill profiles.
     */
    public void setAutofillProfileItems(FastCheckoutAutofillProfile[] profiles) {
        assert profiles != null && profiles.length != 0;

        FastCheckoutAutofillProfile previousSelection =
                mModel.get(FastCheckoutProperties.SELECTED_PROFILE);
        FastCheckoutAutofillProfile newSelection = profiles[0];

        // Populate all model entries.
        ModelList profileItems = mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST);
        profileItems.clear();
        for (FastCheckoutAutofillProfile profile : profiles) {
            if (previousSelection != null
                    && profile.getGUID().equals(previousSelection.getGUID())) {
                newSelection = profile;
            }
            PropertyModel model = AutofillProfileItemProperties.create(
                    /*profile=*/profile, /*isSelected=*/false, /*onClickListener=*/() -> {
                        setSelectedAutofillProfile(profile);
                        setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN);
                    });
            profileItems.add(new ListItem(DetailItemType.PROFILE, model));
        }

        // Add the footer item.
        profileItems.add(new ListItem(DetailItemType.FOOTER,
                FooterItemProperties.create(
                        /*label=*/R.string.fast_checkout_detail_screen_add_autofill_profile_text,
                        /*onClickHandler=*/() -> mDelegate.openAutofillProfileSettings())));

        setSelectedAutofillProfile(newSelection);
    }

    /**
     * Sets the selected Autofill profile and updates the IS_SELECTED entry in the models
     * of the profile item entries on the Autofill profiles page.
     * @param selectedProfile The profile that is to be selected.
     */
    public void setSelectedAutofillProfile(FastCheckoutAutofillProfile selectedProfile) {
        assert selectedProfile != null;
        mModel.set(FastCheckoutProperties.SELECTED_PROFILE, selectedProfile);

        int foundProfiles = 0;
        ModelList allItems = mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST);
        for (ListItem item : allItems) {
            if (item.type != DetailItemType.PROFILE) {
                continue;
            }
            boolean isSelected = selectedProfile.equals(
                    item.model.get(AutofillProfileItemProperties.AUTOFILL_PROFILE));
            item.model.set(AutofillProfileItemProperties.IS_SELECTED, isSelected);
            if (isSelected) {
                ++foundProfiles;
            }
        }

        // Exactly one of the models must contain the selected profile.
        assert foundProfiles == 1;
    }

    /**
     * Sets the credit card items and creates the corresponding models for the
     * credit card item entries on the credit card page.
     * If there is a selected credit card prior to calling this method, the card
     * with the same GUID will remain selected. If no prior selection was made or this
     * GUID no longer exists, the first credit card is selected.
     * @param creditCards The array of FastCheckoutCreditCard to set as credit cards.
     */
    public void setCreditCardItems(FastCheckoutCreditCard[] creditCards) {
        assert creditCards != null && creditCards.length != 0;

        FastCheckoutCreditCard previousSelection =
                mModel.get(FastCheckoutProperties.SELECTED_CREDIT_CARD);
        FastCheckoutCreditCard newSelection = creditCards[0];

        // Populate all model entries.
        ModelList cardItems = mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST);
        cardItems.clear();
        for (FastCheckoutCreditCard card : creditCards) {
            if (previousSelection != null && card.getGUID().equals(previousSelection.getGUID())) {
                newSelection = card;
            }
            PropertyModel model = CreditCardItemProperties.create(
                    /*creditCard=*/card, /*isSelected=*/false, /*onClickListener=*/() -> {
                        setSelectedCreditCard(card);
                        setCurrentScreen(FastCheckoutProperties.ScreenType.HOME_SCREEN);
                    });
            ListItem item = new ListItem(DetailItemType.CREDIT_CARD, model);
            cardItems.add(item);
        }

        // Add the footer item.
        cardItems.add(new ListItem(DetailItemType.FOOTER,
                FooterItemProperties.create(
                        /*label=*/R.string.fast_checkout_detail_screen_add_credit_card_text,
                        /*onClickHandler=*/() -> mDelegate.openCreditCardSettings())));

        setSelectedCreditCard(newSelection);
    }

    /**
     * Sets the selected credit card and updates the IS_SELECTED entry in the models
     * of the credit card item entries on the credit card page.
     * @param selectedCreditCard The credit card that is to be selected.
     */
    public void setSelectedCreditCard(FastCheckoutCreditCard selectedCreditCard) {
        assert selectedCreditCard != null;
        mModel.set(FastCheckoutProperties.SELECTED_CREDIT_CARD, selectedCreditCard);

        int foundCards = 0;
        ModelList allItems = mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST);
        for (ListItem item : allItems) {
            if (item.type != DetailItemType.CREDIT_CARD) {
                continue;
            }
            boolean isSelected =
                    selectedCreditCard.equals(item.model.get(CreditCardItemProperties.CREDIT_CARD));
            item.model.set(CreditCardItemProperties.IS_SELECTED, isSelected);
            if (isSelected) {
                ++foundCards;
            }
        }

        // Exactly one of the models must contain the selected credit card.
        assert foundCards == 1;
    }

    /**
     * Selects the currently shown screen on the bottomsheet.
     * @param screenType A {@link FastCheckoutProperties.ScreenType} that defines the screen to be
     *         shown.
     */
    public void setCurrentScreen(int screenType) {
        if (screenType == FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN) {
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_TITLE,
                    R.string.fast_checkout_autofill_profile_sheet_title);
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE,
                    R.string.fast_checkout_autofill_profile_settings_button_description);
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    createSettingsOnClickListener(() -> mDelegate.openAutofillProfileSettings()));
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(FastCheckoutProperties.PROFILE_MODEL_LIST));
        } else if (screenType == ScreenType.CREDIT_CARD_SCREEN) {
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_TITLE,
                    R.string.fast_checkout_credit_card_sheet_title);
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_MENU_TITLE,
                    R.string.fast_checkout_credit_card_settings_button_description);
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_SETTINGS_CLICK_HANDLER,
                    createSettingsOnClickListener(() -> mDelegate.openCreditCardSettings()));
            mModel.set(FastCheckoutProperties.DETAIL_SCREEN_MODEL_LIST,
                    mModel.get(FastCheckoutProperties.CREDIT_CARD_MODEL_LIST));
        }

        mModel.set(FastCheckoutProperties.CURRENT_SCREEN, screenType);
    }

    /**
     * Creates and returns an {@link Toolbar.OnMenuItemClickListener} that
     * executes a Runnable if and only if the Settings MenuItem was clicked.
     * @param runnable The Runnable to execute.
     */
    static OnMenuItemClickListener createSettingsOnClickListener(Runnable runnable) {
        return new OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.settings_menu_id) {
                    runnable.run();
                    return true;
                }
                return false;
            }
        };
    }

    /**
     * Releases the resources used by FastCheckoutMediator.
     */
    @MainThread
    public void destroy() {
        // TODO(crbug.com/1334642): Record as part of the dismissal metrics.
        mModel.set(FastCheckoutProperties.VISIBLE, false);
    }
}
