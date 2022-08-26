// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import './channel_switcher_dialog.js';
import './consumer_auto_update_toggle_dialog.js';
import './edit_hostname_dialog.js';

import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, browserChannelToI18nId, ChannelInfo, VersionInfo} from './about_page_browser_proxy.js';
import {getTemplate} from './detailed_build_info.html.js';
import {DeviceNameBrowserProxy, DeviceNameBrowserProxyImpl, DeviceNameMetadata} from './device_name_browser_proxy.js';
import {DeviceNameState} from './device_name_util.js';

declare global {
  interface HTMLElementEventMap {
    'set-consumer-auto-update': CustomEvent<{item: boolean}>;
  }
}

const SettingsDetailedBuildInfoBase =
    mixinBehaviors(
        [
          DeepLinkingBehavior,
          PrefsBehavior,
          RouteObserverBehavior,
        ],
        I18nMixin(WebUIListenerMixin(PolymerElement))) as {
      new (): PolymerElement & DeepLinkingBehaviorInterface &
          WebUIListenerMixinInterface & I18nMixinInterface &
          PrefsBehaviorInterface & RouteObserverBehaviorInterface,
    };

class SettingsDetailedBuildInfoElement extends SettingsDetailedBuildInfoBase {
  static get is() {
    return 'settings-detailed-build-info';
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

      versionInfo_: Object,

      channelInfo_: Object,

      deviceNameMetadata_: Object,

      currentlyOnChannelText_: String,

      showChannelSwitcherDialog_: Boolean,

      showEditHostnameDialog_: Boolean,

      canChangeChannel_: Boolean,

      isManagedAutoUpdateEnabled_: Boolean,

      showConsumerAutoUpdateToggleDialog_: Boolean,

      eolMessageWithMonthAndYear: {
        type: String,
        value: '',
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kChangeChromeChannel,
          Setting.kChangeDeviceName,
          Setting.kCopyDetailedBuildInfo,
        ]),
      },

      shouldHideEolInfo_: {
        type: Boolean,
        computed: 'computeShouldHideEolInfo_(eolMessageWithMonthAndYear)',
      },

      isHostnameSettingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isHostnameSettingEnabled');
        },
        readOnly: true,
      },

      /**
       * Whether the browser/ChromeOS is managed by their organization
       * through enterprise policies.
       */
      isManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
        readOnly: true,
      },

      /**
       * Whether or not the consumer auto update toggling is allowed.
       */
      isConsumerAutoUpdateTogglingAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isConsumerAutoUpdateTogglingAllowed');
        },
        readOnly: true,
      },

      /**
       * Whether or not to show the consumer auto update toggle.
       */
      showConsumerAutoUpdateToggle_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showConsumerAutoUpdateToggle') &&
              !loadTimeData.getBoolean('isManaged');
        },
        readOnly: true,
      },
    };
  }

  private versionInfo_: VersionInfo;
  private channelInfo_: ChannelInfo;
  private deviceNameMetadata_: DeviceNameMetadata;
  private currentlyOnChannelText_: string;
  private showChannelSwitcherDialog_: boolean;
  private showEditHostnameDialog_: boolean;
  private canChangeChannel_: boolean;
  private isManagedAutoUpdateEnabled_: boolean;
  private showConsumerAutoUpdateToggleDialog_: boolean;
  private eolMessageWithMonthAndYear: string;
  override supportedSettingIds: Set<Setting>;
  private shouldHideEolInfo_: boolean;
  private isHostnameSettingEnabled_: boolean;
  private isManaged_: boolean;
  private isConsumerAutoUpdateTogglingAllowed_: boolean;
  private showConsumerAutoUpdateToggle_: boolean;

  private aboutPageBrowserProxy_: AboutPageBrowserProxy;
  private deviceNameBrowserProxy_: DeviceNameBrowserProxy;

  constructor() {
    super();

    this.aboutPageBrowserProxy_ = AboutPageBrowserProxyImpl.getInstance();
    this.deviceNameBrowserProxy_ = DeviceNameBrowserProxyImpl.getInstance();
  }

  override ready() {
    super.ready();
    this.aboutPageBrowserProxy_.pageReady();

    this.addEventListener(
        'set-consumer-auto-update', (event: CustomEvent<{item: boolean}>) => {
          this.aboutPageBrowserProxy_.setConsumerAutoUpdate(event.detail.item);
        });

    if (this.isManaged_) {
      this.syncManagedAutoUpdateToggle_();
    } else {
      // This is to keep the Chrome pref in sync in case it becomes stale.
      // For example, if users toggle the consumer auto update, but the settings
      // page happened to crash/close before it got flushed out this would
      // assure a sync between the Chrome pref and the platform pref.
      this.syncConsumerAutoUpdateToggle_();
    }

    this.aboutPageBrowserProxy_.getVersionInfo().then(versionInfo => {
      this.versionInfo_ = versionInfo;
    });

    this.updateChannelInfo_();

    if (this.isHostnameSettingEnabled_) {
      this.addWebUIListener(
          'settings.updateDeviceNameMetadata',
          (data: DeviceNameMetadata) => this.updateDeviceNameMetadata_(data));
      this.deviceNameBrowserProxy_.notifyReadyForDeviceName();
    }
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.DETAILED_BUILD_INFO) {
      return;
    }

    this.attemptDeepLink();
  }

  private computeShouldHideEolInfo_(): boolean {
    return this.isManaged_ || !this.eolMessageWithMonthAndYear;
  }

  private updateChannelInfo_() {
    // canChangeChannel() call is expected to be low-latency, so fetch this
    // value by itself to ensure UI consistency (see https://crbug.com/848750).
    this.aboutPageBrowserProxy_.canChangeChannel().then(canChangeChannel => {
      this.canChangeChannel_ = canChangeChannel;
    });

    // getChannelInfo() may have considerable latency due to updates. Fetch this
    // metadata as part of a separate request.
    this.aboutPageBrowserProxy_.getChannelInfo().then(info => {
      this.channelInfo_ = info;
      // Display the target channel for the 'Currently on' message.
      this.currentlyOnChannelText_ = this.i18n(
          'aboutCurrentlyOnChannelInfo',
          this.i18n(browserChannelToI18nId(info.targetChannel, info.isLts)));
    });
  }

  private syncManagedAutoUpdateToggle_() {
    this.aboutPageBrowserProxy_.isManagedAutoUpdateEnabled().then(
        isManagedAutoUpdateEnabled => {
          this.isManagedAutoUpdateEnabled_ = isManagedAutoUpdateEnabled;
        });
  }

  private syncConsumerAutoUpdateToggle_() {
    this.aboutPageBrowserProxy_.isConsumerAutoUpdateEnabled().then(enabled => {
      this.aboutPageBrowserProxy_.setConsumerAutoUpdate(enabled);
    });
  }

  private updateDeviceNameMetadata_(data: DeviceNameMetadata) {
    this.deviceNameMetadata_ = data;
  }

  private getDeviceNameText_(): string {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.deviceNameMetadata_.deviceName;
  }

  private getDeviceNameEditButtonA11yDescription_(): string {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.i18n(
        'aboutDeviceNameEditBtnA11yDescription', this.getDeviceNameText_());
  }

  private canEditDeviceName_(): boolean {
    if (!this.deviceNameMetadata_) {
      return false;
    }

    return this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CAN_BE_MODIFIED;
  }

  private shouldShowPolicyIndicator_(): boolean {
    return this.getDeviceNameIndicatorType_() !== CrPolicyIndicatorType.NONE;
  }

  private shouldShowConsumerAutoUpdateToggle_(): boolean {
    return !this.isManaged_;
  }

  private getDeviceNameIndicatorType_(): string {
    if (!this.deviceNameMetadata_) {
      return CrPolicyIndicatorType.NONE;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER) {
      return CrPolicyIndicatorType.OWNER;
    }

    return CrPolicyIndicatorType.NONE;
  }

  private getChangeChannelIndicatorSourceName_(canChangeChannel: boolean):
      string {
    if (canChangeChannel) {
      // the indicator should be invisible.
      return '';
    }
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        '' :
        loadTimeData.getString('ownerEmail');
  }

  private getChangeChannelIndicatorType_(canChangeChannel: boolean):
      CrPolicyIndicatorType {
    if (canChangeChannel) {
      return CrPolicyIndicatorType.NONE;
    }
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        CrPolicyIndicatorType.DEVICE_POLICY :
        CrPolicyIndicatorType.OWNER;
  }

  private onChangeChannelTap_(e: Event) {
    e.preventDefault();
    this.showChannelSwitcherDialog_ = true;
  }

  private onEditHostnameTap_(e: Event) {
    e.preventDefault();
    this.showEditHostnameDialog_ = true;
  }

  private copyToClipBoardEnabled_(): boolean {
    return !!this.versionInfo_ && !!this.channelInfo_;
  }

  private onCopyBuildDetailsToClipBoardTap_() {
    const buildInfo: {[key: string]: string|boolean} = {
      'application_label': loadTimeData.getString('aboutBrowserVersion'),
      'platform': this.versionInfo_.osVersion,
      'aboutChannelLabel': this.channelInfo_.targetChannel,
      'firmware_version': this.versionInfo_.osFirmware,
      'aboutIsArcStatusTitle': loadTimeData.getBoolean('aboutIsArcEnabled'),
      'arc_label': this.versionInfo_.arcVersion,
      'isEnterpriseManagedTitle':
          loadTimeData.getBoolean('aboutEnterpriseManaged'),
      'aboutIsDeveloperModeTitle':
          loadTimeData.getBoolean('aboutIsDeveloperMode'),
    };

    const entries: string[] = [];
    for (const key in buildInfo) {
      entries.push(this.i18n(key) + ': ' + String(buildInfo[key]));
    }

    navigator.clipboard.writeText(entries.join('\n'));
  }

  private onConsumerAutoUpdateToggled_(_event: Event) {
    if (!this.isConsumerAutoUpdateTogglingAllowed_) {
      return;
    }
    this.showDialogOrFlushConsumerAutoUpdateToggle();
  }

  private onConsumerAutoUpdateToggledSettingsBox_() {
    if (!this.isConsumerAutoUpdateTogglingAllowed_) {
      return;
    }
    // Copy how cr-toggle negates the `checked` field.
    this.setPrefValue(
        'settings.consumer_auto_update_toggle',
        !this.getPref('settings.consumer_auto_update_toggle').value);
    this.showDialogOrFlushConsumerAutoUpdateToggle();
  }

  private showDialogOrFlushConsumerAutoUpdateToggle() {
    if (!this.getPref('settings.consumer_auto_update_toggle').value) {
      // Only show dialog when turning the toggle off.
      this.showConsumerAutoUpdateToggleDialog_ = true;
      return;
    }
    // Turning the toggle on requires no dialog.
    this.aboutPageBrowserProxy_.setConsumerAutoUpdate(true);
  }

  private onConsumerAutoUpdateToggleDialogClosed_() {
    this.showConsumerAutoUpdateToggleDialog_ = false;
  }

  private onVisitBuildDetailsPageTap_(e: Event) {
    e.preventDefault();
    window.open('chrome://version');
  }

  private onChannelSwitcherDialogClosed_() {
    this.showChannelSwitcherDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot!.querySelector('cr-button'))!);
    this.updateChannelInfo_();
  }

  private onEditHostnameDialogClosed_() {
    this.showEditHostnameDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot!.querySelector('cr-button'))!);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-detailed-build-info': SettingsDetailedBuildInfoElement;
  }
}

customElements.define(
    SettingsDetailedBuildInfoElement.is, SettingsDetailedBuildInfoElement);
