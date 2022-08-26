// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.ITabNavigationControllerProxy;

class TabNavigationControllerProxy extends ITabNavigationControllerProxy.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final NavigationController mNavigationController;

    TabNavigationControllerProxy(NavigationController navigationController) {
        mNavigationController = navigationController;
    }

    @Override
    public void navigate(String uri) {
        mHandler.post(() -> {
            NavigateParams.Builder navigateParamsBuilder =
                    new NavigateParams.Builder().disableIntentProcessing();
            mNavigationController.navigate(Uri.parse(uri), navigateParamsBuilder.build());
        });
    }

    @Override
    public void goBack() {
        mHandler.post(() -> { mNavigationController.goBack(); });
    }

    @Override
    public void goForward() {
        mHandler.post(() -> { mNavigationController.goForward(); });
    }

    @Override
    public void canGoBack(IBooleanCallback callback) {
        mHandler.post(() -> {
            try {
                callback.onResult(mNavigationController.canGoBack());
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void canGoForward(IBooleanCallback callback) {
        mHandler.post(() -> {
            try {
                callback.onResult(mNavigationController.canGoForward());
            } catch (RemoteException e) {
            }
        });
    }
}