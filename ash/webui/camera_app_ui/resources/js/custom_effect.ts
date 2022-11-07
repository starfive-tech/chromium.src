// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animation from './animation.js';
import {assertExists, assertNotReached} from './assert.js';
import * as dom from './dom.js';
import {I18nString} from './i18n_string.js';
import * as loadTimeData from './models/load_time_data.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {speakMessage} from './spoken_msg.js';
import * as state from './state.js';
import * as util from './util.js';

/**
 * Interval of emerge time between two consecutive ripples in milliseconds.
 */
const RIPPLE_INTERVAL_MS = 5000;

/**
 * Controller for showing ripple effect.
 */
class RippleEffect {
  /**
   * Initial width of ripple in px.
   */
  private readonly width: number;

  /**
   * Initial height of ripple in px.
   */
  private readonly height: number;

  private readonly cancelHandle: number;

  /**
   * @param anchor Element to show ripple effect on.
   */
  constructor(private readonly anchor: HTMLElement) {
    const style = this.anchor.computedStyleMap();

    this.width = util.getStyleValueInPx(style, '--ripple-start-width');
    this.height = util.getStyleValueInPx(style, '--ripple-start-height');
    this.cancelHandle = setInterval(() => {
      this.addRipple();
    }, RIPPLE_INTERVAL_MS);

    this.addRipple();
  }

  private async addRipple(): Promise<void> {
    const rect = this.anchor.getBoundingClientRect();
    if (rect.width === 0) {
      return;
    }
    const template = util.instantiateTemplate('#ripple-template');
    const ripple = dom.getFrom(template, '.ripple', HTMLDivElement);
    const style = ripple.attributeStyleMap;
    style.set('left', CSS.px(rect.left - (this.width - rect.width) / 2));
    style.set('top', CSS.px(rect.top - (this.height - rect.height) / 2));
    style.set('width', CSS.px(this.width));
    style.set('height', CSS.px(this.height));
    document.body.appendChild(template);
    await animation.play(ripple);
    document.body.removeChild(ripple);
  }

  /**
   * Stops ripple effect.
   */
  stop(): void {
    clearInterval(this.cancelHandle);
  }
}

/**
 * Interval for toast updaing position.
 */
const TOAST_POSITION_UPDATE_MS = 500;

enum PositionProperty {
  BOTTOM = 'bottom',
  CENTER = 'center',
  LEFT = 'left',
  MIDDLE = 'middle',
  RIGHT = 'right',
  TOP = 'top',
}

type PositionProperties = Array<{
  elProperty: PositionProperty,
  toastProperty: PositionProperty,
  offset: number,
}>;

type PositionInfos = Array<{
  target: HTMLElement,
  properties: PositionProperties,
}>;

export enum IndicatorType {
  DOC_SCAN_AVAILABLE = 'doc_scan_available',
  DOC_MODE_MULTI_PAGE_AVAILABLE = 'doc_mode_multi_scan_available',
  DOWNLOAD_DOCUMENT_SCANNER = 'download_document_scanner',
}

/**
 * Setup the required state observers to dismiss toasts when changing
 * modes/cameras.
 */
export function setup(): void {
  state.addObserver(state.State.CAMERA_SWITCHING, (val) => {
    if (val) {
      hide();
    }
  });
  state.addObserver(state.State.MODE_SWITCHING, (val) => {
    if (val) {
      hide();
    }
  });
}

function getIndicatorI18nStringId(indicatorType: IndicatorType): I18nString {
  switch (indicatorType) {
    case IndicatorType.DOWNLOAD_DOCUMENT_SCANNER:
      return I18nString.DOWNLOADING_DOCUMENT_SCANNING_FEATURE;
    case IndicatorType.DOC_SCAN_AVAILABLE:
      return I18nString.NEW_DOCUMENT_SCAN_TOAST;
    case IndicatorType.DOC_MODE_MULTI_PAGE_AVAILABLE:
      return I18nString.DOCUMENT_MODE_MULTI_PAGE_TOAST;
    default:
      assertNotReached();
  }
}

function getIndicatorIcon(indicatorType: IndicatorType): string|null {
  switch (indicatorType) {
    case IndicatorType.DOWNLOAD_DOCUMENT_SCANNER:
      return '/images/download_dlc_toast_icon.svg';
    case IndicatorType.DOC_SCAN_AVAILABLE:
    case IndicatorType.DOC_MODE_MULTI_PAGE_AVAILABLE:
      return '/images/new_feature_toast_icon.svg';
    default:
      return null;
  }
}

