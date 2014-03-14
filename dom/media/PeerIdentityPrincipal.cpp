/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeerIdentityPrincipal.h"

#include "nsIClassInfoImpl.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

NS_IMPL_CLASSINFO(PeerIdentityPrincipal, nullptr, nsIClassInfo::MAIN_THREAD_ONLY,
                  NS_PEERIDENTITYPRINCIPAL_CID)
NS_IMPL_ISUPPORTS2_CI(PeerIdentityPrincipal, nsIPrincipal, nsISerializable)


nsresult
PeerIdentityPrincipal::UriEquals(nsIPrincipal *aOther, bool *aResult)
{
  *aResult = false;
  if (!aOther) {
    NS_WARNING("Comparing null principal");
    return NS_OK;
  }

  if (aOther == this) {
    *aResult = true;
    return NS_OK;
  }

  nsRefPtr<nsIURI> otherURI;
  nsresult rv = aOther->GetURI(getter_AddRefs(otherURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return mUri->Equals(otherURI, aResult);
}

NS_IMETHODIMP
PeerIdentityPrincipal::Equals(nsIPrincipal *aOther, bool *aResult)
{
  nsresult rv = UriEquals(aOther, aResult);
  if (NS_SUCCEEDED(rv) && *aResult && mBase) {
    *aResult = false;

    nsCOMPtr<PeerIdentityPrincipal> other = do_QueryInterface(aOther, &rv);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    rv = mBase->Equals(other->mBase, aResult);
  }
  return rv;
}

NS_IMETHODIMP
PeerIdentityPrincipal::EqualsConsideringDomain(nsIPrincipal *aOther,
                                               bool *aResult)
{
  nsresult rv = UriEquals(aOther, aResult);
  if (NS_SUCCEEDED(rv) && *aResult && mBase) {
    *aResult = false;

    nsCOMPtr<PeerIdentityPrincipal> other = do_QueryInterface(aOther, &rv);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    rv = mBase->EqualsConsideringDomain(other->mBase, aResult);
  }
  return rv;
}

NS_IMETHODIMP
PeerIdentityPrincipal::Read(nsIObjectInputStream* aStream)
{
  return mUri->Read(aStream);
}

NS_IMETHODIMP
PeerIdentityPrincipal::Write(nsIObjectOutputStream* aStream)
{
  return mUri->Write(aStream);
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetHashValue(uint32_t *aHashValue)
{
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetURI(nsIURI * *aURI)
{
#ifdef MOZILLA_INTERNAL_API
  return NS_EnsureSafeToReturn(mUri, aURI);
#else
  *aURI = nullptr;
  return NS_ERROR_NULL_POINTER;
#endif
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetDomain(nsIURI * *aDomain)
{
  // domain is used for script-based principals to store the value set by
  // scripts on window.document.domain; this doesn't matter here
  *aDomain = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::SetDomain(nsIURI *aDomain)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetOrigin(char * *aOrigin)
{
  *aOrigin = nullptr;

  nsCString str;
  nsresult rv = mUri->GetSpec(str);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aOrigin = ToNewCString(str);
  if (NS_WARN_IF(!*aOrigin)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/**
 * Check that this principal subsumes the other.  Two cases are permitted: One
 * where this principal has an identical peerIdentity; and another where the
 * other principal is the principal for the page (and this is just a restriction
 * of the same).
 *
 * Note: peerIdentity subsumes a normal principal, sometimes; but a normal
 * principal never subsumes peerIdentity.
 */
NS_IMETHODIMP
PeerIdentityPrincipal::SubsumesConsideringDomain(nsIPrincipal *aOther, bool *aResult)
{
  nsresult rv = Equals(aOther, aResult);
  if (NS_SUCCEEDED(rv) && !*aResult && mBase) {
    rv = mBase->SubsumesConsideringDomain(aOther, aResult);
  }
  return rv;
}

NS_IMETHODIMP
PeerIdentityPrincipal::Subsumes(nsIPrincipal *aOther, bool *aResult)
{
  nsresult rv = Equals(aOther, aResult);
  if (NS_SUCCEEDED(rv) && !*aResult && mBase) {
    rv = mBase->Subsumes(aOther, aResult);
  }
  return rv;
}

NS_IMETHODIMP
PeerIdentityPrincipal::CheckMayLoad(nsIURI *aUri, bool aReport, bool aAllowIfInheritsPrincipal)
{
  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetCsp(nsIContentSecurityPolicy * *aCsp)
{
  *aCsp = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::SetCsp(nsIContentSecurityPolicy *aCsp)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetJarPrefix(nsACString & aJarPrefix)
{
  aJarPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetBaseDomain(nsACString & aBaseDomain)
{
  return mUri->GetHostPort(aBaseDomain);
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetAppStatus(uint16_t *aAppStatus)
{
  *aAppStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetAppId(uint32_t *aAppId)
{
  *aAppId = nsIScriptSecurityManager::NO_APP_ID;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetIsInBrowserElement(bool *aIsInBrowserElement)
{
  *aIsInBrowserElement = false;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetUnknownAppId(bool *aUnknownAppId)
{
  *aUnknownAppId = false;
  return NS_OK;
}

NS_IMETHODIMP
PeerIdentityPrincipal::GetIsNullPrincipal(bool *aIsNullPrincipal)
{
  *aIsNullPrincipal = false;
  return NS_OK;
}

} /* namespace mozilla */
