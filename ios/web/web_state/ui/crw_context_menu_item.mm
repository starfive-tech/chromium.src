// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/crw_context_menu_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWContextMenuItem

+ (CRWContextMenuItem*)itemWithID:(NSString*)ID
                            title:(NSString*)title
                           action:(ProceduralBlock)action {
  return [[CRWContextMenuItem alloc] initWithID:ID title:title action:action];
}

- (instancetype)initWithID:(NSString*)ID
                     title:(NSString*)title
                    action:(ProceduralBlock)action {
  self = [super init];
  if (self) {
    _ID = ID;
    _title = title;
    _action = action;
  }
  return self;
}

@end
