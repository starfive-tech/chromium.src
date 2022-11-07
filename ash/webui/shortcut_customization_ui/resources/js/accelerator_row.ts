// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_row.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorInfo, AcceleratorSource, ShortcutProviderInterface} from './shortcut_types.js';

export type ShowEditDialogEvent = CustomEvent<{
  description: string,
  accelerators: AcceleratorInfo[],
  action: number,
  source: AcceleratorSource,
}>;

declare global {
  interface HTMLElementEventMap {
    'show-edit-dialog': ShowEditDialogEvent;
  }
}

/**
 * @fileoverview
 * 'accelerator-row' is a wrapper component for one shortcut. It features a
 * description of the shortcut along with a list of accelerators.
 * TODO(jimmyxgong): Implement opening a dialog when clicked.
 */
export class AcceleratorRowElement extends PolymerElement {
  static get is() {
    return 'accelerator-row';
  }

  static get properties() {
    return {
      description: {
        type: String,
        value: '',
      },

      acceleratorInfos: {
        type: Array,
        value: () => {},
      },

      isLocked_: {
        type: Boolean,
        value: false,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
        observer: 'onSourceChanged_',
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  private isLocked_: boolean;
  private shortcutInterfaceProvider_: ShortcutProviderInterface =
      getShortcutProvider();

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (!this.isLocked_) {
      this.removeEventListener('click', () => this.showDialog_());
    }
  }

  protected onSourceChanged_() {
    this.shortcutInterfaceProvider_.isMutable(this.source)
        .then((result: boolean) => {
          this.isLocked_ = !result;
          if (!this.isLocked_) {
            this.addEventListener('click', () => this.showDialog_());
          }
        });
  }

  private showDialog_() {
    this.dispatchEvent(new CustomEvent(
        'show-edit-dialog',
        {
          bubbles: true,
          composed: true,
          detail: {
            description: this.description,
            accelerators: this.acceleratorInfos,
            action: this.action,
            source: this.source,
          },
        },
        ));
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-row': AcceleratorRowElement;
  }
}

customElements.define(AcceleratorRowElement.is, AcceleratorRowElement);