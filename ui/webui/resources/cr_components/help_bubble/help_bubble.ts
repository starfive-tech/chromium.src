// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A bubble for displaying in-product help. These are created
 * dynamically by HelpBubbleMixin, and their API should be considered an
 * implementation detail and subject to change (you should not add them to your
 * components directly).
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './help_bubble_icons.html.js';

import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert, assertNotReached} from '//resources/js/assert_ts.js';
import {isWindows} from '//resources/js/cr.m.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './help_bubble.html.js';
import {HelpBubbleArrowPosition, HelpBubbleButtonParams, Progress} from './help_bubble.mojom-webui.js';

const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

const ACTION_BUTTON_ID_PREFIX = 'action-button-';

export const HELP_BUBBLE_DISMISSED_EVENT = 'help-bubble-dismissed';
export const HELP_BUBBLE_TIMED_OUT_EVENT = 'help-bubble-timed-out';

export type HelpBubbleDismissedEvent = CustomEvent<{
  anchorId: string,
  fromActionButton: boolean,
  buttonIndex?: number,
}>;

export type HelpBubbleTimedOutEvent = CustomEvent<{
  anchorId: string,
}>;

export interface HelpBubbleElement {
  $: {
    arrow: HTMLElement,
    buttons: HTMLElement,
    close: CrIconButtonElement,
    bodyIcon: HTMLElement,
    main: HTMLElement,
    progress: HTMLElement,
    topContainer: HTMLElement,
  };
}

