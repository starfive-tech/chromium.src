// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-icon-button' is a button which displays an icon with a
 * ripple. It can be interacted with like a normal button using click as well as
 * space and enter to effectively click the button and fire a 'click' event.
 *
 * There are two sources to icons, cr-icons and iron-iconset-svg. The cr-icon's
 * are defined as background images with a reference to a resource file
 * associated with a CSS class name. The iron-icon's are defined as inline SVG's
 * under a key that is stored in a global map that is accessible to the
 * iron-icon element.
 *
 * Example of using a cr-icon:
 * <link rel="import" href="chrome://resources/cr_elements/cr_icons_css.html">
 * <dom-module id="module">
 *   <template>
 *     <style includes="cr-icons"></style>
 *     <cr-icon-button class="icon-class-name"></cr-icon-button>
 *   </template>
 * </dom-module>
 *
 * In general when an icon is specified using a class, the expectation is the
 * class will set an image to the --cr-icon-image variable.
 *
 * Example of using an iron-icon:
 * <link rel="import" href="chrome://resources/cr_elements/icons.html">
 * <cr-icon-button iron-icon="cr:icon-key"></cr-icon-button>
 *
 * The color of the icon can be overridden using CSS variables. When using
 * iron-icon both the fill and stroke can be overridden the variables:
 * --cr-icon-button-fill-color
 * --cr-icon-button-stroke-color
 *
 * When not using iron-icon (ie. specifying --cr-icon-image), the icons support
 * one color and the 'stroke' variables are ignored.
 *
 * When using iron-icon's, more than one icon can be specified by setting
 * the |ironIcon| property to a comma-delimited list of keys.
 */

import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class CrIconButtonElement extends PolymerElement {
  static get is() {
    return 'cr-icon-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },

      /**
       * Use this property in order to configure the "tabindex" attribute.
       */
      customTabIndex: {
        type: Number,
        observer: 'applyTabIndex_',
      },

      ironIcon: {
        type: String,
        observer: 'onIronIconChanged_',
        reflectToAttribute: true,
      },

      /** @private */
      multipleIcons_: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  constructor() {
    super();
    /**
     * It is possible to activate a tab when the space key is pressed down. When
     * this element has focus, the keyup event for the space key should not
     * perform a 'click'. |spaceKeyDown_| tracks when a space pressed and
     * handled by this element. Space keyup will only result in a 'click' when
     * |spaceKeyDown_| is true. |spaceKeyDown_| is set to false when element
     * loses focus.
     * @private {boolean}
     */
    this.spaceKeyDown_ = false;
  }

  /** @override */
  ready() {
    super.ready();
    this.setAttribute('aria-disabled', 'false');
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }

    this.addEventListener('blur', this.onBlur_.bind(this));
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener(
        'keyup', e => this.onKeyUp_(/** @type {!KeyboardEvent} */ (e)));
  }

  /** @param {string} className */
  toggleClass(className) {
    if (this.classList.contains(className)) {
      this.classList.remove(className);
    } else {
      this.classList.add(className);
    }
  }

  /**
   * @param {boolean} newValue
   * @param {boolean} oldValue
   * @private
   */
  disabledChanged_(newValue, oldValue) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    this.applyTabIndex_();
  }

  /**
   * Updates the tabindex HTML attribute to the actual value.
   * @private
   */
  applyTabIndex_() {
    let value = this.customTabIndex;
    if (value === undefined) {
      value = this.disabled ? -1 : 0;
    }
    this.setAttribute('tabindex', value);
  }

  /** @private */
  onBlur_() {
    this.spaceKeyDown_ = false;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
  }

  /** @private */
  onIronIconChanged_() {
    this.shadowRoot.querySelectorAll('iron-icon').forEach(el => el.remove());
    if (!this.ironIcon) {
      return;
    }
    const icons = (this.ironIcon || '').split(',');
    this.multipleIcons_ = icons.length > 1;
    icons.forEach(icon => {
      const ironIcon = document.createElement('iron-icon');
      ironIcon.icon = icon;
      this.$.icon.appendChild(ironIcon);
      if (ironIcon.shadowRoot) {
        ironIcon.shadowRoot.querySelectorAll('svg', 'img')
            .forEach(child => child.setAttribute('role', 'none'));
      }
    });
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key === 'Enter') {
      this.click();
    } else if (e.key === ' ') {
      this.spaceKeyDown_ = true;
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyUp_(e) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (this.spaceKeyDown_ && e.key === ' ') {
      this.spaceKeyDown_ = false;
      this.click();
    }
  }
}

customElements.define(CrIconButtonElement.is, CrIconButtonElement);
