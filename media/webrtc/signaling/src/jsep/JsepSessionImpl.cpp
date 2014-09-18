/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/jsep/JsepSessionImpl.h"

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include <mozilla/Move.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SipccSdp.h"

namespace mozilla {
namespace jsep {

nsresult JsepSessionImpl::AddTrack(const RefPtr<JsepMediaStreamTrack>& track) {
  JsepSendingTrack strack;
  strack.mTrack = track;

  mSendingTracks.push_back(strack);

  return NS_OK;
}

nsresult JsepSessionImpl::track(size_t index,
                                RefPtr<JsepMediaStreamTrack>* track) const {
  if (index >= mSendingTracks.size()) {
    return NS_ERROR_INVALID_ARG;
  }

  *track = mSendingTracks[index].mTrack;

  return NS_OK;
}

nsresult JsepSessionImpl::CreateOffer(const JsepOfferOptions& options,
                                      std::string* offer) {
  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  if (NS_FAILED(rv))
    return rv;

  // Now add all the m-lines that we are attempting to negotiate.

  // TODO(ekr@rtfm.com): Do renegotiation.

  *offer = "XXX";

  return NS_OK;
}

nsresult JsepSessionImpl::CreateGenericSDP(UniquePtr<Sdp>* sdpp) {
  // draft-ietf-rtcweb-jsep-08 Section 5.2.1:
  //  o  The second SDP line MUST be an "o=" line, as specified in
  //     [RFC4566], Section 5.2.  The value of the <username> field SHOULD
  //     be "-".  The value of the <sess-id> field SHOULD be a
  //     cryptographically random number.  To ensure uniqueness, this
  //     number SHOULD be at least 64 bits long.  The value of the <sess-
  //     version> field SHOULD be zero.  The value of the <nettype>
  //     <addrtype> <unicast-address> tuple SHOULD be set to a non-
  //     meaningful address, such as IN IP4 0.0.0.0, to prevent leaking the
  //     local address in this field.  As mentioned in [RFC4566], the
  //     entire o= line needs to be unique, but selecting a random number
  //     for <sess-id> is sufficient to accomplish this.

  // Generate the sess-id if it is zero (the chance of getting that randomly
  // is vanishing.
  SECStatus rv = PK11_GenerateRandom(
      reinterpret_cast<unsigned char *>(&mSessionId), sizeof(mSessionId));
  if (rv != SECSuccess) {
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  auto origin = MakeUnique<SdpOrigin>();


  //  UniquePtr<Sdp> sdp = MakeUnique<SipccSdp>();

#if 0
  origin.SetUsername("-");
  origin.SetSessionVersion(mSessionVersion);
  origin.SetNetType(sdp::kInternet);
  origin.SetAddrType(sdp::kIPv4);
  origin.SetAddr("0.0.0.0");
#endif

  //  *sdpp = Move(sdp);
  return NS_OK;
}


}  // namespace jsep
}  // namespace mozilla