function getOffsetProperties(
    element: HTMLElement, prefix: string): PositionProperties {
  const properties = [];
  const style = element.computedStyleMap();

  function getPositionProperty(key: string) {
    const property = assertExists(style.get(key)).toString();
    return util.assertEnumVariant(PositionProperty, property);
  }

  for (const dir of ['x', 'y']) {
    const toastProperty = getPositionProperty(`--${prefix}-ref-${dir}`);
    const elProperty = getPositionProperty(`--${prefix}-element-ref-${dir}`);
    const offset = util.getStyleValueInPx(style, `--${prefix}-offset-${dir}`);
    properties.push({elProperty, toastProperty, offset});
  }
  return properties;
}

function updatePositions(anchor: HTMLElement, infos: PositionInfos): void {
  for (const {target, properties} of infos) {
    updatePosition(anchor, target, properties);
  }
}

function updatePosition(
    anchor: HTMLElement, targetElement: HTMLElement,
    properties: PositionProperties): void {
  const rect = anchor.getBoundingClientRect();
  const style = targetElement.attributeStyleMap;
  if (rect.width === 0) {
    style.set('display', 'none');
    return;
  }
  style.clear();
  for (const {elProperty, toastProperty, offset} of properties) {
    let value;
    if (elProperty === PositionProperty.CENTER) {
      value = rect.left + offset + rect.width / 2;
    } else if (elProperty === PositionProperty.MIDDLE) {
      value = rect.top + offset + rect.height / 2;
    } else {
      value = rect[elProperty] + offset;
    }

    if (toastProperty === PositionProperty.RIGHT) {
      value = window.innerWidth - value;
    } else if (toastProperty === PositionProperty.BOTTOM) {
      value = window.innerHeight - value;
    }
    style.set(toastProperty, CSS.px(value));
  }
}

/**
 * Controller for showing a toast.
 */
class Toast {
  protected cancelHandle: number;

  constructor(
      protected readonly anchor: HTMLElement,
      protected readonly template: DocumentFragment,
      protected readonly toast: HTMLDivElement,
      protected readonly message: string,
      protected readonly positionInfos: PositionInfos) {
    this.cancelHandle = setInterval(() => {
      updatePositions(anchor, positionInfos);
    }, TOAST_POSITION_UPDATE_MS);
    updatePositions(anchor, positionInfos);
  }

  show(): void {
    document.body.appendChild(this.template);
    speakMessage(this.message);
  }

  focus(): void {
    this.anchor.setAttribute('aria-owns', 'new-feature-toast');
    this.toast.focus();
  }

  hide(): void {
    this.anchor.removeAttribute('aria-owns');
    clearInterval(this.cancelHandle);
    for (const {target} of this.positionInfos) {
      target.remove();
    }
  }
}

