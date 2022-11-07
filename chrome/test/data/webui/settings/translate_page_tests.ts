// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrIconButtonElement, LanguageHelper, LanguagesBrowserProxyImpl, SettingsAddLanguagesDialogElement, SettingsTranslatePageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, fakeDataBind} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
// clang-format on

const translate_page_tests = {
  TestNames: {
    TranslateSettings: 'base translate settings',
    AlwaysTranslateDialog: 'always translate dialog',
    NeverTranslateDialog: 'never translate dialog',
  },
};

Object.assign(window, {translate_page_tests});

suite('translate page settings', function() {
  let languageHelper: LanguageHelper;
  let translatePage: SettingsTranslatePageElement;
  let browserProxy: TestLanguagesBrowserProxy;

  const translateTarget = 'translate_recent_target';
  // Always Translate language pref name for the platform.
  const alwaysTranslatePref = 'translate_allowlists';
  const neverTranslatePref = 'translate_blocked_languages';

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableDesktopDetailedLanguageSettings: true,
    });
    document.body.innerHTML = '';
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(
        settingsPrivate as unknown as typeof chrome.settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Set up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate =
          browserProxy.getLanguageSettingsPrivate() as unknown as
          FakeLanguageSettingsPrivate;
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      translatePage = document.createElement('settings-translate-page');

      translatePage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, translatePage, 'prefs');

      translatePage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, translatePage, 'language-helper');

      translatePage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, translatePage, 'languages');

      document.body.appendChild(translatePage);
      flush();

      languageHelper = translatePage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    document.body.innerHTML = '';
  });

  suite(translate_page_tests.TestNames.TranslateSettings, function() {
    test('change target language', function() {
      const targetLanguageSelector = translatePage.shadowRoot!.querySelector
          <HTMLSelectElement>('#targetLanguage');
      assertTrue(!!targetLanguageSelector);

      assertEquals(targetLanguageSelector.value,
          translatePage.getPref(translateTarget).value);

      targetLanguageSelector.value = 'sw';
      targetLanguageSelector.dispatchEvent(new CustomEvent('change'));

      assertEquals(translatePage.getPref(translateTarget).value, 'sw');
    });

    test('never translate remove icon enabled state', function() {
      // The icon should be disabled if there is only one element on the list
      // and enabled if there are more than one.
      const neverTranslateDiv =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#neverTranslateList');
      assertTrue(!!neverTranslateDiv);

      // Initially only one disabled icon
      let deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(1, deleteIcons.length);
      assertTrue(deleteIcons[0]!.disabled);

      // Add another language to never translate.
      languageHelper.disableTranslateLanguage('sw');
      flush();

      // All icons should be enabled now.
      deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(2, deleteIcons.length);
      for (const icon of deleteIcons) {
        assertFalse(icon.disabled);
      }

      // Remove language and icon should be disabled again.
      languageHelper.enableTranslateLanguage('sw');
      flush();

      // All icons should be enabled now.
      deleteIcons = neverTranslateDiv.querySelectorAll<CrIconButtonElement>(
          '.icon-delete-gray');
      assertEquals(1, deleteIcons.length);
      assertTrue(deleteIcons[0]!.disabled);
    });

    test('test translate.enable toggle', function() {
      const settingsToggle =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#offerTranslateOtherLanguages');
      assertTrue(!!settingsToggle);

      // Clicking on the toggle switches it to false.
      settingsToggle.click();
      let newToggleValue = translatePage.getPref('translate.enabled').value;
      assertFalse(newToggleValue);

      // Clicking on the toggle switches it to true again.
      settingsToggle.click();
      newToggleValue = translatePage.getPref('translate.enabled').value;
      assertTrue(newToggleValue);
    });
  });

  suite(translate_page_tests.TestNames.AlwaysTranslateDialog, function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogClosedResolver: PromiseResolver<void>;
    let dialogClosedObserver: MutationObserver;

    // Resolves the PromiseResolver if the mutation includes removal of the
    // settings-add-languages-dialog.
    // TODO(michaelpg): Extract into a common method similar to
    // whenAttributeIs for use elsewhere.
    function onMutation(
        mutations: MutationRecord[], observer: MutationObserver) {
      if (mutations.some(function(mutation) {
            return mutation.type === 'childList' &&
                Array.from(mutation.removedNodes).includes(dialog);
          })) {
        // Sanity check: the dialog should no longer be in the DOM.
        assertEquals(
            null,
            translatePage.shadowRoot!.querySelector(
                'settings-add-languages-dialog'));
        observer.disconnect();
        assertTrue(!!dialogClosedResolver);
        dialogClosedResolver.resolve();
      }
    }

    setup(function() {
      const addLanguagesButton =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#addAlwaysTranslate');
      const whenDialogOpen = eventToPromise('cr-dialog-open', translatePage);
      assertTrue(!!addLanguagesButton);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // iron-list asynchronously at microtask timing, so wait for a new
      // task.
      return whenDialogOpen.then(() => {
        dialog = translatePage.shadowRoot!.querySelector(
            'settings-add-languages-dialog')!;
        assertTrue(!!dialog);
        assertEquals(dialog.id, 'alwaysTranslateDialog');

        // Observe the removal of the dialog via MutationObserver since the
        // HTMLDialogElement 'close' event fires at an unpredictable time.
        dialogClosedResolver = new PromiseResolver();
        dialogClosedObserver = new MutationObserver(onMutation);
        dialogClosedObserver.observe(
            translatePage.shadowRoot!, {childList: true});

        flush();
      });
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('add languages and confirm', function() {
      dialog.dispatchEvent(
          new CustomEvent('languages-added', {detail: ['en', 'no']}));
      dialog.$.dialog.close();
      assertDeepEquals(
          ['en', 'no'], translatePage.getPref(alwaysTranslatePref).value);

      return dialogClosedResolver.promise;
    });
  });

  suite(translate_page_tests.TestNames.NeverTranslateDialog, function() {
    let dialog: SettingsAddLanguagesDialogElement;
    let dialogClosedResolver: PromiseResolver<void>;
    let dialogClosedObserver: MutationObserver;

    // Resolves the PromiseResolver if the mutation includes removal of the
    // settings-add-languages-dialog.
    // TODO(michaelpg): Extract into a common method similar to
    // whenAttributeIs for use elsewhere.
    function onMutation(
        mutations: MutationRecord[], observer: MutationObserver) {
      if (mutations.some(function(mutation) {
            return mutation.type === 'childList' &&
                Array.from(mutation.removedNodes).includes(dialog);
          })) {
        // Sanity check: the dialog should no longer be in the DOM.
        assertEquals(
            null,
            translatePage.shadowRoot!.querySelector(
                'settings-add-languages-dialog'));
        observer.disconnect();
        assertTrue(!!dialogClosedResolver);
        dialogClosedResolver.resolve();
      }
    }

    setup(function() {
      const addLanguagesButton =
          translatePage.shadowRoot!.querySelector<HTMLElement>(
              '#addNeverTranslate');
      const whenDialogOpen = eventToPromise('cr-dialog-open', translatePage);
      assertTrue(!!addLanguagesButton);
      addLanguagesButton.click();

      // The page stamps the dialog, registers listeners, and populates the
      // iron-list asynchronously at microtask timing, so wait for a new
      // task.
      return whenDialogOpen.then(() => {
        dialog = translatePage.shadowRoot!.querySelector(
            'settings-add-languages-dialog')!;
        assertTrue(!!dialog);
        assertEquals(dialog.id, 'neverTranslateDialog');

        // Observe the removal of the dialog via MutationObserver since the
        // HTMLDialogElement 'close' event fires at an unpredictable time.
        dialogClosedResolver = new PromiseResolver();
        dialogClosedObserver = new MutationObserver(onMutation);
        dialogClosedObserver.observe(
            translatePage.shadowRoot!, {childList: true});

        flush();
      });
    });

    teardown(function() {
      dialogClosedObserver.disconnect();
    });

    test('add languages and confirm', function() {
      dialog.dispatchEvent(
          new CustomEvent('languages-added', {detail: ['sw', 'no']}));
      dialog.$.dialog.close();
      assertDeepEquals(
          ['en-US', 'sw', 'no'],
          translatePage.getPref(neverTranslatePref).value);

      return dialogClosedResolver.promise;
    });
  });
});
