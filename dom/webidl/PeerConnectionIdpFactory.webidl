/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface PeerConnectionIdpFactory
{
  /* Makes a worker for the IdP, inserts a message port into the worker
   *  and hands back the corresponding port on this side. */
  [Throws]
  static MessagePort createIdpInstance(DOMString uri, DOMString name);
};
