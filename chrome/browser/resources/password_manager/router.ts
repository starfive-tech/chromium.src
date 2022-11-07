// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * The different pages that can be shown at a time.
 */
export enum Page {
  PASSWORDS = 'passwords',
  CHECKUP = 'checkup',
  SETTINGS = 'settings',
}

export enum UrlParam {
  SEARCH_TERM = 'q',
}

export class Route {
  page: Page;
  queryParameters: URLSearchParams;
}

/**
 * A helper object to manage in-page navigations. Since the Password Manager
 * page needs to support different urls for different subpages (like the checkup
 * page), we use this object to manage the history and url conversions.
 */
export class Router {
  static getInstance(): Router {
    return routerInstance || (routerInstance = new Router());
  }

  private currentRoute_:
      Route = {page: Page.PASSWORDS, queryParameters: new URLSearchParams()};
  private routeObservers_: Set<RouteObserverMixinInterface> = new Set();

  constructor() {
    this.processRoute_();

    window.addEventListener('popstate', () => {
      this.processRoute_();
    });
  }

  addObserver(observer: RouteObserverMixinInterface) {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  removeObserver(observer: RouteObserverMixinInterface) {
    assert(this.routeObservers_.delete(observer));
  }

  get currentRoute(): Route {
    return this.currentRoute_;
  }

  /**
   * Navigates to a page and pushes a new history entry.
   */
  navigateTo(page: Page) {
    if (page === this.currentRoute_.page) {
      return;
    }

    const oldRoute = this.currentRoute_;
    this.currentRoute_ = {
      page: page,
      queryParameters: new URLSearchParams(),
    };
    const path = '/' + page;
    const state = {url: path};
    history.pushState(state, '', path);
    this.notifyObservers_(oldRoute);
  }

  /**
   * Updates the URL parameters of the current route via replacing the
   * window history state. This changes location.search but doesn't
   * change the page itself, hence does not push a new route history entry.
   * Notifies routeObservers_.
   */
  updateRouterParams(params: URLSearchParams) {
    let url: string = this.currentRoute_.page;
    const queryString = params.toString();
    if (queryString) {
      url += '?' + queryString;
    }
    window.history.replaceState(window.history.state, '', url);

    const oldRoute = this.currentRoute_;
    this.currentRoute_ = {
      page: oldRoute.page,
      queryParameters: params,
    };
    this.notifyObservers_(oldRoute);
  }

  private notifyObservers_(oldRoute: Route) {
    assert(oldRoute !== this.currentRoute_);

    for (const observer of this.routeObservers_) {
      observer.currentRouteChanged(this.currentRoute_, oldRoute);
    }
  }

  /**
   * Helper function to set the current page and notify all observers.
   */
  private processRoute_() {
    const oldRoute = this.currentRoute_;
    this.currentRoute_ = {
      page: oldRoute.page,
      queryParameters: new URLSearchParams(location.search),
    };
    const section = location.pathname.substring(1).split('/')[0] || '';

    switch (section) {
      case Page.PASSWORDS:
        this.currentRoute_.page = Page.PASSWORDS;
        break;
      case Page.CHECKUP:
        this.currentRoute_.page = Page.CHECKUP;
        break;
      case Page.SETTINGS:
        this.currentRoute_.page = Page.SETTINGS;
        break;
      default:
        history.replaceState({}, '', this.currentRoute_.page);
    }
    this.notifyObservers_(oldRoute);
  }
}

let routerInstance: Router|null = null;

type Constructor<T> = new (...args: any[]) => T;

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass {
        override connectedCallback() {
          super.connectedCallback();

          Router.getInstance().addObserver(this);

          this.currentRouteChanged(
              Router.getInstance().currentRoute,
              Router.getInstance().currentRoute);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          Router.getInstance().removeObserver(this);
        }

        currentRouteChanged(_newRoute: Route, _oldRoute: Route): void {
          assertNotReached();
        }
      }

      return RouteObserverMixin;
    });

export interface RouteObserverMixinInterface {
  currentRouteChanged(newRoute: Route, oldRoute: Route): void;
}
