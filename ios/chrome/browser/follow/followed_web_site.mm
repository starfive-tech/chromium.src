// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/followed_web_site.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowedWebSite

- (instancetype)initWithTitle:(NSString*)title
                   webPageURL:(NSURL*)webPageURL
                   faviconURL:(NSURL*)faviconURL
                       RSSURL:(NSURL*)RSSURL
                    available:(BOOL)available {
  if ((self = [super init])) {
    _title = [title copy];
    _webPageURL = webPageURL;
    _faviconURL = faviconURL;
    _RSSURL = RSSURL;
    _available = available;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;

  if (![object isMemberOfClass:[FollowedWebSite class]])
    return NO;

  return [self isEqualToFollowedWebSite:object];
}

- (NSUInteger)hash {
  return [self.title hash] ^ [self.webPageURL hash] ^ [self.faviconURL hash] ^
         [self.RSSURL hash];
}

#pragma mark - Private

- (BOOL)isEqualToFollowedWebSite:(FollowedWebSite*)channel {
  DCHECK(channel);
  return [self.title isEqualToString:channel.title] &&
         [self.webPageURL isEqual:channel.webPageURL] &&
         [self.faviconURL isEqual:channel.faviconURL] &&
         [self.RSSURL isEqual:channel.RSSURL];
}

@end
