// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** The base processor implementation for the Carousel suggestions. */
public abstract class BaseCarouselSuggestionProcessor implements SuggestionProcessor {
    private final Context mContext;
    private final int mCarouselViewDecorationHeightPx;
    private boolean mEnableHorizontalFade;

    /**
     * @param context Current context.
     */
    public BaseCarouselSuggestionProcessor(Context context) {
        mContext = context;
        mCarouselViewDecorationHeightPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height);
    }

    /**
     * @return The height of the Carousel view for use when computing view minimum height.
     */
    @Override
    public final int getMinimumViewHeight() {
        return mCarouselViewDecorationHeightPx + getMinimumCarouselItemViewHeight();
    }

    /**
     * @return Minimum height of an element hosted by the carousel.
     */
    public abstract int getMinimumCarouselItemViewHeight();

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @CallSuper
    @Override
    public void onNativeInitialized() {
        mEnableHorizontalFade = ChromeFeatureList.isEnabled(
                ChromeFeatureList.OMNIBOX_MOST_VISITED_TILES_FADING_ON_TABLET);
    }

    @CallSuper
    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int matchIndex) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        model.set(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE,
                isTablet && mEnableHorizontalFade);
    }

    @Override
    public boolean allowBackgroundRounding() {
        return false;
    }
}
