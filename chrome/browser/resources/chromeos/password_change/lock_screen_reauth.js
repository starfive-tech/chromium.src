// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to let user init online re-auth flow on
 * the lock screen.
 */

import 'chrome://resources/js/cr.m.js';
import 'chrome://resources/js/cr/event_target.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './components/oobe_icons.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Authenticator, AuthMode, AuthParams, SUPPORTED_PARAMS} from './gaia_auth_host/authenticator.m.js';

const clearDataType = {
  appcache: true,
  cache: true,
  cookies: true,
};

Polymer({
  is: 'lock-reauth',
  behaviors: [I18nBehavior],

  _template: html`{__html_template__}`,

  properties: {
    // User non-canonicalized email for display
    email_: String,


    /**
     * Auth Domain property of the authenticator. Updated via events.
     */
    authDomain_: {
      type: String,
      value: '',
    },

    /**
     * Whether the ‘verify user’ screen is shown.
     */
    isVerifyUser_: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether the ‘verify user again’ screen is shown.
     */
    isErrorDisplayed_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether user is authenticating on SAML page.
     */
    isSamlPage_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether there is a failure to scrape the user's password.
     */
    isConfirmPassword_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether no password is scraped or multiple passwords are scraped.
     */
    isManualInput_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user's password has changed.
     */
    isPasswordChanged_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether to show Saml Notice Message.
     */
    showSamlNoticeMessage_: {
      type: Boolean,
      value: false,
    },

    passwordConfirmAttempt_: {
      type: Number,
      value: 0,
    },

    passwordChangeAttempt_: {
      type: Number,
      value: 0,
    },
  },

  /**
   * The UI component that hosts IdP pages.
   * @type {!Authenticator|undefined}
   */
  authenticator_: undefined,

  /**
   * Webview that view IdP page
   * @type {!webview|undefined}
   * @private
   */
  signinFrame_: undefined,

  /** @override */
  ready() {
    this.signinFrame_ = this.getSigninFrame_();
    this.authenticator_ = new Authenticator(this.signinFrame_);
    this.authenticator_.addEventListener('authDomainChange', (e) => {
      this.authDomain_ = e.newValue;
    });
    this.authenticator_.addEventListener(
        'authCompleted', (e) => void this.onAuthCompletedMessage_(e));
    this.authenticator_.addEventListener(
        'loadAbort', (e) => void this.onLoadAbortMessage_(e));
    chrome.send('initialize');
  },

  /** @private */
  resetState_() {
    this.isVerifyUser_ = false;
    this.isErrorDisplayed_ = false;
    this.isSamlPage_ = false;
    this.isConfirmPassword_ = false;
    this.isManualInput_ = false;
    this.isPasswordChanged_ = false;
    this.showSamlNoticeMessage_ = false;
    this.authDomain_ = '';
  },

  /**
   * Set the orientation which will be used in styling webui.
   * @param {!Object} is_horizontal whether the orientation is horizontal or
   *  vertical.
   */
  setOrientation(is_horizontal) {
    if (is_horizontal) {
      document.documentElement.setAttribute('orientation', 'horizontal');
    } else {
      document.documentElement.setAttribute('orientation', 'vertical');
    }
  },

  /**
   * Set the width which will be used in styling webui.
   * @param {!Object} width the width of the dialog.
   */
  setWidth(width) {
    document.documentElement.style.setProperty(
      '--lock-screen-reauth-dialog-width', width + 'px');
  },

  /**
   * Loads the authentication parameter into the iframe.
   * @param {!AuthParams} data authenticator parameters bag.
   */
  loadAuthenticator(data) {
    this.authenticator_.setWebviewPartition(data.webviewPartitionName);
    const params = {};
    SUPPORTED_PARAMS.forEach(name => {
      if (data.hasOwnProperty(name)) {
        params[name] = data[name];
      }
    });

    this.authenticatorParams_ = params;
    this.email_ = data.email;
    if (!data['doSamlRedirect']) {
      this.doGaiaRedirect_();
    }
    chrome.send('authenticatorLoaded');
  },


  /**
   * This function is used when the wrong user is verified correctly
   * It reset authenticator state and display error message.
   */
  resetAuthenticator() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      this.authenticator_.resetStates();
      this.isButtonsEnabled_ = true;
      this.isErrorDisplayed_ = true;
    });
  },

  /**
   * Reloads the page.
   */
  reloadAuthenticator() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      this.authenticator_.resetStates();
    });
  },

  /**
   * @return {!Element}
   * @private
   */
  getSigninFrame_() {
    // Note: Can't use |this.$|, since it returns cached references to elements
    // originally present in DOM, while the signin-frame is dynamically
    // recreated (see Authenticator.setWebviewPartition()).
    const signinFrame = this.shadowRoot.getElementById('signin-frame');
    assert(signinFrame);
    return signinFrame;
  },

  onAuthCompletedMessage_(e) {
    const credentials = e.detail;
    chrome.send('completeAuthentication', [
      credentials.gaiaId,
      credentials.email,
      credentials.password,
      credentials.scrapedSAMLPasswords,
      credentials.usingSAML,
      credentials.services,
      credentials.passwordAttributes,
    ]);
  },

  /**
   * Invoked when onLoadAbort message received.
   * @param {!CustomEvent<!Object>} e Event with the payload containing
   *     additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onLoadAbortMessage_(e) {
    this.onWebviewError_(e.detail);
  },

  /**
   * Handler for webview error handling.
   * @param {!Object} data Additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   * @private
   */
  onWebviewError_(data) {
    chrome.send('webviewLoadAborted', [data.error_code]);
  },

  /**
   * Invoked when the user has successfully authenticated via SAML,
   * the Chrome Credentials Passing API was not used and the authenticator needs
   * the user to confirm the scraped password.
   * @param {number} passwordCount The number of passwords that were scraped.
   */
  showSamlConfirmPassword(passwordCount) {
    this.resetState_();
    /** This statement override resetState_ calls.
     * Thus have to be AFTER resetState_. */
    this.isConfirmPassword_ = true;
    this.isManualInput_ = (passwordCount === 0);
    if (this.passwordConfirmAttempt_ > 0) {
      this.$.passwordInput.value = '';
      this.$.passwordInput.invalid = true;
    }
    this.passwordConfirmAttempt_++;
  },

  /**
   * Invoked when the user's password doesn't match his old password.
   * @private
   */
  passwordChanged() {
    this.resetState_();
    this.isPasswordChanged_ = true;
    this.passwordChangeAttempt_++;
    if (this.passwordChangeAttempt_ > 1) {
      this.$.oldPasswordInput.invalid = true;
    }
  },

  /** @private */
  onVerify_() {
    this.authenticator_.load(AuthMode.DEFAULT, this.authenticatorParams_);
    this.resetState_();
    /**
     * These statements override resetStates_ calls.
     * Thus have to be AFTER resetState_.
     */
    this.isSamlPage_ = true;
    this.showSamlNoticeMessage_ = true;
  },

  /** @private */
  onConfirm_() {
    if (!this.$.passwordInput.validate()) {
      return;
    }
    if (this.isManualInput_) {
      // When using manual password entry, both passwords must match.
      const confirmPasswordInput =
          this.shadowRoot.querySelector('#confirmPasswordInput');
      if (!confirmPasswordInput.validate()) {
        return;
      }

      if (confirmPasswordInput.value != this.$.passwordInput.value) {
        this.$.passwordInput.invalid = true;
        confirmPasswordInput.invalid = true;
        return;
      }
    }

    chrome.send('onPasswordTyped', [this.$.passwordInput.value]);
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },

  /** @private */
  onResetAndClose_() {
    this.signinFrame_.clearData({since: 0}, clearDataType, () => {
      onCloseTap_();
    });
  },

  /** @private */
  onNext_() {
    if (!this.$.oldPasswordInput.validate()) {
      this.$.oldPasswordInput.focusInput();
      return;
    }
    chrome.send('updateUserPassword', [this.$.oldPasswordInput.value]);
    this.$.oldPasswordInput.value = '';
  },

  /** @private */
  doGaiaRedirect_() {
    this.authenticator_.load(AuthMode.DEFAULT, this.authenticatorParams_);
    this.resetState_();
    /**
     * These statements override resetStates_ calls.
     * Thus have to be AFTER resetState_.
     */
    this.isSamlPage_ = true;
  },

  /** @private */
  passwordPlaceholder_(locale, isManualInput_) {
    return this.i18n(
        isManualInput_ ? 'manualPasswordInputLabel' : 'confirmPasswordLabel');
  },

  /** @private */
  passwordErrorText_(locale, isManualInput_) {
    return this.i18n(
        isManualInput_ ? 'manualPasswordMismatch' :
                         'passwordChangedIncorrectOldPassword');
  },

});
