// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/ax_screen_ai_annotator_factory.h"

#include "chrome/browser/accessibility/ax_screen_ai_annotator.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace screen_ai {

// static
screen_ai::AXScreenAIAnnotator*
AXScreenAIAnnotatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<screen_ai::AXScreenAIAnnotator*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AXScreenAIAnnotatorFactory* AXScreenAIAnnotatorFactory::GetInstance() {
  static base::NoDestructor<AXScreenAIAnnotatorFactory> instance;
  return instance.get();
}

AXScreenAIAnnotatorFactory::AXScreenAIAnnotatorFactory()
    : BrowserContextKeyedServiceFactory(
          "AXScreenAIAnnotator",
          BrowserContextDependencyManager::GetInstance()) {}

AXScreenAIAnnotatorFactory::~AXScreenAIAnnotatorFactory() = default;

KeyedService* AXScreenAIAnnotatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new screen_ai::AXScreenAIAnnotator(context);
}

// Incognito profiles should use their own instance.
content::BrowserContext* AXScreenAIAnnotatorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace screen_ai
