// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_first_run_variations_seed_manager.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_url_constants.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/variations/ios_chrome_seed_response.h"
#import "ios/chrome/common/channel_info.h"
#import "net/http/http_status_code.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Maximum time allowed to fetch the seed before the request is cancelled.
const base::TimeDelta kRequestTimeout = base::Seconds(3);

// Whether a current request for variations seed is being made; this variable
// exists that only one instance of the manager updates the global seed at one
// time.
static BOOL g_seed_fetching_in_progress = NO;

// The source of truth of the entire app for the stored seed.
static IOSChromeSeedResponse* g_shared_seed = nil;

}  // namespace

@interface IOSChromeFirstRunVariationsSeedManager () {
  // The variations server domain name.
  std::string _variationsDomain;

  // The forced channel string retrieved from the command line.
  std::string _forcedChannel;
}

// Whether the current binary should fetch Finch seed for experiment purpose.
@property(nonatomic, readonly) BOOL fetchingEnabled;

// The URL of the variations server, including query parameters that identifies
// the request initiator.
@property(nonatomic, readonly) NSURL* variationsUrl;

// The timestamp when the current seed request starts. This is used for metric
// reporting, and will be reset to `nil` when the request finishes.
@property(nonatomic, strong) NSDate* startTimeOfOngoingSeedRequest;

// The fetched seed response.
@property(nonatomic, readonly) IOSChromeSeedResponse* seed;

@end

@implementation IOSChromeFirstRunVariationsSeedManager

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    _fetchingEnabled = YES;
#else
    _fetchingEnabled = NO;
#endif
    _variationsDomain = variations::kDefaultServerUrl;
    _forcedChannel = std::string();
    [self applySwitchesFromCommandLine];
  }
  return self;
}

- (void)startSeedFetch {
  // Set up a serial queue to to avoid concurrent read/write to static data.
  static dispatch_once_t onceToken;
  static dispatch_queue_t queue = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* label = "com.google.chrome.first_run_variations_seed_manager";
#else
  const char* label = "org.chromium.first_run_variations_seed_manager";
#endif
  dispatch_once(&onceToken, ^{
    queue = dispatch_queue_create(label, DISPATCH_QUEUE_SERIAL);
  });

  // Adds the task of fetching the seed to the static serial queue, and return
  // from `startSeedFetch` immediately. Note that the block will retain `self`.
  dispatch_async(queue, ^{
    [self startSeedFetchHelper];
  });
}

- (IOSChromeSeedResponse*)popSeed {
  IOSChromeSeedResponse* seed = self.seed;
  [self updateSharedSeed:nil];
  return seed;
}

#pragma mark - Accessors

- (IOSChromeSeedResponse*)seed {
  return g_shared_seed;
}

- (NSURL*)variationsUrl {
  // Setting "osname", "milestone" and "channel" as parameters. Dogfood
  // experimenting is not supported on Chrome iOS, therefore we do not need the
  // "restrict" parameter.
  std::string queryString =
      "?osname=ios&milestone=" + version_info::GetMajorVersionNumber();
  std::string channel = _forcedChannel;
  if (channel.empty() && GetChannel() != version_info::Channel::UNKNOWN) {
    channel = GetChannelString();
  }
  if (!channel.empty()) {
    queryString += "&channel=" + channel;
  }
  return [NSURL
      URLWithString:base::SysUTF8ToNSString(_variationsDomain + queryString)];
}

#pragma mark - Private

// Parse custom values from the command line and apply them to the seed manager.
- (void)applySwitchesFromCommandLine {
  NSArray<NSString*>* arguments = [[NSProcessInfo processInfo] arguments];
  std::string url_switch =
      "--" + std::string(variations::switches::kVariationsServerURL) + "=";
  std::string channel_switch =
      "--" + std::string(variations::switches::kFakeVariationsChannel) + "=";
  for (NSString* a in arguments) {
    std::string arg = base::SysNSStringToUTF8(a);

    if (base::StartsWith(arg, url_switch)) {
      _variationsDomain = arg.substr(url_switch.size());
      if (!_fetchingEnabled && !_variationsDomain.empty()) {
        _fetchingEnabled = YES;
      }
    } else if (base::StartsWith(arg, channel_switch)) {
      _forcedChannel = arg.substr(channel_switch.size());
    }
  }
}

