// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Arrays;
import java.util.Collection;

/** Tests for password update dialog. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PasswordEditDialogControllerTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final String INITIAL_USERNAME = USERNAMES[INITIAL_USERNAME_INDEX];
    private static final String CHANGED_USERNAME = "user3";
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String ACCOUNT_NAME = "foo@bar.com";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private PasswordEditDialogCoordinator.Delegate mDelegateMock;

    private FakeModalDialogManager mModalDialogManager = new FakeModalDialogManager(0);

    @Mock
    private PasswordEditDialogView mDialogViewMock;

    private PropertyModel mCustomViewModel;
    private PropertyModel mModalDialogModel;

    private PasswordEditDialogCoordinator mDialogCoordinator;
    private boolean mIsSignedIn;

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {/*isSignedIn=*/false, /*isSignedIn=*/true});
    }

    public PasswordEditDialogControllerTest(boolean isSignedIn) {
        mIsSignedIn = isSignedIn;
    }

    /**
     * Tests that properties of password edit modal dialog and custom view are set correctly
     * based on passed parameters when the details feature is disabled.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUpdatePasswordDialogPropertiesFeatureDisabled() {
        createAndShowDialog(true);
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.confirm_username_dialog_title));
        Assert.assertThat("Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES), contains(USERNAMES));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals("Password doesn't match", INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNull(
                "Footer should be null", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNull("No title icon is expected",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
    }

    /**
     * Tests that properties of update modal dialog and custom view are set correctly
     * based on passed parameters when the details feature is enabled.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUpdatePasswordDialogPropertiesFeatureEnabled() {
        createAndShowDialog(true);
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.password_update_dialog_title));
        Assert.assertThat("Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES), contains(USERNAMES));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals("Password doesn't match", INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull("There should be a title icon",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
        Assert.assertEquals(mModalDialogManager.getShownDialogModel().get(
                                    ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.password_manager_update_button));
        if (mIsSignedIn) {
            Assert.assertTrue("Footer should contain user account name",
                    mCustomViewModel.get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /**
     * Tests that properties of save modal dialog and custom view are set correctly based on passed
     * parameters when the details feature is enabled.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testSavePasswordDialogPropertiesFeatureEnabled() {
        createAndShowDialog(false);
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.save_password));
        // Save dialog has only one username in usernames list - the one the user's just entered
        Assert.assertThat("Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES),
                contains(new String[] {INITIAL_USERNAME}));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals("Password doesn't match", INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull("There should be a title icon",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
        Assert.assertEquals(mModalDialogManager.getShownDialogModel().get(
                                    ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.password_manager_save_button));
        if (mIsSignedIn) {
            Assert.assertTrue("Footer should contain user account name",
                    mCustomViewModel.get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /** Tests that the username entered in the layout propagates to the model. */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUsernameChanged() {
        createAndShowDialog(false);

        Callback<String> usernameChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameChangedCallback.onResult(CHANGED_USERNAME);
        Assert.assertEquals("Selected username doesn't match", CHANGED_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
    }

    /**
     * Tests that correct username and password are propagated to the dialog accepted delegate
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogIsAcceptedWithCorrectUsernameAndPassword() {
        createAndShowDialog(false);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        Assert.assertThat(
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME), is(INITIAL_USERNAME));
        Assert.assertThat(
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD), is(INITIAL_PASSWORD));

        mCustomViewModel.set(PasswordEditDialogProperties.USERNAME, CHANGED_USERNAME);
        mCustomViewModel.set(PasswordEditDialogProperties.PASSWORD, CHANGED_PASSWORD);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        Mockito.verify(mDelegateMock).onDialogAccepted(CHANGED_USERNAME, CHANGED_PASSWORD);
        Mockito.verify(mDelegateMock).onDialogDismissed(true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testEmptyPasswordError() {
        createAndShowDialog(true);

        Callback<String> passwordChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult("");
        Assert.assertTrue("Accept button should be disabled when user enters empty password",
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        Assert.assertTrue("Error should be displayed when user enters empty password",
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_ERROR) != null
                        && !mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_ERROR)
                                    .isEmpty());
    }

    /** Tests that changing password in editText gets reflected in the model. */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testPasswordChanging() {
        createAndShowDialog(true);

        Callback<String> passwordChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult(CHANGED_PASSWORD);
        Assert.assertEquals("Password doesn't match to the expected", CHANGED_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
    }

    /**
     * Tests that the dialog is dismissed when dismiss() is called from native code.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedFromNative() {
        createAndShowDialog(false);

        mDialogCoordinator.dismiss();
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mDelegateMock).onDialogDismissed(false);
    }

    /**
     * Tests that the dialog is dismissed when negative button callback is triggered.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedWithNegativeButton() {
        createAndShowDialog(true);

        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mDelegateMock).onDialogDismissed(false);
    }

    /**
     * Helper function that creates {@link PasswordEditDialogCoordinator},
     * and captures property models for modal dialog and custom dialog view.
     *
     * @param isUpdate Defines whether Save password or Update password dialog will be shown
     */
    private void createAndShowDialog(boolean isUpdate) {
        mDialogCoordinator = new PasswordEditDialogCoordinator(RuntimeEnvironment.getApplication(),
                mModalDialogManager, mDialogViewMock, mDelegateMock);
        if (isUpdate) {
            mDialogCoordinator.showUpdatePasswordDialog(
                    USERNAMES, INITIAL_USERNAME_INDEX, INITIAL_PASSWORD, ACCOUNT_NAME);
        } else {
            mDialogCoordinator.showSavePasswordDialog(
                    INITIAL_USERNAME, INITIAL_PASSWORD, mIsSignedIn ? ACCOUNT_NAME : null);
        }

        mModalDialogModel = mDialogCoordinator.getDialogModelForTesting();
        mCustomViewModel = mDialogCoordinator.getDialogViewModelForTesting();
    }
}
