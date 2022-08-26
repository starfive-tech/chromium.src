// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

// This locks the given app ids in the WebAppProvider system.
//
// Locks can be acquired by using the `WebAppLockManager`. The lock is acquired
// when the callback given to the WebAppLockManager is called. Destruction of
// this class will release the lock or cancel the lock request if it is not
// acquired yet.
class AppLock : public Lock {
 public:
  explicit AppLock(base::flat_set<AppId> app_ids);
  ~AppLock();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_APP_LOCK_H_
