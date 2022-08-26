// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AGENT_H_

#import "ios/chrome/browser/ui/main/observing_scene_state_agent.h"

// A scene agent that monitors the state of the app and informs the Promos
// Manager it's a valid time to show a promo. Promos are shown when the UI is
// available (i.e. the app & scene state allow it).
//
// The UI is considered available when the following conditions are met:
//
// (1) the app initialization is over (the stage InitStageFinal is reached),
// (2) the scene is in the foreground,
// (3) there is no UI blocker,
// (4) the app isn't shutting down,
// (5) there are no launch intents, and
// (6) the last session wasn't a crash.
//
// There are 3 events that can trigger a promo:
//
// (1) reaching the InitStageFinal init stage,
// (2) the scene becomes active in the foreground, and
// (3) the UI blocker is removed.
//
// In a multi-window context, only one scene will present the promo. The first
// scene to receive the event that triggers the promo will be the one selected.
// Another scene will be selected if the presenting scene is dismissed.
@interface PromosManagerSceneAgent : ObservingSceneAgent

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AGENT_H_
