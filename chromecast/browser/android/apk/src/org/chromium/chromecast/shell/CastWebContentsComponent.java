// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.IBinder;
import android.os.PatternMatcher;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chromecast.base.Controller;
import org.chromium.content_public.browser.WebContents;

/**
 * A layer of indirection between CastContentWindowAndroid and CastWebContents(Activity|Service).
 * <p>
 * On builds with DISPLAY_WEB_CONTENTS_IN_SERVICE set to false, it will use CastWebContentsActivity,
 * otherwise, it will use CastWebContentsService.
 */
public class CastWebContentsComponent {
    /**
     * Callback interface for when the associated component is closed or the
     * WebContents is detached.
     */
    public interface OnComponentClosedHandler { void onComponentClosed(); }

    /**
     * Callback interface for when UI events occur.
     */
    public interface SurfaceEventHandler {
        void onVisibilityChange(int visibilityType);
    }

    /**
     * Params to start WebContents in activity or service.
     */
    static class StartParams {
        public final Context context;
        public final WebContents webContents;
        public final String appId;

        public StartParams(Context context, WebContents webContents, String appId) {
            this.context = context;
            this.webContents = webContents;
            this.appId = appId;
        }

        @Override
        public boolean equals(Object other) {
            if (other == this) {
                return true;
            }

            if (!(other instanceof StartParams)) {
                return false;
            }

            StartParams params = (StartParams) other;
            return params.context == this.context && params.webContents == this.webContents
                    && params.appId.equals(this.appId);
        }
    }

    @VisibleForTesting
    interface Delegate {
        void start(StartParams params);
        void stop(Context context);
    }

    @VisibleForTesting
    class ActivityDelegate implements Delegate {
        private static final String TAG = "CastWebContent_AD";
        private boolean mStarted;

        @Override
        public void start(StartParams params) {
            if (mStarted) return; // No-op if already started.
            if (DEBUG) Log.d(TAG, "start: SHOW_WEB_CONTENT in activity");
            startCastActivity(params.context, params.webContents, mEnableTouchInput,
                    mIsRemoteControlMode, mTurnOnScreen);
            mStarted = true;
        }

        @Override
        public void stop(Context context) {
            sendStopWebContentEvent();
            mStarted = false;
        }
    }

    private void startCastActivity(Context context, WebContents webContents, boolean enableTouch,
            boolean isRemoteControlMode, boolean turnOnScreen) {
        Intent intent = CastWebContentsIntentUtils.requestStartCastActivity(context, webContents,
                enableTouch, isRemoteControlMode, turnOnScreen, mKeepScreenOn, mSessionId);
        if (DEBUG) Log.d(TAG, "start activity by intent: " + intent);
        sResumeIntent.set(intent);
        context.startActivity(intent);
    }

    private void sendStopWebContentEvent() {
        Intent intent = CastWebContentsIntentUtils.requestStopWebContents(mSessionId);
        if (DEBUG) Log.d(TAG, "stop: send STOP_WEB_CONTENT intent: " + intent);
        sendIntentSync(intent);
        sResumeIntent.reset();
    }

    private class ServiceDelegate implements Delegate {
        private static final String TAG = "CastWebContent_SD";

        private ServiceConnection mConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {}

            @Override
            public void onServiceDisconnected(ComponentName name) {
                if (DEBUG) Log.d(TAG, "onServiceDisconnected");

                if (mComponentClosedHandler != null) mComponentClosedHandler.onComponentClosed();
            }
        };

        @Override
        public void start(StartParams params) {
            if (DEBUG) Log.d(TAG, "start");
            Intent intent = CastWebContentsIntentUtils.requestStartCastService(
                    params.context, params.webContents, mSessionId);
            params.context.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
        }

