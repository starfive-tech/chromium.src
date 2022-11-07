// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/face_ml_app_ui/face_ml_app_untrusted_ui.h"

#include "ash/webui/grit/ash_face_ml_app_bundle_resources.h"
#include "ash/webui/grit/ash_face_ml_app_bundle_resources_map.h"
#include "ash/webui/grit/ash_face_ml_app_untrusted_resources.h"
#include "ash/webui/grit/ash_face_ml_app_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace ash {

namespace {

content::WebUIDataSource* CreateFaceMLAppUntrustedDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIFaceMLAppUntrustedURL);
  source->SetDefaultResource(IDR_ASH_FACE_ML_APP_UNTRUSTED_APP_HTML);
  source->AddResourcePaths(base::make_span(
      kAshFaceMlAppUntrustedResources, kAshFaceMlAppUntrustedResourcesSize));

  // Add all resources from ash_face_ml_app_bundle.pak.
  source->AddResourcePaths(base::make_span(kAshFaceMlAppBundleResources,
                                           kAshFaceMlAppBundleResourcesSize));

  source->AddFrameAncestor(GURL(kChromeUIFaceMLAppURL));

  // TODO(b/239374316) Allow to handle blob: and data: URLs.
  // E.g. allow <img>, <audio>, <video> to load blob: and data: URLs.
  // and allow framing blob: URLs for browsable content.

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types lit-html polymer_resin;");

  return source;
}

}  // namespace

FaceMLAppUntrustedUI::FaceMLAppUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      CreateFaceMLAppUntrustedDataSource();

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

FaceMLAppUntrustedUI::~FaceMLAppUntrustedUI() = default;

}  // namespace ash
