// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals,
    browserSignals) {
  validateAdMetadata(adMetadata);
  validateBid(bid);
  validateAuctionConfig(auctionConfig);
  validateTrustedScoringSignals(trustedScoringSignals);
  validateBrowserSignals(browserSignals);
  return bid;
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render","metadata":{"ad":"metadata","here":[1,2,3]}}')
    throw 'Wrong adMetadata ' + adMetadataJSON;
}

function validateBid(bid) {
  if (bid !== 2)
    throw 'Wrong bid ' + bid;
}

function validateAuctionConfig(auctionConfig) {
  if (Object.keys(auctionConfig).length !== 10) {
    throw 'Wrong number of auctionConfig fields ' +
        JSON.stringify(auctionConfig);
  }

  if (!auctionConfig.seller.includes('b.test'))
    throw 'Wrong seller ' + auctionConfig.seller;

  if (auctionConfig.decisionLogicUrl !==
      auctionConfig.seller + '/interest_group/decision_argument_validator.js') {
    throw 'Wrong decisionLogicUrl ' + auctionConfig.decisionLogicUrl;
  }

  if (auctionConfig.trustedScoringSignalsUrl !==
    auctionConfig.seller + '/interest_group/trusted_scoring_signals.json') {
    throw 'Wrong trustedScoringSignalsUrl ' +
        auctionConfig.trustedScoringSignalsUrl;
  }

  // TODO(crbug.com/1186444): Consider validating URL fields like
  // auctionConfig.decisionLogicUrl once we decide what to do about URL
  // normalization.

  if (auctionConfig.interestGroupBuyers.length !== 2 ||
      !auctionConfig.interestGroupBuyers[0].startsWith('https://a.test') ||
      !auctionConfig.interestGroupBuyers[1].startsWith('https://d.test')) {
    throw 'Wrong interestGroupBuyers ' +
        JSON.stringify(auctionConfig.interestGroupBuyers);
  }

  const buyerAOrigin = auctionConfig.interestGroupBuyers[0];
  const buyerBOrigin = auctionConfig.interestGroupBuyers[1];

  // If auctionSignals is passed as a JSON string instead of an object,
  // stringify() will wrap it in another layer of quotes, causing the test to
  // fail. The order of properties produced by stringify() isn't guaranteed by
  // the ECMAScript standard, but some sites depend on the V8 behavior of
  // serializing in declaration order.
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '{"so":"I","hear":["you","like","json"]}')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '{"signals":"from","the":["seller"]}')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJSON;
  if (auctionConfig.sellerTimeout !== 200)
    throw 'Wrong sellerTimeout ' + auctionConfig.sellerTimeout;

  if (JSON.stringify(auctionConfig.perBuyerSignals[buyerAOrigin]) !==
          '{"signalsForBuyer":1}') {
    throw 'Wrong perBuyerSignals ' +
        JSON.stringify(auctionConfig.perBuyerSignals);
  }

  if (auctionConfig.perBuyerTimeouts[buyerAOrigin] !== 110 ||
      auctionConfig.perBuyerTimeouts[buyerBOrigin] !== 120 ||
      auctionConfig.perBuyerTimeouts['*'] !== 150) {
    throw 'Wrong perBuyerTimeouts ' +
        JSON.stringify(auctionConfig.perBuyerTimeouts);
  }

  const perBuyerPrioritySignals = auctionConfig.perBuyerPrioritySignals;
  if (Object.keys(perBuyerPrioritySignals).length !== 2 ||
      JSON.stringify(perBuyerPrioritySignals[buyerAOrigin]) !==
         '{"foo":1}' ||
      JSON.stringify(perBuyerPrioritySignals['*']) !==
         '{"BaR":-2}') {
    throw 'Wrong perBuyerPrioritySignals ' +
        JSON.stringify(perBuyerPrioritySignals);
  }

  if ('componentAuctions' in auctionConfig) {
    throw 'Unexpected componentAuctions ' +
        JSON.stringify(auctionConfig.componentAuctions);
  }
}

function validateTrustedScoringSignals(signals) {
  if (signals.renderUrl["https://example.com/render"] !== "foo") {
    throw 'Wrong trustedScoringSignals.renderUrl ' +
        signals.renderUrl["https://example.com/render"];
  }
  if (signals.adComponentRenderUrls["https://example.com/render-component"] !==
      1) {
    throw 'Wrong trustedScoringSignals.adComponentRenderUrls ' +
        signals.adComponentRenderUrls["https://example.com/render-component"];
  }
}

function validateBrowserSignals(browserSignals) {
  if (browserSignals.topWindowHostname !== 'c.test')
    throw 'Wrong topWindowHostname ' + browserSignals.topWindowHostname;
  if ('topLevelSeller' in browserSignals)
    throw 'Wrong topLevelSeller ' + browserSignals.topLevelSeller;
  if ("componentSeller" in browserSignals)
    throw 'Wrong componentSeller ' + browserSignals.componentSeller;
  if (!browserSignals.interestGroupOwner.startsWith('https://a.test'))
    throw 'Wrong interestGroupOwner ' + browserSignals.interestGroupOwner;
  if (browserSignals.renderUrl !== "https://example.com/render")
    throw 'Wrong renderUrl ' + browserSignals.renderUrl;
  const adComponentsJSON = JSON.stringify(browserSignals.adComponents);
  if (adComponentsJSON !== '["https://example.com/render-component"]')
    throw 'Wrong adComponents ' + browserSignals.adComponents;
  if (browserSignals.biddingDurationMsec < 0)
    throw 'Wrong biddingDurationMsec ' + browserSignals.biddingDurationMsec;
}
