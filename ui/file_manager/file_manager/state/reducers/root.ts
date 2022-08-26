// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Action, ActionType} from '../actions.js';

import {cacheEntries, clearCachedEntries} from './all_entries.js';
import {changeDirectory} from './current_directory.js';
import {search} from './search.js';

/**
 * Root reducer for the State for Files app.
 * It dispatches the ` action` to other reducers that update each part of the
 * State.
 *
 * Every top-level attribute of the State should have its reducer being called
 * from here.
 */
export function rootReducer(currentState: State, action: Action): State {
  // Before any actual Reducer, we cache the entries, so the reducers can just
  // use any entry from `allEntries`.
  const state = cacheEntries(currentState, action);

  switch (action.type) {
    case ActionType.CHANGE_DIRECTORY:
      return Object.assign(state, {
        currentDirectory: changeDirectory(state, action),
      });

    case ActionType.CLEAR_STALE_CACHED_ENTRIES:
      return clearCachedEntries(state, action);

    case ActionType.SEARCH:
      return Object.assign(state, {
        search: search(state, action),
      });

    default:
      console.error(`invalid action: ${action}`);
      return state;
  }
}
