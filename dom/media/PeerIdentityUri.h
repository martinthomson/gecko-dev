 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * vim: sw=2 ts=2 sts=2 expandtab
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PeerIdentityUri_h
#define PeerIdentityUri_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsISizeOf.h"
#include "nsISerializable.h"
#include "nsIMutable.h"
#include "nsIIDNService.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif


class nsIObjectInputStream;
class nsIObjectOutputStream;

namespace mozilla {

#define NS_PEERIDENTITYURI_CID_STR "12f30117-c472-4e2b-a4bf-9051d752a766"
#define NS_PEERIDENTITYURI_CID \
    { 0x12f30117, 0xc472, 0x4e2b, \
      { 0xa4, 0xbf, 0x90, 0x51, 0xd7, 0x52, 0xa7, 0x66 } }
#define NS_PEERIDENTITYURI_SCHEME "moz-peeridentity"

class PeerIdentityUri MOZ_FINAL : public nsIURI,
                                  public nsISizeOf,
                                  public nsISerializable,
                                  public nsIMutable
{
public:
  PeerIdentityUri(const nsAString& aPeerIdentity)
    : mPeerIdentity(aPeerIdentity), mIdnService(),
    mNormalizedHost(EmptyCString()) {}
  PeerIdentityUri(const nsACString& aSpec);
  ~PeerIdentityUri() {}

  static nsresult Parse(const nsACString& aSpec, nsAString& identity);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSIMUTABLE

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  const nsString GetPeerIdentity() const { return mPeerIdentity; }

private:
  bool Equals(PeerIdentityUri *aOther);
  NS_IMETHOD NormalizeHost();

  nsString mPeerIdentity;

  nsCOMPtr<nsIIDNService> mIdnService;
  nsCString mNormalizedHost;
};

} /* namespace mozilla */

#endif /* PeerIdentityUri_h */
