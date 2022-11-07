// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_dialog.js';
import './shortcut_input.js';
import './shortcuts_page.js';
import '../css/shortcut_customization_fonts.css.js';
import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';

import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorEditDialogElement} from './accelerator_edit_dialog.js';
import {RequestUpdateAcceleratorEvent} from './accelerator_edit_view.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ShowEditDialogEvent} from './accelerator_row.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {getTemplate} from './shortcut_customization_app.html.js';
import {AcceleratorConfig, AcceleratorInfo, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfoList, ShortcutProviderInterface} from './shortcut_types.js';

export interface ShortcutCustomizationAppElement {
  $: {
    navigationPanel: NavigationViewPanelElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'edit-dialog-closed': CustomEvent<void>;
    'request-update-accelerator': RequestUpdateAcceleratorEvent;
    'show-edit-dialog': ShowEditDialogEvent;
  }
}

/**
 * @fileoverview
 * 'shortcut-customization-app' is the main landing page for the shortcut
 * customization app.
 */
export class ShortcutCustomizationAppElement extends PolymerElement {
  static get is() {
    return 'shortcut-customization-app';
  }

  static get properties() {
    return {
      dialogShortcutTitle_: {
        type: String,
        value: '',
      },

      dialogAccelerators_: {
        type: Array,
        value: () => {},
      },

      dialogAction_: {
        type: Number,
        value: 0,
      },

      dialogSource_: {
        type: Number,
        value: 0,
      },

      showEditDialog_: {
        type: Boolean,
        value: false,
      },

      showRestoreAllDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }


  protected showRestoreAllDialog_: boolean;
  protected dialogShortcutTitle_: string;
  protected dialogAccelerators_: AcceleratorInfo[];
  protected dialogAction_: number;
  protected dialogSource_: AcceleratorSource;
  protected showEditDialog_: boolean;
  private shortcutProvider_: ShortcutProviderInterface = getShortcutProvider();
  private acceleratorLookupManager_: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addNavigationSelectors_();
    this.fetchAccelerators_();
    this.addEventListener('show-edit-dialog', this.showDialog_);
    this.addEventListener('edit-dialog-closed', this.onDialogClosed_);
    this.addEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('show-edit-dialog', this.showDialog_);
    this.removeEventListener('edit-dialog-closed', this.onDialogClosed_);
    this.removeEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators_);
  }

  private fetchAccelerators_() {
    // Kickoff fetching accelerators by first fetching the accelerator configs.
    this.shortcutProvider_.getAllAcceleratorConfig().then(
        (result: AcceleratorConfig) =>
            this.onAcceleratorConfigFetched_(result));
  }

  private onAcceleratorConfigFetched_(config: AcceleratorConfig) {
    this.acceleratorLookupManager_.setAcceleratorLookup(config);
    // After fetching the config infos, fetch the layout infos next.
    this.shortcutProvider_.getLayoutInfo().then(
        (result: LayoutInfoList) => this.onLayoutInfosFetched_(result));
  }

  private onLayoutInfosFetched_(layoutInfos: LayoutInfoList) {
    this.acceleratorLookupManager_.setAcceleratorLayoutLookup(layoutInfos);
    // Notify pages to update their accelerators.
    this.$.navigationPanel.notifyEvent('updateAccelerators');
  }

  private addNavigationSelectors_() {
    const pages = [
      this.$.navigationPanel.createSelectorItem(
          'Chrome OS', 'shortcuts-page',
          'navigation-selector:laptop-chromebook', 'chromeos-page-id',
          {category: /**ChromeOS*/ 0}),
      this.$.navigationPanel.createSelectorItem(
          'Browser', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
          'browser-page-id', {category: /**Browser*/ 1}),
      this.$.navigationPanel.createSelectorItem(
          'Android', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
          'android-page-id', {category: /**Android*/ 2}),
      this.$.navigationPanel.createSelectorItem(
          'Accessibility', 'shortcuts-page',
          'navigation-selector:laptop-chromebook', 'a11y-page-id',
          {category: /**Accessbility*/ 3}),

    ];
    this.$.navigationPanel.addSelectors(pages);
  }

  private showDialog_(e: ShowEditDialogEvent) {
    this.dialogShortcutTitle_ = e.detail.description;
    this.dialogAccelerators_ = e.detail.accelerators;
    this.dialogAction_ = e.detail.action;
    this.dialogSource_ = e.detail.source;
    this.showEditDialog_ = true;
  }

  private onDialogClosed_() {
    this.showEditDialog_ = false;
    this.dialogShortcutTitle_ = '';
    this.dialogAccelerators_ = [];
  }

  private onRequestUpdateAccelerators_(e: RequestUpdateAcceleratorEvent) {
    this.$.navigationPanel.notifyEvent('updateSubsections');
    const updatedAccels =
        this.acceleratorLookupManager_
            .getAccelerators(e.detail.source, e.detail.action)
            ?.filter((accel) => {
              // Hide accelerators that are default and disabled.
              return !(
                  accel.type === AcceleratorType.DEFAULT &&
                  accel.state === AcceleratorState.DISABLED_BY_USER);
            });

    this.shadowRoot!.querySelector<AcceleratorEditDialogElement>('#editDialog')!
        .updateDialogAccelerators(updatedAccels as AcceleratorInfo[]);
  }

  protected onRestoreAllDefaultClicked_() {
    this.showRestoreAllDialog_ = true;
  }

  protected onCancelRestoreButtonClicked_() {
    this.closeRestoreAllDialog_();
  }

  protected onConfirmRestoreButtonClicked_() {
    // TODO(jimmyxgong): Implement this function.
  }

  protected closeRestoreAllDialog_() {
    this.showRestoreAllDialog_ = false;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shortcut-customization-app': ShortcutCustomizationAppElement;
  }
}

customElements.define(
    ShortcutCustomizationAppElement.is, ShortcutCustomizationAppElement);
