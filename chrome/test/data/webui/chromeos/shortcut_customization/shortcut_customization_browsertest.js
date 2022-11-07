// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://shortcut-customization. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ShortcutCustomizationApp*`
 * To run a single test suite such as 'AcceleratorRowTest':
 * browser_tests --gtest_filter=ShortcutCustomizationAppAcceleratorRowTest.All
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ui/base/ui_base_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var ShortcutCustomizationAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://shortcut-customization/';
  }

  get featureList() {
    return {enabled: ['features::kShortcutCustomizationApp']};
  }
};

const tests = [
  ['AcceleratorEditViewTest', 'accelerator_edit_view_test.js'],
  ['AcceleratorLookupManagerTest', 'accelerator_lookup_manager_test.js'],
  ['AcceleratorViewTest', 'accelerator_view_test.js'],
  ['AcceleratorRowTest', 'accelerator_row_test.js'],
  ['AcceleratorEditDialogTest', 'accelerator_edit_dialog_test.js'],
  ['AcceleratorSubsectionTest', 'accelerator_subsection_test.js'],
  ['FakeShortcutProviderTest', 'fake_shortcut_provider_test.js'],
  ['ShortcutCustomizationApp', 'shortcut_customization_test.js'],
];

tests.forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `ShortcutCustomizationApp${testName}`;
  this[className] = class extends ShortcutCustomizationAppBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://shortcut-customization/test_loader.html` +
          `?module=chromeos/shortcut_customization/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}