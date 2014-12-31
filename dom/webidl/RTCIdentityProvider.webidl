/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This is currently only a proposal.
 */

[NoInterfaceObject]
interface RTCIdentityProviderRegistrar {
  void register(RTCIdentityProvider idp);

  [ChromeOnly]
  readonly attribute RTCIdentityProvider? idp;
  [ChromeOnly, Throws]
  Promise<RTCIdentityAssertionResult>
  generateAssertion(DOMString contents, DOMString origin,
                    optional DOMString usernameHint);
  [ChromeOnly, Throws]
  Promise<RTCIdentityValidationResult>
  validateAssertion(DOMString assertion, DOMString origin);
};

callback interface RTCIdentityProvider {
  Promise<RTCIdentityAssertionResult>
  generateAssertion(DOMString contents, DOMString origin,
                    optional DOMString usernameHint);
  Promise<RTCIdentityValidationResult>
  validateAssertion(DOMString assertion, DOMString origin);
};

dictionary RTCIdentityAssertionResult {
  required RTCIdentityProviderDetails idp;
  required DOMString assertion;
};

dictionary RTCIdentityProviderDetails {
  required DOMString domain;
  DOMString protocol = "default";
};

dictionary RTCIdentityValidationResult {
  required DOMString identity;
  required DOMString contents;
};
