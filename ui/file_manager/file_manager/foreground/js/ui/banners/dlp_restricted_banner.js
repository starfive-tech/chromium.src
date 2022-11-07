// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DialogType} from '../../../../common/js/dialog_type.js';
import {VolumeManagerCommon} from '../../../../common/js/volume_manager_types.js';
import {Banner} from '../../../../externs/banner.js';

import {StateBanner} from './state_banner.js';

/**
 * The custom element tag name.
 * @type {string}
 */
export const TAG_NAME = 'dlp-restricted-banner';

/** @const {!HTMLTemplateElement} */
const htmlTemplate = html`{__html_template__}`;

/**
 * A banner that shows that some of the files or folders in the current
 * directory are restricted by Data Leak Prevention (DLP).
 */
export class DlpRestrictedBanner extends StateBanner {
  /**
   * Returns the HTML template for this banner.
   * @returns {!Node}
   */
  getTemplate() {
    return htmlTemplate.content.cloneNode(true);
  }

  /**
   * This banner is triggered in SELECT_OPEN_FILE, SELECT_OPEN_MULTI_FILE and
   * SELECT_SAVEAS_FILE dialog types, in case that some of the files and
   * folders, respectfully, cannot be selected due to DLP restrictions. The
   * following list are volumes for which this can happen.
   * @returns {!Array<!Banner.AllowedVolume>}
   */
  allowedVolumes() {
    // TODO(aidazolic): confirm this list is correct.
    return [
      {root: VolumeManagerCommon.RootType.DOWNLOADS},
      {root: VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT},
      {root: VolumeManagerCommon.RootType.COMPUTER},
      {root: VolumeManagerCommon.RootType.ARCHIVE},
      {root: VolumeManagerCommon.RootType.RECENT},
      {root: VolumeManagerCommon.RootType.RECENT_AUDIO},
      {root: VolumeManagerCommon.RootType.RECENT_IMAGES},
      {root: VolumeManagerCommon.RootType.RECENT_VIDEOS},
      {root: VolumeManagerCommon.RootType.TRASH},
    ];
  }

  /**
   * Persist the banner at all times if the folder is shared.
   * @returns {number}
   */
  timeLimit() {
    return Banner.INIFINITE_TIME;
  }

  /**
   * When the custom filter shows this banner in the controller, it passes the
   * context to the banner. The type, which is either File Picker or File Saver,
   * determines the text used in the banner.
   * @param {!Object} context The type of the dialog.
   */
  onFilteredContext(context) {
    if (!context || context.type == null) {
      console.warn('Context not supplied or dialog type key missing.');
      return;
    }
    const text = this.shadowRoot.querySelector('span[slot="text"]');
    // TODO(crbug.com/1360874): update to translation strings when they're
    // final.
    switch (context.type) {
      case DialogType.SELECT_OPEN_FILE:
      case DialogType.SELECT_OPEN_MULTI_FILE:
        text.innerText = 'Administrator policy restricts access to some files';
        return;
      case DialogType.SELECT_SAVEAS_FILE:
        text.innerText =
            'Administrator policy restricts saving to some locations';
        return;
      default:
        console.warn(`The DLP banner should not be shown for ${context.type}.`);
        return;
    }
  }
}

customElements.define(TAG_NAME, DlpRestrictedBanner);
