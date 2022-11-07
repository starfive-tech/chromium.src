// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../settings_shared.css.js';
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="internet-shared" assetpath="chrome://resources/">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));
