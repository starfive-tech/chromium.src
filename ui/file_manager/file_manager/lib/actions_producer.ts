// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from './base_store.js';

/**
 * ActionsProducer is a generator function that yields and/or returns Actions.
 *
 * It's a function that return a AsyncGenerator.
 *
 * @template TYield: Is the type of the yielded/returned action.
 * @template Args: It's the inferred types of the arguments in the function.
 *
 * Example of an ActionsProducer:
 *   async function* doSomething(): ActionsProducerGen<SomeAction> {
 *     yield {type: 'some-action', status: 'STARTING'};
 *     // Call Some API.
 *     const result = await someApi();
 *     yield {type: 'some-action', status: 'SUCCESS', result};
 *   }
 */
export type ActionsProducer<TYield extends BaseAction, Args extends any[]> =
    (...args: Args) => ActionsProducerGen<TYield>;

/**
 * This is the type of the generator that is returned by the ActionsProducer.
 *
 * This is used to enforce the same type for yield and return, since the
 * built-in template accepts different types for both.
 * @template TYield the type for the action yielded by the ActionsProducer.
 */
export type ActionsProducerGen<TYield> =
    AsyncGenerator<void|TYield, void|TYield>;

/**
 * Exception used to stop ActionsProducer when they're no longer valid.
 *
 * The concurrency model function uses this exception to force the
 * ActionsProducer to stop.
 */
export class ConcurrentActionInvalidatedError extends Error {}

/** Helper to distinguish the Action from a ActionsProducer.  */
export function isActionsProducer(
    value: BaseAction|
    ActionsProducerGen<BaseAction>): value is ActionsProducerGen<BaseAction> {
  return (
      (value as ActionsProducerGen<BaseAction>).next !== undefined &&
      (value as ActionsProducerGen<BaseAction>).throw !== undefined);
}
