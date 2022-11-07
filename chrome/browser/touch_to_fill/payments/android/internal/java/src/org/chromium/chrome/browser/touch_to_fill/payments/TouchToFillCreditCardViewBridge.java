// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ TouchToFillCreditCardViewImpl. Delegates calls from native to Java.
 */
@JNINamespace("autofill")
@JNIAdditionalImport(TouchToFillCreditCardComponent.class)
class TouchToFillCreditCardViewBridge {
    private final TouchToFillCreditCardComponent mComponent;

    private TouchToFillCreditCardViewBridge(TouchToFillCreditCardComponent.Delegate delegate,
            Context context, BottomSheetController bottomSheetController) {
        mComponent = new TouchToFillCreditCardCoordinator();
        mComponent.initialize(context, bottomSheetController, delegate);
    }

    @CalledByNative
    private static @Nullable TouchToFillCreditCardViewBridge create(
            TouchToFillCreditCardComponent.Delegate delegate, WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillCreditCardViewBridge(delegate, context, bottomSheetController);
    }

    @CalledByNative
    private void showSheet() {
        mComponent.showSheet();
    }

    @CalledByNative
    private void hideSheet() {
        mComponent.hideSheet();
    }
}
