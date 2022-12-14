// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Base template with elements common to all Cellular Setup flow sub-pages. */
Polymer({
  is: 'base-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Main title for the page.
     *
     * @type {string}
     */
    title: String,

    /**
     * Message displayed under the main title.
     *
     * @type {string}
     */
    message: String,

    /**
     * Name for the cellular-setup iconset iron-icon displayed beside message.
     *
     * @type {string}
     */
    messageIcon: {
      type: String,
      value: '',
    },
  },

  /**
   * @returns {string}
   * @private
   */
  getTitle_() {
    return this.title;
  },

  /**
   * @returns {boolean}
   * @private
   */
  isTitleShown_() {
    return !!this.title;
  },

  /**
   * @returns {boolean}
   * @private
   */
  isMessageIconShown_() {
    return !!this.messageIcon;
  },
});