class NewFeatureToast extends Toast {
  constructor(anchor: HTMLElement) {
    const template = util.instantiateTemplate('#new-feature-toast-template');
    const toast = dom.getFrom(template, '#new-feature-toast', HTMLDivElement);

    const i18nId = util.assertEnumVariant(
        I18nString, anchor.getAttribute('i18n-new-feature'));
    const textElement =
        dom.getFrom(template, '.custom-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textElement.textContent = text;
    const ariaLabel =
        loadTimeData.getI18nMessage(I18nString.NEW_CONTROL_NAVIGATION, text);
    toast.setAttribute('aria-label', ariaLabel);

    super(anchor, template, toast, text, [{
            target: toast,
            properties: getOffsetProperties(anchor, 'toast'),
          }]);
  }
}

class IndicatorToast extends Toast {
  constructor(anchor: HTMLElement, indicatorType: IndicatorType) {
    const template = util.instantiateTemplate('#indicator-toast-template');
    const toast = dom.getFrom(template, '#indicator-toast', HTMLDivElement);

    const i18nId = getIndicatorI18nStringId(indicatorType);
    const textElement =
        dom.getFrom(template, '.custom-toast-text', HTMLSpanElement);
    const text = loadTimeData.getI18nMessage(i18nId);
    textElement.textContent = text;
    toast.setAttribute('aria-label', text);

    const icon = getIndicatorIcon(indicatorType);
    const iconElement =
        dom.getFrom(template, '#indicator-icon', HTMLImageElement);
    if (icon === null) {
      iconElement.hidden = true;
    } else {
      iconElement.src = icon;
      iconElement.hidden = false;
    }

    const indicatorDot =
        dom.getFrom(template, '#indicator-dot', HTMLDivElement);
    super(anchor, template, toast, text, [
      {
        target: toast,
        properties: getOffsetProperties(anchor, 'toast'),
      },
      {
        target: indicatorDot,
        properties: getOffsetProperties(anchor, 'indicator-dot'),
      },
    ]);
  }
}

interface EffectPayload {
  ripple: RippleEffect|null;
  toast: Toast;
  timeout: number;
}

interface EffectHandle {
  hide: () => void;
  focusToast: () => void;
}

let globalEffectPayload: EffectPayload|null = null;

/**
 * Hides the specified effect or the effect being showing.
 */
export function hide(effectPayload?: EffectPayload): void {
  if (effectPayload !== undefined) {
    stopEffect(effectPayload);
    if (effectPayload === globalEffectPayload) {
      globalEffectPayload = null;
    }
  } else if (globalEffectPayload !== null) {
    stopEffect(globalEffectPayload);
    globalEffectPayload = null;
  }
}

function stopEffect(effectPayload: EffectPayload) {
  const {ripple, toast, timeout} = effectPayload;
  if (ripple !== null) {
    ripple.stop();
  }
  toast.hide();
  clearTimeout(timeout);
}

/**
 * Timeout for effects.
 */
const EFFECT_TIMEOUT_MS = 10000;

/**
 * Shows the new feature toast message and ripple around the `anchor` element.
 * The message to show is defined in HTML attribute and the relative position is
 * defined in CSS.
 *
 * @return Functions to hide the effect or focus the toast.
 */
export function showNewFeature(anchor: HTMLElement): EffectHandle {
  return show(new NewFeatureToast(anchor), new RippleEffect(anchor));
}

/**
 * Shows the indicator toast message and an indicator dot around the `anchor`
 * element. The message to show is given by `indicatorType` and the relative
 * position of the toast and dot are defined in CSS.
 *
 * @return Functions to hide the effect or focus the toast.
 */
export function showIndicator(
    anchor: HTMLElement, indicatorType: IndicatorType): EffectHandle {
  return show(new IndicatorToast(anchor, indicatorType));
}

/**
 * Shows the effects.
 *
 * @return Functions to hide the effect or focus the toast.
 */
function show(toast: Toast, ripple: RippleEffect|null = null): EffectHandle {
  hide();

  const timeout = setTimeout(hide, EFFECT_TIMEOUT_MS);
  globalEffectPayload = {ripple, toast, timeout};
  toast.show();
  const originalEffectPayload = globalEffectPayload;
  return {
    hide: () => hide(originalEffectPayload),
    focusToast: () => toast.focus(),
  };
}

/**
 * @return If effect is showing.
 */
export function isShowing(): boolean {
  return globalEffectPayload !== null;
}

/**
 * Focuses to toast.
 */
export function focus(): void {
  if (globalEffectPayload === null) {
    return;
  }
  globalEffectPayload.toast.focus();
}

/**
 * Shows feature visual effect for PTZ options entry.
 */
export function showPtzToast(): void {
  const ptzPanelEntry = dom.get('#open-ptz-panel', HTMLButtonElement);
  const {hide, focusToast} = showNewFeature(ptzPanelEntry);
  focusToast();
  ptzPanelEntry.addEventListener('click', hide, {once: true});
}

/**
 * Shows document scan feature is available indicator on the scan mode button.
 */
export function showDocScanAvailableIndicator(): void {
  const scanModeButton = dom.get('input[data-mode="scan"]', HTMLInputElement);
  showIndicator(scanModeButton, IndicatorType.DOC_SCAN_AVAILABLE);
}

/**
 * Shows loading indicator toast for document mode when it's supported but not
 * yet ready.
 */
export async function showDownloadingDocScanIndicator(): Promise<void> {
  const docModeButton = dom.get('#scan-document-option', HTMLDivElement);
  const {hide} =
      showIndicator(docModeButton, IndicatorType.DOWNLOAD_DOCUMENT_SCANNER);
  await ChromeHelper.getInstance().checkDocumentModeReadiness();
  hide();
}
