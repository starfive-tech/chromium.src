// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace autofill_assistant {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAssistant;
extern const base::Feature kAutofillAssistantAnnotateDom;
extern const base::Feature kAutofillAssistantChromeEntry;
extern const base::Feature kAutofillAssistantCudFilterProfiles;
extern const base::Feature kAutofillAssistantDesktop;
extern const base::Feature kAutofillAssistantDialogOnboarding;
extern const base::Feature kAutofillAssistantDirectActions;
extern const base::Feature kAutofillAssistantDisableOnboardingFlow;
extern const base::Feature kAutofillAssistantDisableProactiveHelpTiedToMSBB;
extern const base::Feature kAutofillAssistantFeedbackChip;
extern const base::Feature kAutofillAssistantFullJsFlowStackTraces;
extern const base::Feature kAutofillAssistantFullJsSnippetStackTraces;
extern const base::Feature kAutofillAssistantGetPaymentsClientToken;
extern const base::Feature kAutofillAssistantGetTriggerScriptsByHashPrefix;
extern const base::Feature kAutofillAssistantInCCTTriggering;
extern const base::Feature kAutofillAssistantInTabTriggering;
extern const base::Feature kAutofillAssistantLoadDFMForTriggerScripts;
extern const base::Feature kAutofillAssistantProactiveHelp;
extern const base::Feature kAutofillAssistantRemoteAssistantUi;
extern const base::Feature kAutofillAssistantSignGetActionsRequests;
extern const base::Feature
    kAutofillAssistantSignGetNoRoundTripScriptsByHashRequests;
extern const base::Feature kAutofillAssistantUrlHeuristic1;
extern const base::Feature kAutofillAssistantUrlHeuristic2;
extern const base::Feature kAutofillAssistantUrlHeuristic3;
extern const base::Feature kAutofillAssistantUrlHeuristic4;
extern const base::Feature kAutofillAssistantUrlHeuristic5;
extern const base::Feature kAutofillAssistantUrlHeuristics;
extern const base::Feature kAutofillAssistantUseDidFinishNavigation;
extern const base::Feature kAutofillAssistantVerifyGetActionsResponses;
extern const base::Feature
    kAutofillAssistantVerifyGetNoRoundTripScriptsByHashResponses;

}  // namespace features
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FEATURES_H_
