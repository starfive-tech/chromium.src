// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */

import {$} from 'chrome://resources/js/util.m.js';
export {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import './strings.m.js';
import './lock_screen_reauth.js';

function initialize() {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in chrome://resources/js/util.m.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  window.$ = $;
}

initialize();
