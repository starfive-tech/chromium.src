// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, Route, RouteObserverMixin, Router, UrlParam} from 'chrome://password-manager/password_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

const TestElementBase = RouteObserverMixin(PolymerElement);
class TestElement extends TestElementBase {
  static get properties() {
    return {
      newRoute: Object,
      oldRoute: Object,
    };
  }

  newRoute: Route|undefined;
  oldRoute: Route|undefined;

  override currentRouteChanged(newRoute: Route, oldRoute: Route): void {
    this.newRoute = newRoute;
    this.oldRoute = oldRoute;
  }
}
customElements.define('test-element', TestElement);

suite('PasswordManagerAppTest', function() {
  let testElement: TestElement;

  setup(function() {
    document.body.innerHTML = '';
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
    return flushTasks();
  });

  test('navigating notifies observers', function() {
    Router.getInstance().navigateTo(Page.CHECKUP);
    assertEquals(Page.CHECKUP, Router.getInstance().currentRoute.page);
    assertEquals(Page.CHECKUP, testElement.newRoute!.page);
    assertEquals(Page.PASSWORDS, testElement.oldRoute!.page);
    assertFalse(testElement.newRoute === testElement.oldRoute);
  });

  test('update Params notifies Observers', function() {
    const newParams = new URLSearchParams();
    newParams.set(UrlParam.SEARCH_TERM, 'test');
    Router.getInstance().updateRouterParams(newParams);

    assertEquals(newParams, Router.getInstance().currentRoute.queryParameters);
    assertEquals(newParams, testElement.newRoute!.queryParameters);
    assertFalse(testElement.newRoute === testElement.oldRoute);
  });

  test('Invalid path corrected', function() {
    history.replaceState({}, '', 'invalid-page');

    // Create a new router to simulate opening on the invalid page.
    const router = new Router();
    assertEquals(location.pathname, '/passwords');
    assertEquals(Page.PASSWORDS, router.currentRoute.page);
  });
});