// Helper method for `startSeedFetch` that initiates an HTTPS request to the
// Finch server in the static serial queue.
- (void)startSeedFetchHelper {
  DCHECK(g_seed_fetching_in_progress)
      << "SeedFetch started while already in progress";

  // Stops executing if seed fetching is disabled.
  if (!self.fetchingEnabled) {
    [self notifyDelegateSeedFetchResult:NO];
    [self recordSeedFetchResult];
    return;
  }

  g_seed_fetching_in_progress = YES;
  NSMutableURLRequest* request = [NSMutableURLRequest
       requestWithURL:self.variationsUrl
          cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
      timeoutInterval:kRequestTimeout.InSecondsF()];
  // Pass only "gzip" as an accepted format. Do not pass delta compression
  // ("x-bm"), as it is not applicable on first run (since there is no
  // existing seed).
  [request setValue:@"gzip" forHTTPHeaderField:@"A-IM"];
  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response,
                            NSError* error) {
          [self onSeedRequestCompletedWithData:data
                                      response:(NSHTTPURLResponse*)response
                                         error:error];
        }];
  self.startTimeOfOngoingSeedRequest = [NSDate now];
  [task resume];
}

// Method that generates the seed using the HTTPS response sent back from the
// Finch server, stores them in the shared seed, and records relevant metrics.
- (void)onSeedRequestCompletedWithData:(NSData*)data
                              response:(NSHTTPURLResponse*)httpResponse
                                 error:(NSError*)error {
  // Normally net::HTTP_NOT_MODIFIED should be considered as a
  // successful response, but it is not expected when the request does
  // not contain "If-None-Match" header.
  BOOL success = error == nil && httpResponse.statusCode == net::HTTP_OK;
  if (success) {
    // TODO(crbug.com/1353937): Log time delta.
    IOSChromeSeedResponse* seed = [self seedResponseForHTTPResponse:httpResponse
                                                               data:data];
    if (seed == nil) {
      success = NO;
    } else {
      [self updateSharedSeed:seed];
    }
  }
  [self recordSeedFetchResult];
  self.startTimeOfOngoingSeedRequest = nil;
  g_seed_fetching_in_progress = NO;

  [self notifyDelegateSeedFetchResult:success];
}

// Generates and returns the SeedResponse by parsing the HTTP response returned
// by the variations server. Returns `nil` if the HTTP response is invalid.
- (IOSChromeSeedResponse*)seedResponseForHTTPResponse:
                              (NSHTTPURLResponse*)httpResponse
                                                 data:(NSData*)data {
  NSString* signature =
      [httpResponse valueForHTTPHeaderField:@"X-Seed-Signature"];
  NSString* country = [httpResponse valueForHTTPHeaderField:@"X-Country"];

  // Returned seed should have been gzip compressed.
  NSArray<NSString*>* instanceManipulations = [[httpResponse
      valueForHTTPHeaderField:@"IM"] componentsSeparatedByString:@","];
  NSCharacterSet* whitespace = [NSCharacterSet whitespaceCharacterSet];
  // Only gzip compressed data is supported on first run seed fetching with
  // "gzip" specified in the request.
  if ([instanceManipulations count] != 1 ||
      ![[instanceManipulations[0] stringByTrimmingCharactersInSet:whitespace]
          isEqualToString:@"gzip"]) {
    [self recordSeedFetchResult];
    return nil;
  }

  IOSChromeSeedResponse* seed =
      [[IOSChromeSeedResponse alloc] initWithSignature:signature
                                               country:country
                                                  time:[NSDate now]
                                                  data:data
                                            compressed:YES];
  return seed;
}

// Records the seed fetch result on UMA.
- (void)recordSeedFetchResult {
  // TODO(crbug.com/3835653): First parameter of this method should be an enum
  // that is yet to be implemented. Log metrics for seed fetch result.
  return;
}

// Updates the shared seed response. This method is extracted out for testing
// purposes.
- (void)updateSharedSeed:(IOSChromeSeedResponse*)seed {
  g_shared_seed = seed;
}

// Notifies the delegate of the seed fetching result. Since the seed fetch
// request is sent on the global queue instead of the main queue, this method
// should explicitly dispatch the result back on the main queue.
// TODO(crbug.com/3835653): Merge with `recordSeedFetchResult`.
- (void)notifyDelegateSeedFetchResult:(BOOL)result {
  if (self.delegate) {
    __weak IOSChromeFirstRunVariationsSeedManager* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf.delegate didFetchSeedSuccess:result];
    });
  }
}

@end
