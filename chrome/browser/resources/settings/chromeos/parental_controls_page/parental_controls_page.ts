// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings page for managing Parental Controls features.
 */

import 'chrome://resources/cr_elements/icons.html.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_browser_proxy.js';
import {getTemplate} from './parental_controls_page.html.js';

const SettingsParentalControlsPageElementBase = I18nMixin(PolymerElement) as {
  new (): PolymerElement & I18nMixinInterface,
};

export class SettingsParentalControlsPageElement extends
    SettingsParentalControlsPageElementBase {
  static get is() {
    return 'settings-parental-controls-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isChild_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isChild');
        },
      },

      online_: {
        type: Boolean,
        value() {
          return navigator.onLine;
        },
      },
    };
  }

  private online_: boolean;
  private browserProxy_: ParentalControlsBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = ParentalControlsBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();

    // Set up online/offline listeners.
    window.addEventListener('offline', this.onOffline_.bind(this));
    window.addEventListener('online', this.onOnline_.bind(this));
  }

  /**
   * Returns the setup parental controls CrButtonElement.
   */
  getSetupButton(): CrButtonElement {
    const setupButton =
        this.shadowRoot!.querySelector<CrButtonElement>('#setupButton');
    assert(setupButton);
    return setupButton;
  }

  /**
   * Updates the UI when the device goes offline.
   */
  private onOffline_() {
    this.online_ = false;
  }

  /**
   * Updates the UI when the device comes online.
   */
  private onOnline_() {
    this.online_ = true;
  }

  /**
   * @return Returns the string to display in the main
   * description area for non-child users.
   */
  private getSetupLabelText_(online: boolean): string {
    if (online) {
      return this.i18n('parentalControlsPageSetUpLabel');
    } else {
      return this.i18n('parentalControlsPageConnectToInternetLabel');
    }
  }

  private handleSetupButtonClick_(event: Event) {
    event.stopPropagation();
    this.browserProxy_.showAddSupervisionDialog();
  }

  private handleFamilyLinkButtonClick_(event: Event) {
    event.stopPropagation();
    this.browserProxy_.launchFamilyLinkSettings();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-parental-controls-page': SettingsParentalControlsPageElement;
  }
}

customElements.define(
    SettingsParentalControlsPageElement.is,
    SettingsParentalControlsPageElement);
