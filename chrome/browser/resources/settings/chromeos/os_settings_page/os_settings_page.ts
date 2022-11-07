// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-page' is the settings page containing the actual OS settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './settings_idle_load.js';
import '../os_apps_page/os_apps_page.js';
import '../os_people_page/os_people_page.js';
import '../os_privacy_page/os_privacy_page.js';
import '../os_printing_page/os_printing_page.js';
import '../os_search_page/os_search_page.js';
import '../personalization_page/personalization_page.js';
import '../../settings_page/settings_section.js';
import '../../settings_page_styles.css.js';
import '../bluetooth_page/bluetooth_page.js';
import '../device_page/device_page.js';
import '../internet_page/internet_page.js';
import '../kerberos_page/kerberos_page.js';
import '../multidevice_page/multidevice_page.js';
import '../os_bluetooth_page/os_bluetooth_page.js';
import '../os_icons.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {beforeNextRender, microTask, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {MainPageBehavior, MainPageBehaviorInterface} from '../main_page_behavior.js';
import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from '../os_apps_page/android_apps_browser_proxy.js';
import {OSPageVisibility} from '../os_page_visibility.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getTemplate} from './os_settings_page.html.js';
import {SettingsIdleLoadElement} from './settings_idle_load.js';

const OsSettingsPageElementBase = mixinBehaviors(
                                      [MainPageBehavior, RouteObserverBehavior],
                                      WebUIListenerMixin(PolymerElement)) as {
  new (): PolymerElement & WebUIListenerMixinInterface &
      MainPageBehaviorInterface & RouteObserverBehaviorInterface,
};

class OsSettingsPageElement extends OsSettingsPageElementBase {
  static get is() {
    return 'os-settings-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      showAndroidApps: Boolean,

      showArcvmManageUsb: Boolean,

      showCrostini: Boolean,

      showPluginVm: Boolean,

      showReset: Boolean,

      showStartup: Boolean,

      showKerberosSection: Boolean,

      allowCrostini_: Boolean,

      havePlayStoreApp: Boolean,

      androidAppsInfo: Object,

      /**
       * Whether the user is in guest mode.
       */
      isGuestMode_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isGuest'),
      },

