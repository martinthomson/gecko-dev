/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeerIdentityUri.h"

#include <algorithm>

#include "nsEscape.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsISupportsImpl.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"


namespace mozilla {

PeerIdentityUri::PeerIdentityUri(const nsACString& aSpec)
  : mIdnService(), mNormalizedHost(EmptyCString())
{
  nsString identity;
  PeerIdentityUri::Parse(aSpec, identity);
  NS_WARN_IF(identity.IsEmpty());
  mPeerIdentity = identity;
}

static NS_DEFINE_CID(kPeerIdentityUriImplementationCID,
                     NS_PEERIDENTITYURI_CID);

NS_IMPL_ADDREF(PeerIdentityUri)
NS_IMPL_RELEASE(PeerIdentityUri)

NS_INTERFACE_TABLE_HEAD(PeerIdentityUri)
  NS_INTERFACE_TABLE3(PeerIdentityUri, nsISerializable,
                      nsISizeOf, nsIMutable)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  if (aIID.Equals(kPeerIdentityUriImplementationCID))
    foundInterface = static_cast<nsIURI *>(this);
  else
  NS_INTERFACE_MAP_ENTRY(nsIURI)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
PeerIdentityUri::Read(nsIObjectInputStream* aStream)
{
  return aStream->ReadString(mPeerIdentity);
}

NS_IMETHODIMP
PeerIdentityUri::Write(nsIObjectOutputStream* aStream)
{
  return aStream->WriteWStringZ(mPeerIdentity.get());
}

nsresult
PeerIdentityUri::Parse(const nsACString& aSpec, nsAString &identity)
{
  identity.Truncate();

  int32_t colon = aSpec.FindChar(':');
  if (NS_WARN_IF(colon == -1)) {
    return NS_ERROR_DOM_BAD_URI;
  }

  const nsACString& scheme = Substring(aSpec, 0, colon);
  if (NS_WARN_IF(scheme != NS_LITERAL_CSTRING(NS_PEERIDENTITYURI_SCHEME))) {
    return NS_ERROR_DOM_BAD_URI;
  }

  identity = NS_ConvertUTF8toUTF16(Substring(aSpec, colon + 1));
  return NS_OK;
}

size_t
PeerIdentityUri::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mPeerIdentity.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

size_t
PeerIdentityUri::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

/* attribute boolean mutable; */
NS_IMETHODIMP
PeerIdentityUri::GetMutable(bool *aMutable)
{
  *aMutable = false;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetMutable(bool aMutable)
{
  return NS_ERROR_FAILURE;
}

#define NS_PEERIDENTITYURI_PREFIX NS_PEERIDENTITYURI_SCHEME ":"

NS_IMETHODIMP
PeerIdentityUri::GetSpec(nsACString & aSpec) {
  aSpec = NS_LITERAL_CSTRING(NS_PEERIDENTITYURI_PREFIX) +
      NS_ConvertUTF16toUTF8(mPeerIdentity);
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::GetAsciiSpec(nsACString & aAsciiSpec)
{
  aAsciiSpec.Truncate();

  nsCString user;
  nsresult rv = GetUsername(user);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString host;
  rv = GetAsciiHost(host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString escapedUser;
  aAsciiSpec = NS_LITERAL_CSTRING(NS_PEERIDENTITYURI_PREFIX) +
    NS_EscapeURL(user, esc_OnlyNonASCII | esc_AlwaysCopy, escapedUser) +
    NS_LITERAL_CSTRING("@") + host;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetSpec(const nsACString & aSpec) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetPrePath(nsACString & aPrePath)
{
  return GetSpec(aPrePath);
}

NS_IMETHODIMP
PeerIdentityUri::GetScheme(nsACString & aScheme)
{
  aScheme.AssignLiteral(NS_PEERIDENTITYURI_SCHEME);
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetScheme(const nsACString & aScheme)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetUserPass(nsACString & aUserPass)
{
  return GetUsername(aUserPass);
}

NS_IMETHODIMP
PeerIdentityUri::SetUserPass(const nsACString & aUserPass)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetUsername(nsACString & aUsername)
{
  aUsername.Truncate();

  int32_t at = mPeerIdentity.FindChar('@');
  if (at < 0) {
    return NS_OK;
  }
  aUsername = NS_ConvertUTF16toUTF8(Substring(mPeerIdentity, 0, at));
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetUsername(const nsACString & aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetPassword(nsACString & aPassword)
{
  aPassword.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetPassword(const nsACString & aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetHostPort(nsACString & aHostPort)
{
  return GetHost(aHostPort);
}

NS_IMETHODIMP
PeerIdentityUri::SetHostPort(const nsACString & aHostPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetHost(nsACString & aHost)
{
  aHost.Truncate();

  int32_t at = mPeerIdentity.FindChar('@');
  nsString host = mPeerIdentity;
  if (at >= 0) {
    host = Substring(mPeerIdentity, at + 1);
  }

  aHost = NS_ConvertUTF16toUTF8(host);
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetHost(const nsACString & aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetPort(int32_t *aPort)
{
  *aPort = 0;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetPort(int32_t aPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetPath(nsACString & aPath)
{
  aPath.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetPath(const nsACString & aPath)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::Equals(nsIURI *aOther, bool *aEquals)
{
  *aEquals = false;
  if (!aOther) {
    return NS_OK;
  }

  nsRefPtr<PeerIdentityUri> otherURI;
  nsresult rv = aOther->QueryInterface(kPeerIdentityUriImplementationCID,
                                       getter_AddRefs(otherURI));
  if (NS_SUCCEEDED(rv)) {
    *aEquals = Equals(otherURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::NormalizeHost()
{
  if (!mNormalizedHost.IsEmpty()) {
    return NS_OK;
  }

  nsresult rv;
  if (!mIdnService) {
    mIdnService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCString utf8Host;
  rv = GetHost(utf8Host);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = mIdnService->ConvertUTF8toACE(utf8Host, mNormalizedHost);
  NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

bool
PeerIdentityUri::Equals(PeerIdentityUri *aOther)
{
  NormalizeHost();
  aOther->NormalizeHost();

  size_t at = std::max(mPeerIdentity.FindChar('@'), 0);
  if (at >= aOther->mPeerIdentity.Length() ||
      aOther->mPeerIdentity.CharAt(at) != '@') {
    return false;
  }
  nsString user(Substring(mPeerIdentity, 0, at));
  nsString otherUser(Substring(aOther->mPeerIdentity, 0, at));

  return user == otherUser && mNormalizedHost == aOther->mNormalizedHost;
}

NS_IMETHODIMP
PeerIdentityUri::SchemeIs(const char * aScheme, bool *aRetval)
{
  *aRetval = !strncmp(aScheme, NS_PEERIDENTITYURI_SCHEME,
      sizeof(NS_PEERIDENTITYURI_SCHEME) + 1);
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::Resolve(const nsACString & relativePath, nsACString & _retval)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::GetAsciiHost(nsACString & aAsciiHost)
{
  nsresult rv = NormalizeHost();
  if (NS_FAILED(rv)) {
    return rv;
  }
  aAsciiHost = mNormalizedHost;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::GetOriginCharset(nsACString & aOriginCharset)
{
  aOriginCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::GetRef(nsACString & aRef)
{
  aRef.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::SetRef(const nsACString & aRef)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityUri::EqualsExceptRef(nsIURI *other, bool *_retval)
{
  return Equals(other, _retval);
}

NS_IMETHODIMP
PeerIdentityUri::Clone(nsIURI * *aRetval)
{
  *aRetval = new PeerIdentityUri(mPeerIdentity);
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityUri::CloneIgnoringRef(nsIURI * *_retval)
{
  return Clone(_retval);
}

NS_IMETHODIMP
PeerIdentityUri::GetSpecIgnoringRef(nsACString & aSpecIgnoringRef)
{
  return GetSpec(aSpecIgnoringRef);
}

NS_IMETHODIMP
PeerIdentityUri::GetHasRef(bool *aHasRef)
{
  *aHasRef = false;
  return NS_OK;
}

} /* namespace mozilla */
