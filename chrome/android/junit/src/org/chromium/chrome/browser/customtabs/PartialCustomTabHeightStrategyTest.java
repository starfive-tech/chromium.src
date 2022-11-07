// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.animation.Animator.AnimatorListener;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link PartialCustomTabHandleStrategy}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabHeightStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    // Pixel 3 XL metrics
    private static final int DEVICE_HEIGHT = 2960;
    private static final int DEVICE_WIDTH = 1440;

    private static final int NAVBAR_HEIGHT = 160;
    private static final int MAX_INIT_POS = DEVICE_HEIGHT / 2;
    private static final int INITIAL_HEIGHT = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;
    private static final int FULL_HEIGHT = DEVICE_HEIGHT - NAVBAR_HEIGHT;
    private static final int MULTIWINDOW_HEIGHT = FULL_HEIGHT / 2;
    private static final int STATUS_BAR_HEIGHT = 68;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    // Parameterize the internal implementation. 'Fixed window' type does not resize the Window
    // but the WebContents, while 'Window above navbar' type dynamically resizes the Window.
    @Parameter(0)
    public boolean mWindowAboveNavbar;

    @Mock
    private Activity mActivity;
    @Mock
    private Window mWindow;
    @Mock
    private WindowManager mWindowManager;
    @Mock
    private Resources mResources;
    @Mock
    private Configuration mConfiguration;
    private WindowManager.LayoutParams mAttributes;
    @Mock
    private ViewStub mHandleViewStub;
    @Mock
    private ImageView mHandleView;
    @Mock
    private View mDecorView;
    @Mock
    private View mRootView;
    @Mock
    private Display mDisplay;
    @Mock
    private PartialCustomTabHeightStrategy.OnResizedCallback mOnResizedCallback;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private LinearLayout mNavbar;
    @Mock
    private ViewPropertyAnimator mViewAnimator;
    @Mock
    private ImageView mSpinnerView;
    @Mock
    private CircularProgressDrawable mSpinner;
    @Mock
    private View mToolbarView;
    @Mock
    private View mToolbarCoordinator;
    @Mock
    private ViewGroup mContentFrame;
    @Mock
    private ViewGroup mCoordinatorLayout;
    @Mock
    private View mDragBar;
    @Mock
    private View mDragHandlebar;
    @Mock
    private CommandLine mCommandLine;
    @Mock
    private FullscreenManager mFullscreenManager;

    private Context mContext;
    private List<WindowManager.LayoutParams> mAttributeResults;
    private DisplayMetrics mRealMetrics;
    private DisplayMetrics mMetrics;
    private Callback<Integer> mBottomInsetCallback = inset -> {};
    private FrameLayout.LayoutParams mLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    private FrameLayout.LayoutParams mCoordinatorLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getWindowManager()).thenReturn(mWindowManager);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view_stub)).thenReturn(mHandleViewStub);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view)).thenReturn(mHandleView);
        when(mActivity.findViewById(R.id.drag_bar)).thenReturn(mDragBar);
        when(mActivity.findViewById(R.id.drag_handlebar)).thenReturn(mDragHandlebar);
        when(mActivity.findViewById(R.id.coordinator)).thenReturn(mCoordinatorLayout);
        when(mActivity.findViewById(android.R.id.content)).thenReturn(mContentFrame);
        when(mHandleView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mToolbarCoordinator.getLayoutParams()).thenReturn(mLayoutParams);
        mAttributes = new WindowManager.LayoutParams();
        when(mWindow.getAttributes()).thenReturn(mAttributes);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getContext()).thenReturn(mActivity);
        when(mDecorView.getRootView()).thenReturn(mRootView);
        when(mRootView.getLayoutParams()).thenReturn(mAttributes);
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mNavbar.getLayoutParams()).thenReturn(mLayoutParams);
        when(mNavbar.animate()).thenReturn(mViewAnimator);
        when(mViewAnimator.alpha(anyFloat())).thenReturn(mViewAnimator);
        when(mViewAnimator.setDuration(anyLong())).thenReturn(mViewAnimator);
        when(mViewAnimator.setListener(anyObject())).thenReturn(mViewAnimator);
        when(mSpinnerView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mSpinnerView.getParent()).thenReturn(mContentFrame);
        when(mSpinnerView.animate()).thenReturn(mViewAnimator);
        when(mContentFrame.getLayoutParams()).thenReturn(mLayoutParams);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mCoordinatorLayout.getLayoutParams()).thenReturn(mCoordinatorLayoutParams);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;

        mAttributeResults = new ArrayList<>();
        doAnswer(invocation -> {
            WindowManager.LayoutParams attributes = new WindowManager.LayoutParams();
            attributes.copyFrom((WindowManager.LayoutParams) invocation.getArgument(0));
            mAttributeResults.add(attributes);
            return null;
        })
                .when(mWindow)
                .setAttributes(any(WindowManager.LayoutParams.class));

        mRealMetrics = new DisplayMetrics();
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mRealMetrics);
            return null;
        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));

        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        CommandLine.setInstanceForTesting(mCommandLine);
    }

    @After
    public void tearDown() {
        // Reset the multi-window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
    }

    private PartialCustomTabHeightStrategy createPcctAtHeight(int heightPx) {
        return createPcctAtHeight(heightPx, false);
    }

    private PartialCustomTabHeightStrategy createPcctAtHeight(int heightPx, boolean isFixedHeight) {
        PartialCustomTabHeightStrategy pcct =
                new PartialCustomTabHeightStrategy(mActivity, heightPx, null, null, isFixedHeight,
                        mOnResizedCallback, mActivityLifecycleDispatcher, mFullscreenManager);
        pcct.setWindowAboveNavbarForTesting(mWindowAboveNavbar);
        pcct.setMockViewForTesting(
                mNavbar, mSpinnerView, mSpinner, mToolbarView, mToolbarCoordinator);
        return pcct;
    }

    @Test
    public void create_heightIsCappedToHalfOfDeviceHeight() {
        createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));
    }

    @Test
    public void create_largeInitialHeight() {
        createPcctAtHeight(5000);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsFullHeight(mAttributeResults.get(0));
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsFullHeight(mAttributeResults.get(0));
    }

    private void doTestHeightWithStatusBar() {
        when(mContentFrame.getHeight())
                .thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT);
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        verifyWindowFlagsSet();
        assertEquals(1, mAttributeResults.size());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_R() {
        Assume.assumeTrue(
                "Fix for status bar height only applied in 'window-above-navbar' version.",
                mWindowAboveNavbar);
        configureStatusBarHeightForR();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_Q() {
        Assume.assumeTrue(
                "Fix for status bar height only applied in 'window-above-navbar' version.",
                mWindowAboveNavbar);
        configureStatusBarHeightForQ();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_landscape_R() {
        Assume.assumeTrue("Fix for status bar color only applied in 'window-above-navbar' version.",
                mWindowAboveNavbar);
        configureStatusBarHeightForR();
        configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, mAttributeResults.get(0).height);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_landscape_Q() {
        Assume.assumeTrue("Fix for status bar color only applied in 'window-above-navbar' version.",
                mWindowAboveNavbar);
        configureStatusBarHeightForQ();
        configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, mAttributeResults.get(0).height);
    }

    @Test
    public void create_landscapeOrientation() {
        configLandscapeMode();
        createPcctAtHeight(800);
        verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(1, mAttributeResults.size());
        assertEquals(0, mAttributeResults.get(0).y);
    }

    private static MotionEvent event(long ts, int action, int ypos) {
        return MotionEvent.obtain(ts, ts, action, DEVICE_WIDTH / 2, ypos, 0);
    }

    private static void actionDown(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_DOWN, ypos));
    }

    private static void actionMove(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_MOVE, ypos));
    }

    private static void actionUp(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_UP, ypos));
    }

    /**
     * Simulate dragging the tab and lifting the finger at the end.
     * @param handleStrategy {@link PartialCustomTabHandleStrategy} object.
     * @param ypos Series of y positions simulating the events.
     * @return Window attributes after the dragging finishes.
     */
    private WindowManager.LayoutParams dragTab(
            PartialCustomTabHandleStrategy handleStrategy, int... ypos) {
        int npos = ypos.length;
        assert npos >= 2;
        long timestamp = SystemClock.uptimeMillis();

        // ACTION_DOWN -> ACTION_MOVE * (npos-1) -> ACTION_UP
        actionDown(handleStrategy, timestamp, ypos[0]);
        for (int i = 1; i < npos; ++i) actionMove(handleStrategy, timestamp, ypos[i]);
        actionUp(handleStrategy, timestamp, ypos[npos - 1]);

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        int length = mAttributeResults.size();
        assertTrue(length > 1);
        return mAttributeResults.get(length - 1);
    }

    private void assertMotionEventIgnored(PartialCustomTabHandleStrategy handleStrategy) {
        assertFalse(handleStrategy.onInterceptTouchEvent(
                event(SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, 1500)));
    }

    private void assertTabIsAtInitialPos(WindowManager.LayoutParams attrs) {
        if (mWindowAboveNavbar) {
            if (attrs.gravity == Gravity.BOTTOM) {
                assertEquals(INITIAL_HEIGHT, attrs.height);
            } else if (attrs.gravity == Gravity.NO_GRAVITY) {
                // Tab is in motion. Check y since the it is in full height.
                assertEquals(INITIAL_HEIGHT + NAVBAR_HEIGHT, attrs.y);
            }
        } else {
            assertEquals(MAX_INIT_POS, attrs.y);
        }
    }

    private void assertTabIsFullHeight(WindowManager.LayoutParams attrs) {
        if (mWindowAboveNavbar) {
            assertEquals(FULL_HEIGHT, attrs.height);
        } else {
            assertEquals(0, attrs.y);
        }
    }

    private void assertTabBelowStatusBar(WindowManager.LayoutParams attrs) {
        if (mWindowAboveNavbar) {
            assertEquals(FULL_HEIGHT - STATUS_BAR_HEIGHT, attrs.height);
        } else {
            assertEquals(STATUS_BAR_HEIGHT, attrs.y);
        }
    }

    private void disableSpinnerAnimation() {
        // Disable animation for the mock spinner view.
        doAnswer(invocation -> {
            AnimatorListener listener = invocation.getArgument(0);
            listener.onAnimationEnd(null);
            return mViewAnimator;
        })
                .when(mViewAnimator)
                .setListener(any(AnimatorListener.class));
    }

    private WindowManager.LayoutParams getWindowAttributes() {
        return mAttributeResults.get(mAttributeResults.size() - 1);
    }

    private void configPortraitMode() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT);
        when(mDisplay.getRotation()).thenReturn(Surface.ROTATION_90);
    }

    private void configLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
    }

    private void configLandscapeMode(int direction) {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_WIDTH);
        when(mDisplay.getRotation()).thenReturn(direction);
    }

    private void configureStatusBarHeightForR() {
        // Setup for R+
        WindowMetrics windowMetric = Mockito.mock(WindowMetrics.class);
        WindowInsets windowInsets = Mockito.mock(WindowInsets.class);

        doReturn(windowMetric).when(mWindowManager).getCurrentWindowMetrics();
        doReturn(windowInsets).when(windowMetric).getWindowInsets();
        doReturn(new Rect(0, 0, mRealMetrics.widthPixels, mRealMetrics.heightPixels))
                .when(windowMetric)
                .getBounds();
        doReturn(Insets.of(0, STATUS_BAR_HEIGHT, 0, 0))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.statusBars()));
        doReturn(Insets.of(0, 0, 0, NAVBAR_HEIGHT))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.navigationBars()));
    }

    private void configureStatusBarHeightForQ() {
        // Setup for Q-
        int statusBarId = 54321;
        doReturn(statusBarId).when(mResources).getIdentifier(eq("status_bar_height"), any(), any());
        doReturn(STATUS_BAR_HEIGHT).when(mResources).getDimensionPixelSize(eq(statusBarId));
    }

    private void verifyWindowFlagsSet() {
        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
    }

    @Test
    public void moveFromTop() {
        // Drag to the top
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));

        // Drag down a little -> slide back to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 50, 100, 150));

        // Drag down enough -> slide to the initial position.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));
    }

    @Test
    public void moveFromInitialHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());

        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag up slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));

        // Drag down slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveUpThenDown() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Shake the tab from the initial position slightly -> back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1600));
    }

    @Test
    public void moveUp_landscapeOrientationUnresizable() {
        configLandscapeMode();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveUp_multiwindowModeUnresizable() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandescapeUnresizable() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandescapeHideCustomNavbar() {
        // Custom navigation bar is drawn only on 'Fixed Window'-type implementation.
        if (mWindowAboveNavbar) return;

        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);

        configLandscapeMode();
        strategy.onConfigurationChanged(mConfiguration);

        assertEquals(0, strategy.getNavbarHeightForTesting());
        verify(mNavbar).setVisibility(View.GONE);
    }

    @Test
    public void showDragHandleOnPortraitMode() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        verify(mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mDragBar);

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.GONE);
        clearInvocations(mDragBar);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mDragBar);

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.GONE);
    }

    @Test
    public void enterMultiwindowModeUnresizable() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveDownToDismiss() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void showSpinnerOnDragUpOnly() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        actionUp(handleStrategy, timestamp, 1450);

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();
        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        // Now the tab is full-height. Start dragging down.
        actionDown(handleStrategy, timestamp, 500);
        actionMove(handleStrategy, timestamp, 650);

        // Verify the spinner remained invisible.
        verify(mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenReachingFullHeight() {
        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        // Verify the spinner remains invisible after the tab reaches the top.
        int topY = strategy.getFullyExpandedYWithAdjustment();
        actionMove(handleStrategy, timestamp, topY);
        verify(mSpinnerView).setVisibility(View.GONE);
        clearInvocations(mSpinnerView);

        actionMove(handleStrategy, timestamp, topY + 200);
        verify(mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenDraggingDown() {
        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        // Drag below the initial height.
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT + 100);

        // Verify the spinner goes invisible.
        verify(mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void hideSpinnerEarly() {
        // Test hiding spinner early (500ms after showing) when there is no glitch at
        // the end of draggin action (only in window-above-navbar version).
        Assume.assumeTrue("Spinner can be hidden early only in 'window-above-navbar' version",
                mWindowAboveNavbar);

        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        long timeOut = PartialCustomTabHeightStrategy.SPINNER_TIMEOUT_MS;
        shadowOf(Looper.getMainLooper()).idleFor(timeOut, TimeUnit.MILLISECONDS);

        // Verify the spinner goes invisible after the specified timeout.
        verify(mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void expandToFullHeightOnShowingKeyboard() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        strategy.onShowSoftInput();
        shadowOf(Looper.getMainLooper()).idle();

        final int length = mAttributeResults.size();
        assertTrue(length > 1);

        // Verify that the tab expands to full height.
        assertTabIsFullHeight(mAttributeResults.get(length - 1));
    }

    @Test
    public void moveUpFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();

        // Try to drag up and verify that the location does not change.
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 250);
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveDownFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Try to drag down and check that it returns to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveDownToDismissFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void dragHandlebarInvisibleFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        verify(mDragHandlebar).setVisibility(View.GONE);
    }

    @Test
    public void invokeResizeCallbackExpansion() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        int expected = PartialCustomTabHeightStrategy.ResizeType.EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType", expected);

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        // invokeResizeCallback() should have been called and ResizeType.EXPANSION logged once.
        assertEquals(
                "ResizeType.EXPANSION should be recorded once.", 1, histogramExpansion.getDelta());
    }

    @Test
    public void invokeResizeCallbackMinimization() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top so it can be minimized in the next step.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        int expected = PartialCustomTabHeightStrategy.ResizeType.MINIMIZATION;
        HistogramDelta histogramMinimization =
                new HistogramDelta("CustomTabs.ResizeType", expected);

        // Drag down enough -> slide to the initial position.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));

        // invokeResizeCallback() should have been called and ResizeType.MINIMIZATION logged once.
        assertEquals("ResizeType.MINIMIZATION should be recorded once.", 1,
                histogramMinimization.getDelta());
    }

    @Test
    public void callbackWhenResized() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertTabIsAtInitialPos(mAttributeResults.get(0));
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Slide back to the initial height -> no resize happens.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));
        verify(mOnResizedCallback, never()).onResized(anyInt());

        // Drag to the top -> resized.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));
        verify(mOnResizedCallback).onResized(eq(FULL_HEIGHT));
        clearInvocations(mOnResizedCallback);

        // Slide back to the top -> no resize happens.
        assertTabIsFullHeight(dragTab(handleStrategy, 50, 100, 150));
        verify(mOnResizedCallback, never()).onResized(anyInt());

        // Drag to the initial height -> resized.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));
        verify(mOnResizedCallback).onResized(eq(INITIAL_HEIGHT));
    }

    @Test
    public void verifyNavigationBarHeightInMultiWindowMode() {
        mMetrics = new DisplayMetrics();
        mMetrics.widthPixels = DEVICE_WIDTH;
        mMetrics.heightPixels = MULTIWINDOW_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mMetrics);
            return null;
        })
                .when(mDisplay)
                .getMetrics(any(DisplayMetrics.class));
        when(mContentFrame.getHeight()).thenReturn(MULTIWINDOW_HEIGHT);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertEquals(0, strategy.getNavbarHeightForTesting());
    }

    @Test
    public void adjustWidthInLandscapeMode() {
        Assume.assumeTrue("Adjusting width is in effect only in 'window-above-navbar' version",
                mWindowAboveNavbar);
        configLandscapeMode(Surface.ROTATION_90);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        WindowManager.LayoutParams attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        configPortraitMode();
        strategy.onConfigurationChanged(mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        configLandscapeMode(Surface.ROTATION_270);
        strategy.onConfigurationChanged(mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);
    }

    @Test
    public void enterAndExitHtmlFullscreen() {
        Assume.assumeTrue("HTML fullscreen mode is supported in 'window-above-navbar' version.",
                mWindowAboveNavbar);

        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertFalse(isFullscreen());

        strategy.onEnterFullscreen(null, null);
        assertTrue(isFullscreen());

        strategy.onExitFullscreen(null);
        assertFalse(isFullscreen());
    }

    @Test
    public void dragToTheSameInitialY() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());

        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag tab slightly but actionDown and actionUp will be performed at the same Y.
        // The tab should remain open.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1500));
    }

    private boolean isFullscreen() {
        WindowManager.LayoutParams attrs = mAttributeResults.get(mAttributeResults.size() - 1);
        return attrs.isFullscreen();
    }
}
