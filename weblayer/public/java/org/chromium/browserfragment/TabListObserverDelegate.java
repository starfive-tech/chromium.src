// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.browserfragment.interfaces.ITabListObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabParams;

/**
 * {@link TabListObserverDelegate} notifies {@link TabListObserver}s of events in the Browser.
 */
class TabListObserverDelegate extends ITabListObserverDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private ObserverList<TabListObserver> mTabListObservers = new ObserverList<TabListObserver>();

    /**
     * Register a TabListObserver.
     *
     * @return true if the observer was added to the list of observers.
     */
    boolean registerObserver(TabListObserver tabListObserver) {
        return mTabListObservers.addObserver(tabListObserver);
    }

    /**
     * Unregister a TabListObserver.
     *
     * @return true if the observer was removed from the list of observers.
     */
    boolean unregisterObserver(TabListObserver tabListObserver) {
        return mTabListObservers.removeObserver(tabListObserver);
    }

    @Override
    public void notifyActiveTabChanged(@Nullable ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = null;
            if (tabParams != null) {
                tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            }
            for (TabListObserver observer : mTabListObservers) {
                observer.onActiveTabChanged(tab);
            }
        });
    }

    @Override
    public void notifyTabAdded(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            for (TabListObserver observer : mTabListObservers) {
                observer.onTabAdded(tab);
            }
        });
    }

    @Override
    public void notifyTabRemoved(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            for (TabListObserver observer : mTabListObservers) {
                observer.onTabRemoved(tab);
            }
            TabRegistry.getInstance().removeTab(tab);
        });
    }

    @Override
    public void notifyWillDestroyBrowserAndAllTabs() {
        mHandler.post(() -> {
            for (TabListObserver observer : mTabListObservers) {
                observer.onWillDestroyBrowserAndAllTabs();
            }
        });
    }
}