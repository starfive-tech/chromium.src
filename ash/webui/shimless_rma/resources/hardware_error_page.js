// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';
import {disableAllButtons, executeThenTransitionState} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'hardware-error-page' is displayed when an unexpected error blocks RMA from
 * continuing.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const HardwareErrorPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class HardwareErrorPage extends HardwareErrorPageBase {
  static get is() {
    return 'hardware-error-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @protected */
  onShutDownButtonClicked_() {
    this.shimlessRmaService_.shutDownAfterHardwareError();
    disableAllButtons(this, /* showBusyStateOverlay= */ true);
  }
}

customElements.define(HardwareErrorPage.is, HardwareErrorPage);
