// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that initiating too many concurrent attributionsrc requests triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/impression.html');
  await page.loadHTML(`<body>`);

  const issuePromise = dp.Audits.onceIssueAdded();

  const maxConcurrentRequests = 30;
  for (let i = 0; i < maxConcurrentRequests + 1; i++) {
    await dp.Runtime.evaluate({
      expression:
          `document.createElement('img').attributionSrc='/inspector-protocol/attribution-reporting/resources/sleep.php'`,
    });
  }
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