        @Override
        public void stop(Context context) {
            if (DEBUG) Log.d(TAG, "stop");
            context.unbindService(mConnection);
        }
    }

    public static final Controller<Intent> sResumeIntent = new Controller<Intent>();

    private static final String TAG = "CastWebComponent";
    private static final boolean DEBUG = true;

    private final OnComponentClosedHandler mComponentClosedHandler;
    private final String mSessionId;
    private final SurfaceEventHandler mSurfaceEventHandler;
    private final Controller<WebContents> mHasWebContentsState = new Controller<>();
    private Delegate mDelegate;
    private boolean mStarted;
    private boolean mEnableTouchInput;
    private final boolean mIsRemoteControlMode;
    private final boolean mTurnOnScreen;
    private final boolean mKeepScreenOn;

    public CastWebContentsComponent(String sessionId,
            OnComponentClosedHandler onComponentClosedHandler,
            SurfaceEventHandler surfaceEventHandler, boolean enableTouchInput,
            boolean isRemoteControlMode, boolean turnOnScreen, boolean keepScreenOn) {
        if (DEBUG) {
            Log.d(TAG,
                    "New CastWebContentsComponent. Instance ID: " + sessionId
                            + "; enableTouchInput:" + enableTouchInput
                            + "; isRemoteControlMode:" + isRemoteControlMode);
        }

        mComponentClosedHandler = onComponentClosedHandler;
        mEnableTouchInput = enableTouchInput;
        mSessionId = sessionId;
        mSurfaceEventHandler = surfaceEventHandler;
        mIsRemoteControlMode = isRemoteControlMode;
        mTurnOnScreen = turnOnScreen;
        mKeepScreenOn = keepScreenOn;

        mHasWebContentsState.subscribe(x -> {
            final IntentFilter filter = new IntentFilter();
            Uri instanceUri = CastWebContentsIntentUtils.getInstanceUri(sessionId);
            filter.addDataScheme(instanceUri.getScheme());
            filter.addDataAuthority(instanceUri.getAuthority(), null);
            filter.addDataPath(instanceUri.getPath(), PatternMatcher.PATTERN_LITERAL);
            filter.addAction(CastWebContentsIntentUtils.ACTION_ACTIVITY_STOPPED);
            filter.addAction(CastWebContentsIntentUtils.ACTION_ON_VISIBILITY_CHANGE);
            return new LocalBroadcastReceiverScope(filter, this ::onReceiveIntent);
        });
    }

    private void onReceiveIntent(Intent intent) {
        if (CastWebContentsIntentUtils.isIntentOfActivityStopped(intent)) {
            if (DEBUG) Log.d(TAG, "onReceive ACTION_ACTIVITY_STOPPED instance=" + mSessionId);
            if (mComponentClosedHandler != null) mComponentClosedHandler.onComponentClosed();
        } else if (CastWebContentsIntentUtils.isIntentOfVisibilityChange(intent)) {
            int visibilityType = CastWebContentsIntentUtils.getVisibilityType(intent);
            if (DEBUG) {
                Log.d(TAG,
                        "onReceive ACTION_ON_VISIBILITY_CHANGE instance=" + mSessionId
                                + "; visibilityType=" + visibilityType);
            }
            if (mSurfaceEventHandler != null) {
                mSurfaceEventHandler.onVisibilityChange(visibilityType);
            }
        }
    }

    @VisibleForTesting
    boolean isStarted() {
        return mStarted;
    }

    public void start(StartParams params, boolean isHeadless) {
        if (BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE || isHeadless) {
            if (DEBUG) Log.d(TAG, "Creating service delegate...");
            start(params, new ServiceDelegate());
        } else {
            if (DEBUG) Log.d(TAG, "Creating activity delegate...");
            start(params, new ActivityDelegate());
        }
    }

    @VisibleForTesting
    void start(StartParams params, Delegate delegate) {
        mDelegate = delegate;
        if (DEBUG) {
            Log.d(TAG,
                    "Starting WebContents with delegate: " + mDelegate.getClass().getSimpleName()
                            + "; Instance ID: " + mSessionId + "; App ID: " + params.appId);
        }
        mHasWebContentsState.set(params.webContents);
        mDelegate.start(params);
        mStarted = true;
    }

    public void stop(Context context) {
        if (!mStarted) return;
        if (DEBUG) {
            Log.d(TAG,
                    "stop with delegate: " + mDelegate.getClass().getSimpleName()
                            + "; Instance ID: " + mSessionId);
        }
        mHasWebContentsState.reset();
        if (DEBUG) Log.d(TAG, "Call delegate to stop");
        mDelegate.stop(context);
        mStarted = false;
    }

    public void enableTouchInput(boolean enabled) {
        if (DEBUG) Log.d(TAG, "enableTouchInput enabled:" + enabled);
        mEnableTouchInput = enabled;
        sendIntentSync(CastWebContentsIntentUtils.enableTouchInput(mSessionId, enabled));
    }

    public static void onComponentClosed(String sessionId) {
        if (DEBUG) Log.d(TAG, "onComponentClosed");
        sendIntentSync(CastWebContentsIntentUtils.onActivityStopped(sessionId));
    }

    public static void onVisibilityChange(String sessionId, int visibilityType) {
        if (DEBUG) Log.d(TAG, "onVisibilityChange");
        sendIntentSync(CastWebContentsIntentUtils.onVisibilityChange(sessionId, visibilityType));
    }

    private static boolean sendIntent(Intent in) {
        return CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcast(in);
    }

    private static void sendIntentSync(Intent in) {
        CastWebContentsIntentUtils.getLocalBroadcastManager().sendBroadcastSync(in);
    }
}
