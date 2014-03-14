/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PeerIdentityPrincipal_h
#define PeerIdentityPrincipal_h

#include "nsIPrincipal.h"
#include "mozilla/Attributes.h"
#include "mozilla/PeerIdentityUri.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif

namespace mozilla {

#define NS_PEERIDENTITYPRINCIPAL_CID_STR "b8363663-ad9a-4f5a-9efb-0f073553b34c"
#define NS_PEERIDENTITYPRINCIPAL_CID \
    { 0xb8363663, 0xad9a, 0x4f5a, \
      { 0x9e, 0xfb, 0x0f, 0x07, 0x35, 0x53, 0xb3, 0x4c } }
#define NS_PEERIDENTITYPRINCIPAL_CONTRACTID "@mozilla.org/peeridentityprincipal;1"

class PeerIdentityPrincipal : public nsIPrincipal {
public:
  PeerIdentityPrincipal(const nsAString& aPeerIdentity,
                        nsIPrincipal* base)
    : mUri(new PeerIdentityUri(aPeerIdentity)), mBase(base) {}
  virtual ~PeerIdentityPrincipal() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINCIPAL
  NS_DECL_NSISERIALIZABLE

  const nsString GetPeerIdentity() const { return mUri->GetPeerIdentity(); }
  nsIPrincipal* GetBasePrincipal() const { return mBase; }

private:
  nsresult UriEquals(nsIPrincipal* aOther, bool* aResult);

  nsRefPtr<PeerIdentityUri> mUri;
  nsCOMPtr<nsIPrincipal> mBase;
};

} /* namespace mozilla */

#endif /* PeerIdentityPrincipal_h */