export class HelpBubbleElement extends PolymerElement {
  static get is() {
    return 'help-bubble';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      anchorId: {
        type: String,
        value: '',
        reflectToAttribute: true,
      },

      closeText: String,

      position: {
        type: HelpBubbleArrowPosition,
        value: HelpBubbleArrowPosition.TOP_CENTER,
        reflectToAttribute: true,
      },
    };
  }

  anchorId: string;
  bodyText: string;
  titleText: string;
  closeButtonAltText: string;
  position: HelpBubbleArrowPosition;
  buttons: HelpBubbleButtonParams[] = [];
  progress: Progress|null = null;
  bodyIconName: string|null;
  bodyIconAltText: string;
  forceCloseButton: boolean;
  timeoutMs: number|null = null;
  timeoutTimerId: number|null = null;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_: HTMLElement|null = null;

  /**
   * Backing data for the dom-repeat that generates progress indicators.
   * The elements are placeholders only.
   */
  private progressData_: void[] = [];

  /**
   * Shows the bubble.
   */
  show() {
    // Set up the progress track.
    if (this.progress) {
      this.progressData_ = new Array(this.progress.total);
    } else {
      this.progressData_ = [];
    }

    this.anchorElement_ =
        this.parentElement!.querySelector<HTMLElement>(`#${this.anchorId}`)!;
    assert(
        this.anchorElement_,
        'Tried to show a help bubble but couldn\'t find element with id ' +
            this.anchorId);

    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened bubble.
    this.style.display = 'block';
    this.removeAttribute('aria-hidden');
    this.updatePosition_();
    this.setAnchorHighlight_(true);

    if (this.timeoutMs !== null) {
      const timedOutCallback = () => {
        this.dispatchEvent(new CustomEvent(HELP_BUBBLE_TIMED_OUT_EVENT, {
          detail: {
            anchorId: this.anchorId,
          },
        }));
      };
      this.timeoutTimerId = setTimeout(timedOutCallback, this.timeoutMs);
    }
  }

  /**
   * Hides the bubble, clears out its contents, and ensures that screen readers
   * ignore it while hidden.
   *
   * TODO(dfried): We are moving towards formalizing help bubbles as single-use;
   * in which case most of this tear-down logic can be removed since the entire
   * bubble will go away on hide.
   */
  hide() {
    this.style.display = 'none';
    this.setAttribute('aria-hidden', 'true');
    this.setAnchorHighlight_(false);
    this.anchorElement_ = null;
    if (this.timeoutTimerId !== null) {
      clearInterval(this.timeoutTimerId);
      this.timeoutTimerId = null;
    }
  }

  /**
   * Retrieves the current anchor element, if set and the bubble is showing,
   * otherwise null.
   */
  getAnchorElement(): HTMLElement|null {
    return this.anchorElement_;
  }

  /**
   * Returns the button with the given `buttonIndex`, or null if not found.
   */
  getButtonForTesting(buttonIndex: number): CrButtonElement|null {
    return this.$.buttons.querySelector<CrButtonElement>(
        `[id="${ACTION_BUTTON_ID_PREFIX + buttonIndex}"]`);
  }

  /**
   * Returns whether the default button is leading (true on Windows) vs trailing
   * (all other platforms).
   */
  static isDefaultButtonLeading(): boolean {
    return isWindows;
  }

  private dismiss_() {
    assert(this.anchorId, 'Dismiss: expected help bubble to have an anchor.');
    this.dispatchEvent(new CustomEvent(HELP_BUBBLE_DISMISSED_EVENT, {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: false,
      },
    }));
  }

  private getProgressClass_(index: number): string {
    return index < this.progress!.current ? 'current-progress' :
                                            'total-progress';
  }

  private shouldShowTitleInTopContainer_(
      progress: Progress|null, titleText: string): boolean {
    return !!titleText && !progress;
  }

  private shouldShowBodyInTopContainer_(
      progress: Progress|null, titleText: string): boolean {
    return !progress && !titleText;
  }

  private shouldShowBodyInMain_(progress: Progress|null, titleText: string):
      boolean {
    return !!progress || !!titleText;
  }

  private shouldShowCloseButton_(
      buttons: HelpBubbleButtonParams[], forceCloseButton: boolean): boolean {
    return buttons.length === 0 || forceCloseButton;
  }

  private shouldShowBodyIcon_(bodyIconName: string): boolean {
    return bodyIconName !== null && bodyIconName !== '';
  }

  private onButtonClick_(e: DomRepeatEvent<HelpBubbleButtonParams>) {
    assert(
        this.anchorId,
        'Action button clicked: expected help bubble to have an anchor.');
    // There is no access to the model index here due to limitations of
    // dom-repeat. However, the index is stored in the node's identifier.
    const index: number = parseInt(
        (e.target as Element).id.substring(ACTION_BUTTON_ID_PREFIX.length));
    this.dispatchEvent(new CustomEvent(HELP_BUBBLE_DISMISSED_EVENT, {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: true,
        buttonIndex: index,
      },
    }));
  }

  private getButtonId_(index: number): string {
    return ACTION_BUTTON_ID_PREFIX + index;
  }

  private getButtonClass_(isDefault: boolean): string {
    return isDefault ? 'default-button' : '';
  }

  private getButtonTabIndex_(index: number, isDefault: boolean): number {
    return isDefault ? 1 : index + 2;
  }

  private buttonSortFunc_(
      button1: HelpBubbleButtonParams,
      button2: HelpBubbleButtonParams): number {
    // Default button is leading on Windows, trailing on other platforms.
    if (button1.isDefault) {
      return isWindows ? -1 : 1;
    }
    if (button2.isDefault) {
      return isWindows ? 1 : -1;
    }
    return 0;
  }

  /**
   * Determine classes that describe the arrow position relative to the
   * HelpBubble
   */
  private getArrowClass_(position: HelpBubbleArrowPosition): string {
    let classList = '';
    // `*-edge` classes move arrow to a HelpBubble edge
    switch (position) {
      case HelpBubbleArrowPosition.TOP_LEFT:
      case HelpBubbleArrowPosition.TOP_CENTER:
      case HelpBubbleArrowPosition.TOP_RIGHT:
        classList = 'top-edge ';
        break;
      case HelpBubbleArrowPosition.BOTTOM_LEFT:
      case HelpBubbleArrowPosition.BOTTOM_CENTER:
      case HelpBubbleArrowPosition.BOTTOM_RIGHT:
        classList = 'bottom-edge ';
        break;
      case HelpBubbleArrowPosition.LEFT_TOP:
      case HelpBubbleArrowPosition.LEFT_CENTER:
      case HelpBubbleArrowPosition.LEFT_BOTTOM:
        classList = 'left-edge ';
        break;
      case HelpBubbleArrowPosition.RIGHT_TOP:
      case HelpBubbleArrowPosition.RIGHT_CENTER:
      case HelpBubbleArrowPosition.RIGHT_BOTTOM:
        classList = 'right-edge ';
        break;
      default:
        assertNotReached('Unknown help bubble position: ' + position);
    }
    // `*-position` classes move arrow along the HelpBubble edge
    switch (position) {
      case HelpBubbleArrowPosition.TOP_LEFT:
      case HelpBubbleArrowPosition.BOTTOM_LEFT:
        classList += 'left-position';
        break;
      case HelpBubbleArrowPosition.TOP_CENTER:
      case HelpBubbleArrowPosition.BOTTOM_CENTER:
        classList += 'horizontal-center-position';
        break;
      case HelpBubbleArrowPosition.TOP_RIGHT:
      case HelpBubbleArrowPosition.BOTTOM_RIGHT:
        classList += 'right-position';
        break;
      case HelpBubbleArrowPosition.LEFT_TOP:
      case HelpBubbleArrowPosition.RIGHT_TOP:
        classList += 'top-position';
        break;
      case HelpBubbleArrowPosition.LEFT_CENTER:
      case HelpBubbleArrowPosition.RIGHT_CENTER:
        classList += 'vertical-center-position';
        break;
      case HelpBubbleArrowPosition.LEFT_BOTTOM:
      case HelpBubbleArrowPosition.RIGHT_BOTTOM:
        classList += 'bottom-position';
        break;
      default:
        assertNotReached('Unknown help bubble position: ' + position);
    }
    return classList;
  }

  /**
   * Sets the bubble position, as relative to that of the anchor element and
   * |this.position|.
   */
  private updatePosition_() {
    assert(
        this.anchorElement_, 'Update position: expected valid anchor element.');

    // How far HelpBubble is from anchorElement
    const ANCHOR_OFFSET = 16;
    const ARROW_WIDTH = 16;
    // The nearest an arrow can be to the adjacent HelpBubble edge
    const ARROW_OFFSET_FROM_EDGE = 22 + (ARROW_WIDTH / 2);

    // Inclusive of 8px visible arrow and 8px margin.
    const anchorRect = this.anchorElement_.getBoundingClientRect();
    const helpBubbleRect = this.getBoundingClientRect();

    let transform = '';
    // Move HelpBubble to correct side of the anchorElement
    switch (this.position) {
      case HelpBubbleArrowPosition.TOP_LEFT:
      case HelpBubbleArrowPosition.TOP_CENTER:
      case HelpBubbleArrowPosition.TOP_RIGHT:
        transform += `translateY(${
          anchorRect.height
        }px) translateY(${ANCHOR_OFFSET}px) `;
        break;
      case HelpBubbleArrowPosition.BOTTOM_LEFT:
      case HelpBubbleArrowPosition.BOTTOM_CENTER:
      case HelpBubbleArrowPosition.BOTTOM_RIGHT:
        transform += `translateY(-100%) translateY(-${ANCHOR_OFFSET}px) `;
        break;
      case HelpBubbleArrowPosition.LEFT_TOP:
      case HelpBubbleArrowPosition.LEFT_CENTER:
      case HelpBubbleArrowPosition.LEFT_BOTTOM:
        transform += `translateX(${
          anchorRect.width
        }px) translateX(${ANCHOR_OFFSET}px) `;
        break;
      case HelpBubbleArrowPosition.RIGHT_TOP:
      case HelpBubbleArrowPosition.RIGHT_CENTER:
      case HelpBubbleArrowPosition.RIGHT_BOTTOM:
        transform += `translateX(-100%) translateX(-${ANCHOR_OFFSET}px) `;
        break;
      default:
        assertNotReached();
    }
    // Move HelpBubble along the anchorElement edge according to arrow position
    switch (this.position) {
      case HelpBubbleArrowPosition.TOP_LEFT:
      case HelpBubbleArrowPosition.BOTTOM_LEFT:
        transform += `translateX(${
          (anchorRect.width / 2) - ARROW_OFFSET_FROM_EDGE
        }px)`;
        break;
      case HelpBubbleArrowPosition.TOP_CENTER:
      case HelpBubbleArrowPosition.BOTTOM_CENTER:
        transform += `translateX(${
          (anchorRect.width / 2) - (helpBubbleRect.width / 2)
        }px)`;
        break;
      case HelpBubbleArrowPosition.TOP_RIGHT:
      case HelpBubbleArrowPosition.BOTTOM_RIGHT:
        transform += `translateX(${
          (anchorRect.width / 2)
          - (helpBubbleRect.width - ARROW_OFFSET_FROM_EDGE)
        }px)`;
        break;
      case HelpBubbleArrowPosition.LEFT_TOP:
      case HelpBubbleArrowPosition.RIGHT_TOP:
        transform += `translateY(${
          (anchorRect.height / 2) - ARROW_OFFSET_FROM_EDGE
        }px)`;
        break;
      case HelpBubbleArrowPosition.LEFT_CENTER:
      case HelpBubbleArrowPosition.RIGHT_CENTER:
        transform += `translateY(${
          (anchorRect.height / 2) - (helpBubbleRect.height / 2)
        }px)`;
        break;
      case HelpBubbleArrowPosition.LEFT_BOTTOM:
      case HelpBubbleArrowPosition.RIGHT_BOTTOM:
        transform += `translateY(${
          (anchorRect.height / 2)
          - (helpBubbleRect.height - ARROW_OFFSET_FROM_EDGE)
        }px)`;
        break;
      default:
        assertNotReached();
    }
    this.style.transform = transform;
  }

  /**
   * Styles the anchor element to appear highlighted while the bubble is open,
   * or removes the highlight.
   */
  private setAnchorHighlight_(highlight: boolean) {
    assert(
        this.anchorElement_,
        'Set anchor highlight: expected valid anchor element.');
    this.anchorElement_.classList.toggle(ANCHOR_HIGHLIGHT_CLASS, highlight);
  }
}

customElements.define(HelpBubbleElement.is, HelpBubbleElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble': HelpBubbleElement;
  }
  interface HTMLElementEventMap {
    [HELP_BUBBLE_DISMISSED_EVENT]: HelpBubbleDismissedEvent;
    [HELP_BUBBLE_TIMED_OUT_EVENT]: HelpBubbleTimedOutEvent;
  }
}