      /**
       * Whether Accessibility OS Settings visibility improvements are enabled.
       */
      isAccessibilityOSSettingsVisibilityEnabled_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityOSSettingsVisibilityEnabled');
        },
      },

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: {
        type: Object,
        value() {
          return {};
        },
      },

      advancedToggleExpanded: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'advancedToggleExpandedChanged_',
      },

      /**
       * True if a section is fully expanded to hide other sections beneath it.
       * False otherwise (even while animating a section open/closed).
       */
      hasExpandedSection_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user is a secondary user. Computed so that it is calculated
       * correctly after loadTimeData is available.
       */
      showSecondaryUserBanner_: {
        type: Boolean,
        computed: 'computeShowSecondaryUserBanner_(hasExpandedSection_)',
      },

      /**
       * Whether to show banner indicating the user to return this device as an
       * update is required as per policy but the device has reached end of
       * life.
       */
      showUpdateRequiredEolBanner_: {
        type: Boolean,
        value: !!loadTimeData.getString('updateRequiredEolBannerText'),
      },

      currentRoute_: Object,

      isBluetoothRevampEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableBluetoothRevamp');
        },
      },
    };
  }

  androidAppsInfo?: AndroidAppsInfo;
  pageVisibility: OSPageVisibility;
  advancedToggleExpanded: boolean;
  private allowCrostini_: boolean;
  private hasExpandedSection_: boolean;
  private showSecondaryUserBanner_: boolean;
  private showUpdateRequiredEolBanner_: boolean;
  private currentRoute_?: Route;
  private isBluetoothRevampEnabled_: boolean;
  /**
   * Used to avoid handling a new toggle while currently toggling.
   */
  private advancedTogglingInProgress_: boolean;

  constructor() {
    super();
    this.advancedTogglingInProgress_ = false;
  }

  override ready() {
    super.ready();

    this.setAttribute('role', 'main');
    this.addEventListener('subpage-expand', this.onSubpageExpanded_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.currentRoute_ = Router.getInstance().getCurrentRoute();

    this.allowCrostini_ = loadTimeData.valueExists('allowCrostini') &&
        loadTimeData.getBoolean('allowCrostini');

    this.addWebUIListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    AndroidAppsBrowserProxyImpl.getInstance().requestAndroidAppsInfo();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    if (routes.ADVANCED && routes.ADVANCED.contains(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section !== oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    MainPageBehavior.currentRouteChanged.call(this, newRoute, oldRoute);
  }

  override containsRoute(route: Route) {
    return !route || routes.BASIC.contains(route) ||
        routes.ADVANCED.contains(route);
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private computeShowSecondaryUserBanner_(): boolean {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  }

  private computeShowUpdateRequiredEolBanner_(): boolean {
    return !this.hasExpandedSection_ && this.showUpdateRequiredEolBanner_;
  }

  private androidAppsInfoUpdate_(info: AndroidAppsInfo) {
    this.androidAppsInfo = info;
  }

  /**
   * Hides the update required EOL banner. It is shown again when Settings is
   * re-opened.
   */
  private onCloseEolBannerClicked_() {
    this.showUpdateRequiredEolBanner_ = false;
  }

  /**
   * Hides everything but the newly expanded subpage.
   */
  private onSubpageExpanded_() {
    this.hasExpandedSection_ = true;
  }

  private getAdvancedPageTemplate_(): SettingsIdleLoadElement {
    const el = this.shadowRoot!.getElementById('advancedPageTemplate');
    assert(el);
    return el as SettingsIdleLoadElement;
  }

  /**
   * Render the advanced page now (don't wait for idle).
   */
  private advancedToggleExpandedChanged_() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // beforeNextRender() must be used instead.
    beforeNextRender(this, () => {
      this.getAdvancedPageTemplate_().get();
    });
  }

  private advancedToggleClicked_() {
    if (this.advancedTogglingInProgress_) {
      return;
    }

    this.advancedTogglingInProgress_ = true;
    const toggle = this.shadowRoot!.getElementById('toggleContainer');
    assert(toggle);

    if (!this.advancedToggleExpanded) {
      this.advancedToggleExpanded = true;
      microTask.run(() => {
        this.getAdvancedPageTemplate_().get().then(() => {
          const event = new CustomEvent('scroll-to-top', {
            bubbles: true,
            composed: true,
            detail: {
              top: toggle.offsetTop,
              callback: () => {
                this.advancedTogglingInProgress_ = false;
              },
            },
          });
          this.dispatchEvent(event);
        });
      });
    } else {
      const event = new CustomEvent('scroll-to-bottom', {
        bubbles: true,
        composed: true,
        detail: {
          bottom: toggle.offsetTop + toggle.offsetHeight + 24,
          callback: () => {
            this.advancedToggleExpanded = false;
            this.advancedTogglingInProgress_ = false;
          },
        },
      });
      this.dispatchEvent(event);
    }
  }

  /**
   * @return Whether to show the basic page, taking into account
   *     both routing and search state.
   */
  private showBasicPage_(currentRoute: Route, hasExpandedSection: boolean):
      boolean {
    return !hasExpandedSection || routes.BASIC.contains(currentRoute);
  }

  /**
   * @return Whether to show the advanced page, taking into account
   *     both routing and search state.
   */
  private showAdvancedPage_(
      currentRoute: Route, hasExpandedSection: boolean,
      advancedToggleExpanded: boolean): boolean {
    return hasExpandedSection ?
        (routes.ADVANCED && routes.ADVANCED.contains(currentRoute)) :
        advancedToggleExpanded;
  }

  private showAdvancedSettings_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Icon name.
   */
  private getArrowIcon_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-page': OsSettingsPageElement;
  }
}

customElements.define(OsSettingsPageElement.is, OsSettingsPageElement);
