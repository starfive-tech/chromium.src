// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Parental Handoff screen.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.m.js';
import '../../components/common_styles/common_styles.m.js';
import '../../components/common_styles/oobe_dialog_host_styles.m.js';
import '../../components/dialogs/oobe_adaptive_dialog.m.js';

import {afterNextRender, dom, flush, html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.m.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.m.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.m.js';
import {OobeNextButton} from '../../components/buttons/oobe_next_button.m.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const ParentalHandoffElementBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @typedef {{
 *   parentalHandoffDialog:  OobeAdaptiveDialogElement,
 * }}
 */
ParentalHandoffElementBase.$;

class ParentalHandoff extends ParentalHandoffElementBase {
  static get is() {
    return 'parental-handoff-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The username to be displayed
       */
      username_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    this.username_ = '';
  }

  /** @override */
  get EXTERNAL_API() {
    return [];
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    if ('username' in data) {
      this.username_ = data.username;
    }
    this.$.parentalHandoffDialog.focus();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ParentalHandoffScreen');
  }

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNextButtonPressed_() {
    this.userActed('next');
  }
}

customElements.define(ParentalHandoff.is, ParentalHandoff);
