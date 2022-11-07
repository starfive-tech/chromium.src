// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordManagerSideBarElement, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

suite('PasswordManagerSideBarTest', function() {
  let sidebar: PasswordManagerSideBarElement;

  setup(function() {
    document.body.innerHTML = '';
    sidebar = document.createElement('password-manager-side-bar');
    document.body.appendChild(sidebar);
    return flushTasks();
  });

  test('check layout', function() {
    assertTrue(isVisible(sidebar));
    const sideBarEntries = sidebar.shadowRoot!.querySelectorAll('a');
    assertEquals(3, sideBarEntries.length);
  });

  [Page.PASSWORDS, Page.CHECKUP, Page.SETTINGS].forEach(
      page => test(`clicking ${page} updates path`, function() {
        const differentPage =
            page === Page.PASSWORDS ? Page.CHECKUP : Page.PASSWORDS;
        Router.getInstance().navigateTo(differentPage);
        assertEquals(differentPage, Router.getInstance().currentRoute.page);

        const element =
            sidebar.shadowRoot!.querySelector<HTMLElement>(`#${page}`)!;
        element.click();
        assertEquals(page, Router.getInstance().currentRoute.page);
      }));

  [Page.PASSWORDS, Page.CHECKUP, Page.SETTINGS].forEach(
      page => test(`navigating to ${page} updates selected item`, function() {
        Router.getInstance().navigateTo(page);
        assertEquals(page, Router.getInstance().currentRoute.page);
        assertEquals(page, (sidebar.$.menu.selectedItem as HTMLElement).id);
      }));
});
