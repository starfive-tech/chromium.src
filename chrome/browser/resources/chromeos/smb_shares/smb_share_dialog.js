// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/smb_shares/add_smb_share_dialog.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'smb-share-dialog' is used to host a <add-smb-share-dialog> element to
 * add SMB file shares.
 */

Polymer({
  is: 'smb-share-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /** @private */
  onDialogClose_() {
    chrome.send('dialogClose');
  },
});
