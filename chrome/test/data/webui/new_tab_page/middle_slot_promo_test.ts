// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {MiddleSlotPromoElement, PromoDismissAction} from 'chrome://new-tab-page/lazy_load.js';
import {$$, BrowserCommandProxy, CrAutoImgElement, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://webui-test/test_util.js';

import {fakeMetricsPrivate, MetricsTracker} from './metrics_test_support.js';
import {installMock} from './test_support.js';

suite('NewTabPageMiddleSlotPromoTest', () => {
  let newTabPageHandler: TestBrowserProxy;
  let promoBrowserCommandHandler: TestBrowserProxy;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = '';
    metrics = fakeMetricsPrivate();
    newTabPageHandler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    promoBrowserCommandHandler = installMock(
        CommandHandlerRemote,
        mock => BrowserCommandProxy.setInstance({handler: mock}));
  });

  async function createMiddleSlotPromo(canShowPromo: boolean):
      Promise<MiddleSlotPromoElement> {
    newTabPageHandler.setResultFor('getPromo', Promise.resolve({
      promo: {
        middleSlotParts: [
          {image: {imageUrl: {url: 'https://image'}}},
          {
            image: {
              imageUrl: {url: 'https://image'},
              target: {url: 'https://link'},
            },
          },
          {
            image: {
              imageUrl: {url: 'https://image'},
              target: {url: 'command:123'},
            },
          },
          {text: {text: 'text', color: 'red'}},
          {
            link: {
              url: {url: 'https://link'},
              text: 'link',
              color: 'green',
            },
          },
          {
            link: {
              url: {url: 'command:123'},
              text: 'command',
              color: 'blue',
            },
          },
        ],
        id: 19030295,
      },
    }));

    promoBrowserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: canShowPromo}));

    const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
    document.body.appendChild(middleSlotPromo);
    const loaded =
        eventToPromise('ntp-middle-slot-promo-loaded', document.body);
    await promoBrowserCommandHandler.whenCalled('canExecuteCommand');
    assertEquals(
        2, promoBrowserCommandHandler.getCallCount('canExecuteCommand'));
    if (canShowPromo) {
      await newTabPageHandler.whenCalled('onPromoRendered');
    } else {
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
    }
    await loaded;
    return middleSlotPromo;
  }

  function assertHasContent(
      hasContent: boolean, middleSlotPromo: MiddleSlotPromoElement) {
    assertEquals(hasContent, !!$$(middleSlotPromo, '#promoContainer'));
  }

  test(`render canShowPromo=true`, async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assert(promoContainer);
    const parts = promoContainer.children;
    assertEquals(6, parts.length);

    const image = parts[0] as CrAutoImgElement;
    const imageWithLink = parts[1] as HTMLAnchorElement;
    const imageWithCommand = parts[2] as HTMLAnchorElement;
    const text = parts[3] as HTMLElement;
    const link = parts[4] as HTMLAnchorElement;
    const command = parts[5] as HTMLAnchorElement;

    assertEquals('https://image', image.autoSrc);

    assertEquals('https://link/', imageWithLink.href);
    assertEquals(
        'https://image',
        (imageWithLink.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('', imageWithCommand.href);
    assertEquals(
        'https://image',
        (imageWithCommand.children[0] as CrAutoImgElement).autoSrc);

    assertEquals('text', text.innerText);
    assertEquals('red', text.style.color);

    assertEquals('https://link/', link.href);
    assertEquals('link', link.innerText);
    assertEquals('green', link.style.color);

    assertEquals('', command.href);
    assertEquals('command', command.text);
    assertEquals('blue', command.style.color);
  });

  test('render canShowPromo=false', async () => {
    const canShowPromo = false;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
  });

  test('clicking on command', async () => {
    const canShowPromo = true;
    const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
    assertHasContent(canShowPromo, middleSlotPromo);
    promoBrowserCommandHandler.setResultFor(
        'executeCommand', Promise.resolve());
    const promoContainer = $$(middleSlotPromo, '#promoContainer');
    assert(promoContainer);
    const imageWithCommand = promoContainer.children[2] as HTMLElement;
    const command = promoContainer.children[5] as HTMLElement;

    async function testClick(el: HTMLElement) {
      promoBrowserCommandHandler.reset();
      el.click();
      // Make sure the command and click information are sent to the browser.
      const [expectedCommand, expectedClickInfo] =
          await promoBrowserCommandHandler.whenCalled('executeCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(Command.kUnknownCommand, expectedCommand);
      assertDeepEquals(
          {
            middleButton: false,
            altKey: false,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          expectedClickInfo);
    }

    await testClick(imageWithCommand);
    await testClick(command);
  });

  [null,
   {middleSlotParts: []},
   {middleSlotParts: [{break: {}}]},
  ].forEach((promo, i) => {
    test(`promo remains hidden if there is no data ${i}`, async () => {
      newTabPageHandler.setResultFor('getPromo', Promise.resolve({promo}));
      const middleSlotPromo = document.createElement('ntp-middle-slot-promo');
      document.body.appendChild(middleSlotPromo);
      await flushTasks();
      assertEquals(
          0, promoBrowserCommandHandler.getCallCount('canExecuteCommand'));
      assertEquals(0, newTabPageHandler.getCallCount('onPromoRendered'));
      assertHasContent(false, middleSlotPromo);
    });
  });

  suite('middle slot promo dismissal', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        middleSlotPromoDismissalEnabled: true,
      });
    });

    test(`clicking dismiss button dismisses promo`, async () => {
      const canShowPromo = true;
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(3, parts.length);

      const dismissPromoButton = parts[1] as HTMLElement;
      dismissPromoButton.click();
      assertEquals(true, middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.DISMISS));
    });

    test(`clicking undo button restores promo`, async () => {
      const canShowPromo = true;
      const middleSlotPromo = await createMiddleSlotPromo(canShowPromo);
      assertHasContent(canShowPromo, middleSlotPromo);
      const parts = middleSlotPromo.$.promoAndDismissContainer.children;
      assertEquals(3, parts.length);

      const dismissPromoButton = parts[1] as HTMLElement;
      dismissPromoButton.click();
      assertEquals(true, middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.DISMISS));

      middleSlotPromo.$.undoDismissPromoButton.click();
      assertEquals(false, middleSlotPromo.$.promoAndDismissContainer.hidden);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Promos.DismissAction', PromoDismissAction.RESTORE));
    });
  });
});
