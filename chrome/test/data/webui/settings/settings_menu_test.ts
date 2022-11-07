// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {pageVisibility, Router, routes, SettingsMenuElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

// clang-format on

function getPerformanceMenuItem(settingsMenu: SettingsMenuElement) {
  return settingsMenu.shadowRoot!.querySelector<HTMLElement>('#performance');
}

suite('SettingsMenu', function() {
  let settingsMenu: SettingsMenuElement;

  setup(function() {
    document.body.innerHTML = '';
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(function() {
    settingsMenu.remove();
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', function() {
    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector =
        settingsMenu.shadowRoot!.querySelector('iron-selector')!;
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    Router.getInstance().navigateTo(routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.people.click();
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('performanceFeatureNotAvailableTest', function() {
    // performance menu item should not exist when high efficiency mode and
    // battery saver mode features are not available
    assertFalse(!!getPerformanceMenuItem(settingsMenu));
  });
});

suite('SettingsMenuPerformance', function() {
  let settingsMenu: SettingsMenuElement;

  setup(function() {
    loadTimeData.overrideValues({
      highEfficiencyModeAvailable: true,
      batterySaverModeAvailable: true,
    });
    document.body.innerHTML = '';
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('performanceFeatureAvailableTest', function() {
    // performance menu item should exist when high efficiency mode and battery
    // saver mode features are available
    assertTrue(!!getPerformanceMenuItem(settingsMenu));
  });

  test('performanceVisibilityTestDefault', function() {
    // performance menu item should not be hidden under default pageVisibility
    assertFalse(getPerformanceMenuItem(settingsMenu)!.hidden);
  });

  test('performanceVisibilityTestFalse', function() {
    // performance menu item should be hidden when pageVisibility is false
    settingsMenu.pageVisibility =
        Object.assign(settingsMenu.pageVisibility || {}, {performance: false});
    assertTrue(getPerformanceMenuItem(settingsMenu)!.hidden);
  });
});

suite('SettingsMenuReset', function() {
  let settingsMenu: SettingsMenuElement;

  setup(function() {
    document.body.innerHTML = '';
    Router.getInstance().navigateTo(routes.RESET, undefined);
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    flush();
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('openResetSection', function() {
    const selector = settingsMenu.$.menu;
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);
  });

  test('navigateToAnotherSection', function() {
    const selector = settingsMenu.$.menu;
    let path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);

    Router.getInstance().navigateTo(routes.PEOPLE, undefined);
    flush();

    path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/people', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.menu;
    const path = new window.URL(selector.selected.toString()).pathname;
    assertEquals('/reset', path);

    Router.getInstance().navigateTo(routes.BASIC, undefined);
    flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });

  test('pageVisibility', function() {
    function assertPagesHidden(expectedHidden: boolean) {
      const ids = [
        'accessibility', 'appearance',
        // <if expr="not is_chromeos">
        'defaultBrowser',
        // </if>
        'downloads', 'languages', 'onStartup', 'people', 'reset',
        // <if expr="not chromeos_ash">
        'system',
        // </if>
      ];

      for (const id of ids) {
        assertEquals(
            expectedHidden,
            settingsMenu.shadowRoot!.querySelector<HTMLElement>(
                                        `#${id}`)!.hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPagesHidden(false);

    // Set the visibility of the pages under test to "false".
    settingsMenu.pageVisibility = Object.assign(pageVisibility || {}, {
      a11y: false,
      advancedSettings: false,
      appearance: false,
      defaultBrowser: false,
      downloads: false,
      languages: false,
      multidevice: false,
      onStartup: false,
      people: false,
      reset: false,
      safetyCheck: false,
      system: false,
    });
    flush();

    // Now, the menu items should be hidden.
    assertPagesHidden(true);
  });
});
