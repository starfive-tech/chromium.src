// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview consolidated consent screen implementation.
 */

/* #js_imports_placeholder */

// Enum that describes the current state of the consolidated consent screen
const ConsolidatedConsentScreenState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
  GOOGLE_EULA: 'google-eula',
  CROS_EULA: 'cros-eula',
  ARC: 'arc',
  PRIVACY: 'privacy',
};

/**
 * URL to use when online page is not available.
 * @type {string}
 */
const GOOGLE_EULA_TERMS_URL = 'chrome://terms';
const ARC_TERMS_URL = 'chrome://terms/arc/terms';
const PRIVACY_POLICY_URL = 'chrome://terms/arc/privacy_policy';

/**
 * Timeout to load online ToS.
 * @type {number}
 */
const CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS = 10000;

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
const ConsolidatedConsentUserAction = {
  ACCEPT_BUTTON: 0,
  BACK_DEMO_BUTTON: 1,
  GOOGLE_EULA_LINK: 2,
  CROS_EULA_LINK: 3,
  ARC_TOS_LINK: 4,
  PRIVACY_POLICY_LINK: 5,
  USAGE_OPTIN_LEARN_MORE: 6,
  BACKUP_OPTIN_LEARN_MORE: 7,
  LOCATION_OPTIN_LEARN_MORE: 8,
  FOOTER_LEARN_MORE: 9,
  ERROR_STEP_RETRY_BUTTON: 10,
  MAX: 11,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const ConsolidatedConsentScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class ConsolidatedConsent extends ConsolidatedConsentScreenElementBase {
  static get is() {
    return 'consolidated-consent-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      isArcEnabled_: {
        type: Boolean,
        value: true,
      },

      isDemo_: {
        type: Boolean,
        value: false,
      },

      isChildAccount_: {
        type: Boolean,
        value: false,
      },

      isTosHidden_: {
        type: Boolean,
        value: false,
      },

      usageManaged_: {
        type: Boolean,
        value: false,
      },

      usageOptinHidden_: {
        type: Boolean,
        value: false,
      },

      usageOptinHiddenLoading_: {
        type: Boolean,
        value: true,
      },

      backupManaged_: {
        type: Boolean,
        value: false,
      },

      locationManaged_: {
        type: Boolean,
        value: false,
      },

      usageChecked: {
        type: Boolean,
        value: true,
      },

      backupChecked: {
        type: Boolean,
        value: true,
      },

      locationChecked: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();

    this.isArcTosInitialized_ = false;

    // Text displayed in the Arc Terms of Service webview.
    this.arcTosContent_ = '';

    // Flag that ensures that OOBE configuration is applied only once.
    this.configuration_applied_ = false;

    /**
     * The hostname of the url where the terms of service will be fetched.
     * Overwritten by tests to load terms of service from local test server.
     */
    this.arcTosHostName_ = 'https://play.google.com';

    // Online URLs
    this.googleEulaUrl_ = '';
    this.crosEulaUrl_ = '';
    this.arcTosUrl_ = '';

    // Used for loading ARC ToS.
    this.countryCode_ = '';

    // When Google EULA and/or ARC ToS need to be shown to the user,
    // it must be loaded before the loaded step is shown.
    this.googleEulaLoading_ = false;
    this.crosEulaLoading_ = false;
    this.arcTosLoading_ = false;
    this.privacyPolicyLoading_ = false;
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['setUsageMode',
            'setBackupMode',
            'setLocationMode',
            'setUsageOptinHidden',
    ];
  }
  // clang-format on

  /** @override */
  defaultUIStep() {
    return ConsolidatedConsentScreenState.LOADING;
  }

  get UI_STEPS() {
    return ConsolidatedConsentScreenState;
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('ConsolidatedConsentScreen');
    this.updateLocalizedContent();

    if (loadTimeData.valueExists(
            'consolidatedConsentArcTosHostNameForTesting')) {
      this.setArcTosHostNameForTesting_(loadTimeData.getString(
          'consolidatedConsentArcTosHostNameForTesting'));
    }
  }

  onBeforeShow(data) {
    window.setTimeout(this.applyOobeConfiguration_);

    this.isArcEnabled_ = data['isArcEnabled'];
    this.isDemo_ = data['isDemo'];
    this.isChildAccount_ = data['isChildAccount'];
    this.isTosHidden_ = data['isTosHidden'];
    this.countryCode_ = data['countryCode'];

    if (this.isDemo_) {
      this.usageOptinHidden_ = false;
      this.usageOptinHiddenLoading_ = false;
    }

    // If the ToS section is hidden, apply the remove the top border of the
    // first opt-in.
    if (this.isTosHidden_) {
      this.$.usageStats.classList.add('first-optin-no-tos');
    }

    this.googleEulaUrl_ = data['googleEulaUrl'];
    this.crosEulaUrl_ = data['crosEulaUrl'];
    this.arcTosUrl_ = this.arcTosHostName_ + '/about/play-terms.html';

    this.maybeLoadWebviews_(this.isTosHidden_, this.isArcEnabled_);

    if (this.isArcOptInsHidden_(this.isArcEnabled_, this.isDemo_)) {
      this.$.loadedContent.classList.remove('landscape-vertical-centered');
      this.$.loadedContent.classList.add('landscape-header-aligned');
    } else {
      this.$.loadedContent.classList.remove('landscape-header-aligned');
      this.$.loadedContent.classList.add('landscape-vertical-centered');
    }

    // Call updateLocalizedContent() to ensure that the listeners of the click
    // events on the ToS links are added.
    this.updateLocalizedContent();
  }

  applyOobeConfiguration_() {
    if (this.configuration_applied_) {
      return;
    }

    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }

    if (configuration.eulaSendStatistics) {
      this.usageChecked = true;
    }

    if (configuration.eulaAutoAccept && configuration.arcTosAutoAccept) {
      this.onAcceptClick_();
    }
  }

  // If ARC is disabled, don't show ARC ToS.
  shouldShowArcTos_(isTosHidden, isArcEnabled) {
    return !isTosHidden && isArcEnabled;
  }

  initializeArcTos_(countryCode) {
    if (this.isArcTosInitialized_) {
      return;
    }

    this.isArcTosInitialized_ = true;
    const webview = this.$.consolidatedConsentArcTosWebview;
    webview.removeContentScripts(['preProcess']);

    var language = this.getCurrentLanguage_();
    countryCode = countryCode.toLowerCase();

    var scriptSetParameters = 'document.countryCode = \'' + countryCode + '\';';
    scriptSetParameters += 'document.language = \'' + language + '\';';
    scriptSetParameters += 'document.viewMode = \'large-view\';';

    webview.addContentScripts([{
      name: 'preProcess',
      matches: [this.getArcTosHostNameForMatchPattern_() + '/*'],
      js: {code: scriptSetParameters},
      run_at: 'document_start',
    }]);

    webview.addContentScripts([{
      name: 'postProcess',
      matches: [this.getArcTosHostNameForMatchPattern_() + '/*'],
      css: {files: ['playstore.css']},
      js: {files: ['playstore.js']},
      run_at: 'document_end',
    }]);

    this.$.arcTosOverlayWebview.addContentScripts([{
      name: 'postProcess',
      matches: ['https://support.google.com/*'],
      css: {files: ['overlay.css']},
      run_at: 'document_end',
    }]);

    webview.addEventListener('newwindow', (event) => {
      event.preventDefault();
      this.showArcTosOverlay(event.targetUrl);
    });
  }

  maybeLoadWebviews_(isTosHidden, isArcEnabled) {
    if (!isTosHidden) {
      this.googleEulaLoading_ = true;
      this.crosEulaLoading_ = true;
      this.loadEulaWebview_(
          this.$.consolidatedConsentGoogleEulaWebview, this.googleEulaUrl_,
          false /* clear_anchors */);
      this.loadEulaWebview_(
          this.$.consolidatedConsentCrosEulaWebview, this.crosEulaUrl_,
          true /* clear_anchors */);
    }

    if (this.shouldShowArcTos_(isTosHidden, isArcEnabled)) {
      this.arcTosLoading_ = true;
      this.privacyPolicyLoading_ = true;
      this.initializeArcTos_(this.countryCode_);
      this.loadArcTosWebview_(this.arcTosUrl_);
    }
  }

  loadEulaWebview_(webview, online_tos_url, clear_anchors) {
    const loadFailureCallback = () => {
      WebViewHelper.loadUrlContentToWebView(
          webview, GOOGLE_EULA_TERMS_URL, WebViewHelper.ContentType.HTML);
    };

    const tosLoader = new WebViewLoader(
        webview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, clear_anchors, true /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  }

  loadArcTosWebview_(online_tos_url) {
    const webview = this.$.consolidatedConsentArcTosWebview;

    var loadFailureCallback = () => {
      this.setUIStep(ConsolidatedConsentScreenState.ERROR);
    };

    var tosLoader = new WebViewLoader(
        webview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, this.isDemo_ /* clear_anchors */,
        false /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  }

  /**
   * Returns a match pattern compatible version of termsOfServiceHostName_ by
   * stripping the port number part of the hostname. During tests
   * termsOfServiceHostName_ will contain a port number part.
   * @return {string}
   * @private
   */
  getArcTosHostNameForMatchPattern_() {
    return this.arcTosHostName_.replace(/:[0-9]+/, '');
  }

  /**
   * Returns current language that can be updated in OOBE flow. If OOBE flow
   * does not exist then use navigator.language.
   * @private
   */
  getCurrentLanguage_() {
    const LANGUAGE_LIST_ID = 'languageList';
    if (loadTimeData.valueExists(LANGUAGE_LIST_ID)) {
      var languageList = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
          loadTimeData.getValue(LANGUAGE_LIST_ID));
      if (languageList) {
        var language = getSelectedValue(languageList);
        if (language) {
          return language;
        }
      }
    }
    return navigator.language;
  }

  loadPrivacyPolicyWebview_(online_tos_url) {
    const webview = this.$.consolidatedConsentPrivacyPolicyWebview;

    var loadFailureCallback = () => {
      if (this.isDemo_) {
        WebViewHelper.loadUrlContentToWebView(
            webview, PRIVACY_POLICY_URL, WebViewHelper.ContentType.PDF);
      }
    };

    var tosLoader = new WebViewLoader(
        webview, CONSOLIDATED_CONSENT_ONLINE_LOAD_TIMEOUT_IN_MS,
        loadFailureCallback, this.isDemo_ /* clear_anchors */,
        false /* inject_css */);
    tosLoader.setUrl(online_tos_url);
  }

  onGoogleEulaContentLoad_() {
    this.googleEulaLoading_ = false;
    this.maybeSetLoadedStep_();
  }

  onCrosEulaContentLoad_() {
    this.crosEulaLoading_ = false;
  }

  maybeSetLoadedStep_() {
    if (!this.googleEulaLoading_ && !this.arcTosLoading_ &&
        !this.usageOptinHiddenLoading_ &&
        this.uiStep == ConsolidatedConsentScreenState.LOADING) {
      this.setUIStep(ConsolidatedConsentScreenState.LOADED);
      this.$.acceptButton.focus();
    }
  }

  onArcTosContentLoad_() {
    const webview = this.$.consolidatedConsentArcTosWebview;
    webview.executeScript({code: 'getPrivacyPolicyLink();'}, (results) => {
      if (results && results.length == 1 && typeof results[0] == 'string') {
        this.loadPrivacyPolicyWebview_(results[0]);
      } else {
        var defaultLink = 'https://www.google.com/intl/' +
            this.getCurrentLanguage_() + '/policies/privacy/';
        this.loadPrivacyPolicyWebview_(defaultLink);
      }
    });

    // In demo mode, consents are not recorded, so no need to store the ToS
    // Content.
    if (!this.isDemo_) {
      // Process online ToS.
      var getToSContent = {code: 'getToSContent();'};
      webview.executeScript(getToSContent, (results) => {
        if (!results || results.length != 1 || typeof results[0] !== 'string') {
          return;
        }
        this.arcTosContent_ = results[0];
      });
    }

    this.arcTosLoading_ = false;
    this.maybeSetLoadedStep_();
  }

  onPrivacyPolicyContentLoad_() {
    this.privacyPolicyLoading_ = false;
  }

  updateLocalizedContent() {
    this.shadowRoot.querySelector('#privacyPolicyLink').onclick = () =>
        this.onPrivacyPolicyLinkClick_();
    this.shadowRoot.querySelector('#googleEulaLink').onclick = () =>
        this.onGoogleEulaLinkClick_();
    this.shadowRoot.querySelector('#googleEulaLinkArcDisabled').onclick = () =>
        this.onGoogleEulaLinkClick_();
    this.shadowRoot.querySelector('#crosEulaLink').onclick = () =>
        this.onCrosEulaLinkClick_();
    this.shadowRoot.querySelector('#crosEulaLinkArcDisabled').onclick = () =>
        this.onCrosEulaLinkClick_();
    this.shadowRoot.querySelector('#arcTosLink').onclick = () =>
        this.onArcTosLinkClick_();
  }

  getSubtitleArcEnabled_(locale) {
    const subtitle = document.createElement('div');
    subtitle.innerHTML =
        this.i18nAdvanced('consolidatedConsentSubheader', {attrs: ['id']});

    const privacyPolicyLink = subtitle.querySelector('#privacyPolicyLink');
    privacyPolicyLink.setAttribute('is', 'action-link');
    privacyPolicyLink.classList.add('oobe-local-link');
    return subtitle.innerHTML;
  }

  getTermsDescriptionArcEnabled_(locale) {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvanced(
        'consolidatedConsentTermsDescription', {attrs: ['id']});

    const googleEulaLink = description.querySelector('#googleEulaLink');
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLink');
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    const arcTosLink = description.querySelector('#arcTosLink');
    arcTosLink.setAttribute('is', 'action-link');
    arcTosLink.classList.add('oobe-local-link');

    return description.innerHTML;
  }

  getTermsDescriptionArcDisabled_(locale) {
    const description = document.createElement('div');
    description.innerHTML = this.i18nAdvanced(
        'consolidatedConsentTermsDescriptionArcDisabled', {attrs: ['id']});

    const googleEulaLink =
        description.querySelector('#googleEulaLinkArcDisabled');
    googleEulaLink.setAttribute('is', 'action-link');
    googleEulaLink.classList.add('oobe-local-link');

    const crosEulaLink = description.querySelector('#crosEulaLinkArcDisabled');
    crosEulaLink.setAttribute('is', 'action-link');
    crosEulaLink.classList.add('oobe-local-link');

    return description.innerHTML;
  }

  getTitle_(locale, isTosHidden, isChildAccount) {
    if (isTosHidden) {
      return this.i18n('consolidatedConsentHeaderManaged');
    }

    if (isChildAccount) {
      return this.i18n('consolidatedConsentHeaderChild');
    }

    return this.i18n('consolidatedConsentHeader');
  }

  getUsageText_(locale, isChildAccount, isArcEnabled, isDemo) {
    if (this.isArcOptInsHidden_(isArcEnabled, isDemo)) {
      return this.i18n('consolidatedConsentUsageOptInArcDisabled');
    }
    if (isChildAccount) {
      return this.i18n('consolidatedConsentUsageOptInChild');
    }
    return this.i18n('consolidatedConsentUsageOptIn');
  }

  getUsageLearnMoreText_(locale, isChildAccount, isArcEnabled, isDemo) {
    if (this.isArcOptInsHidden_(isArcEnabled, isDemo)) {
      if (isChildAccount) {
        return this.i18nAdvanced(
            'consolidatedConsentUsageOptInLearnMoreArcDisabledChild');
      }
      return this.i18nAdvanced(
          'consolidatedConsentUsageOptInLearnMoreArcDisabled');
    }
    if (isChildAccount) {
      return this.i18nAdvanced('consolidatedConsentUsageOptInLearnMoreChild');
    }
    return this.i18nAdvanced('consolidatedConsentUsageOptInLearnMore');
  }

  getBackupLearnMoreText_(locale, isChildAccount) {
    if (isChildAccount) {
      return this.i18nAdvanced('consolidatedConsentBackupOptInLearnMoreChild');
    }
    return this.i18nAdvanced('consolidatedConsentBackupOptInLearnMore');
  }

  getLocationLearnMoreText_(locale, isChildAccount) {
    if (isChildAccount) {
      return this.i18nAdvanced(
          'consolidatedConsentLocationOptInLearnMoreChild');
    }
    return this.i18nAdvanced('consolidatedConsentLocationOptInLearnMore');
  }

  isArcOptInsHidden_(isArcEnabled, isDemo) {
    return !isArcEnabled || isDemo;
  }

  /**
   * Sets current usage mode.
   * @param {boolean} enabled Defines the state of usage opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setUsageMode(enabled, managed) {
    this.usageChecked = enabled;
    this.usageManaged_ = managed;
  }

  /**
   * Sets the hidden property of the usage opt-in.
   * @param {boolean} hidden Defines the value used for the hidden propoerty of
   *     the usage opt-in.
   */
  setUsageOptinHidden(hidden) {
    this.usageOptinHidden_ = hidden;
    this.usageOptinHiddenLoading_ = false;
    this.maybeSetLoadedStep_();
  }

  /**
   * Sets current backup and restore mode.
   * @param {boolean} enabled Defines the state of backup opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setBackupMode(enabled, managed) {
    this.backupChecked = enabled;
    this.backupManaged_ = managed;
  }

  /**
   * Sets current usage of location service opt in mode.
   * @param {boolean} enabled Defines the state of location service opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setLocationMode(enabled, managed) {
    this.locationChecked = enabled;
    this.locationManaged_ = managed;
  }

  /**
   * Opens external URL in popup overlay.
   * @param {string} targetUrl to show in overlay webview.
   */
  showArcTosOverlay(targetUrl) {
    this.$.arcTosOverlayWebview.src = targetUrl;
    this.$.arcTosOverlay.showDialog();
  }

  onGoogleEulaLinkClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.GOOGLE_EULA_LINK);
    this.setUIStep(ConsolidatedConsentScreenState.GOOGLE_EULA);
  }

  onCrosEulaLinkClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.CROS_EULA_LINK);
    this.setUIStep(ConsolidatedConsentScreenState.CROS_EULA);
  }

  onArcTosLinkClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.ARC_TOS_LINK);
    this.setUIStep(ConsolidatedConsentScreenState.ARC);
  }

  onPrivacyPolicyLinkClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.PRIVACY_POLICY_LINK);
    this.setUIStep(ConsolidatedConsentScreenState.PRIVACY);
  }

  onTermsStepOkClick_() {
    this.setUIStep(ConsolidatedConsentScreenState.LOADED);
  }

  onUsageLearnMoreClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.USAGE_OPTIN_LEARN_MORE);
    this.$.usageLearnMorePopUp.showDialog();
  }

  onBackupLearnMoreClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.BACKUP_OPTIN_LEARN_MORE);
    this.$.backupLearnMorePopUp.showDialog();
  }

  onLocationLearnMoreClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.LOCATION_OPTIN_LEARN_MORE);
    this.$.locationLearnMorePopUp.showDialog();
  }

  onFooterLearnMoreClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.FOOTER_LEARN_MORE);
    this.$.footerLearnMorePopUp.showDialog();
  }

  onAcceptClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.ACCEPT_BUTTON);

    this.userActed([
      'tos-accept',
      this.usageChecked,
      this.backupChecked,
      this.locationChecked,
      this.arcTosContent_,
    ]);
  }

  /**
   * On-tap event handler for Retry button.
   *
   * @private
   */
  onRetryClick_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.ERROR_STEP_RETRY_BUTTON);
    this.setUIStep(ConsolidatedConsentScreenState.LOADING);
    this.$.retryButton.focus();
    this.maybeLoadWebviews_(this.isTosHidden_, this.isArcEnabled_);
  }

  /**
   * On-tap event handler for Back button.
   *
   * @private
   */
  onBack_() {
    this.RecordUMAHistogramForUserActions_(
        ConsolidatedConsentUserAction.BACK_DEMO_BUTTON);
    this.userActed('back');
  }

  RecordUMAHistogramForUserActions_(result) {
    chrome.send('metricsHandler:recordInHistogram', [
      'OOBE.ConsolidatedConsentScreen.UserActions',
      result,
      ConsolidatedConsentUserAction.MAX,
    ]);
  }

  /**
   * Sets Play Store hostname url used to fetch terms of service for testing.
   * @param {string} hostname hostname used to fetch terms of service.
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  setArcTosHostNameForTesting_(hostname) {
    this.arcTosHostName_ = hostname;

    // Enable loading content script 'playstore.js' when fetching ToS from
    // the test server.
    var termsView = this.$.consolidatedConsentArcTosWebview;
    termsView.removeContentScripts(['postProcess']);
    termsView.addContentScripts([{
      name: 'postProcess',
      matches: [this.getArcTosHostNameForMatchPattern_() + '/*'],
      css: {files: ['playstore.css']},
      js: {files: ['playstore.js']},
      run_at: 'document_end',
    }]);
  }
}

customElements.define(ConsolidatedConsent.is, ConsolidatedConsent);
