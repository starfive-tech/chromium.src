// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.browserfragment.interfaces.IStringCallback;
import org.chromium.browserfragment.interfaces.ITabProxy;
import org.chromium.browserfragment.interfaces.IWebMessageCallback;

import java.util.List;

/**
 * This class acts as a proxy between a Tab object in the embedding app
 * and the Tab implementation in WebLayer.
 */
class TabProxy extends ITabProxy.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private int mTabId;
    private String mGuid;

    TabProxy(Tab tab) {
        mTabId = tab.getId();
        mGuid = tab.getGuid();
    }

    void invalidate() {
        mTabId = -1;
        mGuid = null;
    }

    boolean isValid() {
        return mGuid != null;
    }

    private Tab getTab() {
        Tab tab = Tab.getTabById(mTabId);
        if (tab == null) {
            // TODO(swestphal): Raise exception.
        }
        return tab;
    }

    @Override
    public void setActive() {
        mHandler.post(() -> {
            Tab tab = getTab();
            tab.getBrowser().setActiveTab(tab);
        });
    }

    @Override
    public void close() {
        mHandler.post(() -> {
            getTab().dispatchBeforeUnloadAndClose();
            invalidate();
        });
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IStringCallback callback) {
        mHandler.post(() -> {
            // TODO(rayankans): Verify the tab's origin is 1P.
            getTab().executeScript(script, useSeparateIsolate, (String result) -> {
                try {
                    callback.onResult(result);
                } catch (RemoteException e) {
                }
            });
        });
    }

    @Override
    public void registerWebMessageCallback(
            IWebMessageCallback callback, String jsObjectName, List<String> allowedOrigins) {
        mHandler.post(() -> {
            getTab().registerWebMessageCallback(new WebMessageCallback() {
                @Override
                public void onWebMessageReceived(
                        WebMessageReplyProxy replyProxy, WebMessage message) {
                    try {
                        callback.onWebMessageReceived(
                                new WebMessageReplyProxyProxy(replyProxy), message.getContents());
                    } catch (RemoteException e) {
                    }
                }

                @Override
                public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {
                    try {
                        callback.onWebMessageReplyProxyClosed(
                                new WebMessageReplyProxyProxy(replyProxy));
                    } catch (RemoteException e) {
                    }
                }

                @Override
                public void onWebMessageReplyProxyActiveStateChanged(
                        WebMessageReplyProxy replyProxy) {
                    try {
                        callback.onWebMessageReplyProxyActiveStateChanged(
                                new WebMessageReplyProxyProxy(replyProxy));
                    } catch (RemoteException e) {
                    }
                }
            }, jsObjectName, allowedOrigins);
        });
    }

    @Override
    public void unregisterWebMessageCallback(String jsObjectName) {
        mHandler.post(() -> { getTab().unregisterWebMessageCallback(jsObjectName); });
    }
}
