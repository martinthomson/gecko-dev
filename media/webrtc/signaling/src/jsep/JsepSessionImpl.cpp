/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"

#include "signaling/src/jsep/JsepSessionImpl.h"
#include <string>

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"


#include <mozilla/Move.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTrackImpl.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/jsep/JsepTransportImpl.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SipccSdp.h"
#include "signaling/src/sdp/SipccSdpParser.h"

namespace mozilla {
namespace jsep {

MOZ_MTLOG_MODULE("jsep")


// Represents a remote track. Only exposed via the abstract interface.
class JsepMediaStreamTrackRemote : public JsepMediaStreamTrack {
 public:
  JsepMediaStreamTrackRemote(mozilla::SdpMediaSection::MediaType type) :
      mType(type) {}

  virtual mozilla::SdpMediaSection::MediaType media_type() const MOZ_OVERRIDE {
    return mType;
  }

 private:
  virtual ~JsepMediaStreamTrackRemote() {}

  mozilla::SdpMediaSection::MediaType mType;
};


// TODO(ekr@rtfm.com): Add state checks. Issue 154

JsepSessionImpl::~JsepSessionImpl() {
  ClearNegotiatedPairs();
  for (auto i = mCodecs.begin(); i != mCodecs.end(); ++i) {
    delete *i;
  }
}

void JsepSessionImpl::Init() {
  SECStatus rv = PK11_GenerateRandom(
      reinterpret_cast<unsigned char *>(&mSessionId), sizeof(mSessionId));
  mSessionId >>= 2; // Discard high order bits.
  if (rv != SECSuccess) {
    MOZ_CRASH();
  }

  SetupDefaultCodecs();
}

nsresult JsepSessionImpl::AddTrack(const RefPtr<JsepMediaStreamTrack>& track) {
  JsepSendingTrack strack;
  strack.mTrack = track;

  mLocalTracks.push_back(strack);

  return NS_OK;
}

nsresult JsepSessionImpl::SetIceCredentials(const std::string& ufrag,
                                            const std::string& pwd) {
  mIceUfrag = ufrag;
  mIcePwd = pwd;

  return NS_OK;
}
nsresult JsepSessionImpl::AddDtlsFingerprint(const std::string& algorithm,
                                             const std::string& value) {
  JsepDtlsFingerprint fp;

  fp.mAlgorithm = algorithm;
  fp.mValue = value;

  mDtlsFingerprints.push_back(fp);

  return NS_OK;
}

nsresult JsepSessionImpl::local_track(
    size_t index,
    RefPtr<JsepMediaStreamTrack>* track) const {
  if (index >= mLocalTracks.size()) {
    return NS_ERROR_INVALID_ARG;
  }

  *track = mLocalTracks[index].mTrack;

  return NS_OK;
}

nsresult JsepSessionImpl::remote_track(
    size_t index,
    RefPtr<JsepMediaStreamTrack>* track) const {
  if (index >= mRemoteTracks.size()) {
    return NS_ERROR_INVALID_ARG;
  }

  *track = mRemoteTracks[index].mTrack;

  return NS_OK;
}

nsresult JsepSessionImpl::CreateOffer(const JsepOfferOptions& options,
                                      std::string* offer) {

  switch (mState) {
    case kJsepStateStable:
      break;
    default:
      mLastError = "Cannot create offer in current state";
      return NS_ERROR_UNEXPECTED;
  }

  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  if (NS_FAILED(rv))
    return rv;

  // Now add all the m-lines that we are attempting to negotiate.
  size_t mline_index = 0;
  size_t nAudio = 0;
  size_t nVideo = 0;

  for (auto track = mLocalTracks.begin();
       track != mLocalTracks.end(); ++track) {
    SdpMediaSection::MediaType mediatype = track->mTrack->media_type();
    SdpDirectionAttribute::Direction dir = SdpDirectionAttribute::kSendrecv;
    if (mediatype == SdpMediaSection::kAudio) {
      ++nAudio;
      if (options.mOfferToReceiveAudio.isSome() &&
          nAudio > *options.mOfferToReceiveAudio) {
        dir = SdpDirectionAttribute::kSendonly;
      }
    } else if (mediatype == SdpMediaSection::kVideo) {
      ++nVideo;
      if (options.mOfferToReceiveVideo.isSome() &&
          nVideo > *options.mOfferToReceiveVideo) {
        dir = SdpDirectionAttribute::kSendonly;
      }
    }

    SdpMediaSection& msection = sdp->AddMediaSection(mediatype, dir);
    rv = AddTransportAttributes(&msection, kJsepSdpOffer,
                                SdpSetupAttribute::kActpass);
    if (NS_FAILED(rv))
      return rv;

    // Set RTCP-MUX.
    msection.GetAttributeList().SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));

    AddCodecs(mediatype, &msection);

    track->mAssignedMLine = Some(mline_index);
    ++mline_index;
  }

  while (options.mOfferToReceiveAudio.isSome() &&
         nAudio < *options.mOfferToReceiveAudio) {
    SdpMediaSection& msection = sdp->AddMediaSection(
        SdpMediaSection::kAudio, SdpDirectionAttribute::kRecvonly);
    AddCodecs(SdpMediaSection::kAudio, &msection);
    rv = AddTransportAttributes(&msection, kJsepSdpOffer,
                                SdpSetupAttribute::kActpass);
    if (NS_FAILED(rv))
      return rv;
    ++nAudio;
  }
  while (options.mOfferToReceiveVideo.isSome() && nVideo <
         *options.mOfferToReceiveVideo) {
    SdpMediaSection& msection = sdp->AddMediaSection(
        SdpMediaSection::kVideo, SdpDirectionAttribute::kRecvonly);
    AddCodecs(SdpMediaSection::kVideo, &msection);
    rv = AddTransportAttributes(&msection, kJsepSdpOffer,
                                SdpSetupAttribute::kActpass);
    if (NS_FAILED(rv))
      return rv;
    ++nVideo;
  }

  // TODO(ekr@rtfm.com): Do renegotiation. Issue 155.
  *offer = sdp->toString();
  mGeneratedLocalDescription = Move(sdp);

  return NS_OK;
}

void JsepSessionImpl::AddCodecs(SdpMediaSection::MediaType mediatype,
                                SdpMediaSection* msection) {
  for (auto c = mCodecs.begin(); c != mCodecs.end(); ++c) {
    (*c)->AddToMediaSection(*msection);
  }
}

JsepCodecDescription* JsepSessionImpl::FindMatchingCodec(
    const SdpRtpmapAttributeList::Rtpmap& entry,
    const SdpMediaSection& msection) const {
  for (auto c = mCodecs.begin(); c != mCodecs.end(); ++c) {
    auto codec = *c;
    if (codec->mEnabled && codec->Matches(entry, msection)) {
      return codec;
    }
  }

  return nullptr;
}

void JsepSessionImpl::AddCommonCodecs(const SdpMediaSection& remote_section,
                                      SdpMediaSection* msection) {
  const std::vector<std::string>& formats = remote_section.GetFormats();
  const SdpRtpmapAttributeList& rtpmap = remote_section.
      GetAttributeList().GetRtpmap();

  const SdpFmtpAttributeList* fmtps = nullptr;
  if (remote_section.GetAttributeList().HasAttribute(SdpAttribute::kFmtpAttribute)) {
    fmtps = &remote_section.GetAttributeList().GetFmtp();
  }

  for (auto fmt = formats.begin(); fmt != formats.end(); ++fmt) {
    if (!rtpmap.HasEntry(*fmt)) {
      continue;
    }

    const SdpRtpmapAttributeList::Rtpmap& entry = rtpmap.GetEntry(*fmt);
    JsepCodecDescription* codec = FindMatchingCodec(entry, remote_section);
    if (codec) {
      codec->mDefaultPt = entry.pt; // Reflect the other side's PT
      codec->AddToMediaSection(*msection);
    }
  }
}


nsresult JsepSessionImpl::CreateAnswer(const JsepAnswerOptions& options,
                                       std::string* answer) {

  switch (mState) {
    case kJsepStateHaveRemoteOffer:
      break;
    default:
      mLastError = "Cannot create answer in current state";
      return NS_ERROR_UNEXPECTED;
  }

  // This is the heart of the negotiation code. Depressing that it's
  // so bad.
  //
  // Here's the current algorithm:
  // 1. Walk through all the m-lines on the other side.
  // 2. For each m-line, walk through all of our local tracks
  //    in sequence and see if any are unassigned. If so, assign
  //    them and mark it sendrecv, otherwise it's recvonly.
  // 3. Just replicate their media attributes.
  // 4. Profit.
  UniquePtr<Sdp> sdp;

  // Make the basic SDP that is common to offer/answer.
  nsresult rv = CreateGenericSDP(&sdp);
  if (NS_FAILED(rv))
    return rv;

  const Sdp& offer = *mPendingRemoteDescription;

  size_t num_m_lines = offer.GetMediaSectionCount();

  for (size_t i = 0; i < num_m_lines; ++i) {
    const SdpMediaSection& remote_msection = offer.GetMediaSection(i);
    // TODO(ekr@rtfm.com): Reflect protocol value. Issue 157.
    SdpMediaSection& msection =
      sdp->AddMediaSection(remote_msection.GetMediaType());

    rv = CreateAnswerMSection(
        options, i, remote_msection, &msection, sdp.get());
    if (NS_FAILED(rv))
      return rv;
  }

  *answer = sdp->toString();
  mGeneratedLocalDescription = Move(sdp);

  return NS_OK;
}

nsresult JsepSessionImpl::CreateAnswerMSection(const JsepAnswerOptions& options,
                                               size_t mline_index,
                                               const SdpMediaSection&
                                               remote_msection,
                                               SdpMediaSection* msection,
                                               Sdp* sdp) {
  SdpSetupAttribute::Role role;
  nsresult rv = DetermineAnswererSetupRole(remote_msection, &role);
  if (NS_FAILED(rv))
    return rv;

  rv = AddTransportAttributes(msection, kJsepSdpAnswer, role);
  if (NS_FAILED(rv))
    return rv;

  bool matched = false;

  for (auto track = mLocalTracks.begin();
       track != mLocalTracks.end(); ++track) {
    if (track->mAssignedMLine.isSome())
      continue;
    if (track->mTrack->media_type() != remote_msection.GetMediaType())
      continue;

    matched = true;
    track->mAssignedMLine = Some(mline_index);
    break;
  }

  // If we matched, then it's sendrecv, else recvonly. No way to
  // do sendonly here. inactive would be used if we had a codec
  // mismatch, but we don't have that worked out yet.
  msection->GetAttributeList().SetAttribute(
      new SdpDirectionAttribute(matched ?
                                SdpDirectionAttribute::kSendrecv :
                                SdpDirectionAttribute::kRecvonly));

  if (remote_msection.GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpMuxAttribute)) {
    msection->GetAttributeList().SetAttribute(
        new SdpFlagAttribute(SdpAttribute::kRtcpMuxAttribute));
  }

  // Now add the codecs.
  // TODO(ekr@rtfm.com): Detect mismatch and mark things inactive. Issue 158.
  AddCommonCodecs(remote_msection, msection);

  return NS_OK;
}

nsresult JsepSessionImpl::DetermineAnswererSetupRole(const SdpMediaSection&
                                                     remote_msection,
                                                     SdpSetupAttribute::Role*
                                                     rolep) {
  // Determine the role.
  // RFC 5763 says:
  //
  //   The endpoint MUST use the setup attribute defined in [RFC4145].
  //   The endpoint that is the offerer MUST use the setup attribute
  //   value of setup:actpass and be prepared to receive a client_hello
  //   before it receives the answer.  The answerer MUST use either a
  //   setup attribute value of setup:active or setup:passive.  Note that
  //   if the answerer uses setup:passive, then the DTLS handshake will
  //   not begin until the answerer is received, which adds additional
  //   latency. setup:active allows the answer and the DTLS handshake to
  //   occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
  //   party is active MUST initiate a DTLS handshake by sending a
  //   ClientHello over each flow (host/port quartet).
  //
  //   We default to assuming that the offerer is passive and we are active.
  SdpSetupAttribute::Role role = SdpSetupAttribute::kActive;

  if (remote_msection.GetAttributeList().HasAttribute(
          SdpAttribute::kSetupAttribute)) {
    switch (remote_msection.GetAttributeList().GetSetup().mRole) {
      case SdpSetupAttribute::kActive:
        role = SdpSetupAttribute::kPassive;
        break;
      case SdpSetupAttribute::kPassive:
      case SdpSetupAttribute::kActpass:
        role = SdpSetupAttribute::kActive;
        break;
      case SdpSetupAttribute::kHoldconn:
        MOZ_MTLOG(ML_ERROR,
                  "The other side used an illegal setup attribute"
                  "(\"holdconn\").");
        return NS_ERROR_FAILURE;
    }
  }

  *rolep = role;
  return NS_OK;
}

static void appendSdpParseErrors(
    const std::vector<std::pair<size_t, std::string> >& aErrors,
    std::string* aErrorString) {
  std::ostringstream os;
  for (auto i = aErrors.begin(); i != aErrors.end(); ++i) {
    // Use endls here?
    os << " | SDP Parsing Error at line " << i->first << ": " + i->second;
  }
  *aErrorString += os.str();
}

nsresult JsepSessionImpl::SetLocalDescription(JsepSdpType type,
                                              const std::string& sdp) {
  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        mLastError = "Cannot set local answer in current state";
        return NS_ERROR_UNEXPECTED;
      }
      break;
    case kJsepStateHaveRemoteOffer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        mLastError = "Cannot set local offer in current state";
        return NS_ERROR_UNEXPECTED;
      }
      break;
    default:
      mLastError = "Cannot set local offer or answer in current state";
      return NS_ERROR_UNEXPECTED;
  }

  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_ERROR_FAILURE;

  // Create transport objects.
  size_t  num_msections = parsed->GetMediaSectionCount();
  for (size_t t = 0; t < num_msections; ++t) {
    if (t < mTransports.size())
      continue; // This transport already exists (assume we are renegotiating).

    // TODO(ekr@rtfm.com): Deal with bundle-only and the like. Issue 159.
    RefPtr<JsepTransport> transport;
    nsresult rv = CreateTransport(parsed->GetMediaSection(t), &transport);
    if (NS_FAILED(rv)) {
      mLastError = "Failed to create transport";
      return rv;
    }

    mTransports.push_back(transport);
  }

  // TODO(ekr@rtfm.com): Compare the generated offer to the passed
  // in argument. Issue 160.

  switch (type ) {
    case kJsepSdpOffer:
      rv = SetLocalDescriptionOffer(Move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetLocalDescriptionAnswer(type, Move(parsed));
      break;
  }

  return rv;
}


nsresult JsepSessionImpl::SetLocalDescriptionOffer(UniquePtr<Sdp> offer) {
  mPendingLocalDescription = Move(offer);
  SetState(kJsepStateHaveLocalOffer);
  return NS_OK;
}

nsresult JsepSessionImpl::SetLocalDescriptionAnswer(JsepSdpType type,
                                                    UniquePtr<Sdp> answer) {
  mPendingLocalDescription = Move(answer);

  nsresult rv = HandleNegotiatedSession(mPendingLocalDescription,
                                        mPendingRemoteDescription,
                                        false);
  if(NS_FAILED(rv))
    return rv;

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);

  SetState(kJsepStateStable);
  return NS_OK;
}

nsresult JsepSessionImpl::SetRemoteDescription(JsepSdpType type,
                                               const std::string& sdp) {

  MOZ_MTLOG(ML_DEBUG, "SetRemoteDescription type="
            << type
            << "\nSDP=\n" << sdp);
  switch (mState) {
    case kJsepStateStable:
      if (type != kJsepSdpOffer) {
        mLastError = "Cannot set remote answer in current state";
        return NS_ERROR_UNEXPECTED;
      }
      break;
    case kJsepStateHaveLocalOffer:
    case kJsepStateHaveRemotePranswer:
      if (type != kJsepSdpAnswer && type != kJsepSdpPranswer) {
        mLastError = "Cannot set remote offer in current state";
        return NS_ERROR_UNEXPECTED;
      }
      break;
    default:
      mLastError = "Cannot set remote offer or answer in current state";
      return NS_ERROR_UNEXPECTED;
  }

  // Parse.
  UniquePtr<Sdp> parsed;
  nsresult rv = ParseSdp(sdp, &parsed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_ERROR_FAILURE;

  // TODO: What additional validation should we do here? Issue 161.
  switch (type) {
    case kJsepSdpOffer:
      rv = SetRemoteDescriptionOffer(Move(parsed));
      break;
    case kJsepSdpAnswer:
    case kJsepSdpPranswer:
      rv = SetRemoteDescriptionAnswer(type, Move(parsed));
      break;
  }

  return rv;
}

nsresult JsepSessionImpl::HandleNegotiatedSession(const UniquePtr<Sdp>& local,
                                                  const UniquePtr<Sdp>& remote,
                                                  bool is_offerer) {
  bool remote_ice_lite = remote->GetAttributeList().HasAttribute(
      SdpAttribute::kIceLiteAttribute);

  mIceControlling = remote_ice_lite || is_offerer;

  if (local->GetMediaSectionCount() != remote->GetMediaSectionCount()) {
    MOZ_MTLOG(ML_ERROR, "Answer and offer have different number of m-lines");
    mLastError = "Answer and offer have different number of m-lines";
    return NS_ERROR_INVALID_ARG;
  }


  std::vector<JsepTrackPair*> track_pairs;

  // Now walk through the m-sections, make sure they match, and create
  // track pairs that describe the media to be set up.
  for (size_t i = 0; i < local->GetMediaSectionCount(); ++i) {
    const SdpMediaSection& lm = local->GetMediaSection(i);
    const SdpMediaSection& rm = remote->GetMediaSection(i);
    const SdpMediaSection& offer = is_offerer ? lm : rm;
    const SdpMediaSection& answer = is_offerer ? rm : lm;


    if (lm.GetMediaType() != rm.GetMediaType()) {
      MOZ_MTLOG(ML_ERROR,
                "Answer and offer have different number of types at m-line "
                << i);
      mLastError = "Answer and offer have different types at m-line";
      return NS_ERROR_INVALID_ARG;
    }

    RefPtr<JsepTransport> transport;
    nsresult rv;

    // Transports are created in SetLocal.
    MOZ_ASSERT(mTransports.size() > i);
    if (mTransports.size() < i) {
      mLastError = "Fewer transports set up than m-lines";
      return NS_ERROR_FAILURE;
    }
    transport = mTransports[i];

    // If the answer says it's inactive we're not doing anything with it.
    // TODO(ekr@rtfm.com): Need to handle renegotiation somehow. Issue 155.
    if (answer.GetDirectionAttribute().mValue ==
        SdpDirectionAttribute::kInactive) {
      transport->mState = JsepTransport::kJsepTransportClosed;
      continue;
    }

    bool sending;
    bool receiving;

    rv = DetermineSendingDirection(offer.GetDirectionAttribute().mValue,
                                   answer.GetDirectionAttribute().mValue,
                                   is_offerer, &sending, &receiving);
    if (NS_FAILED(rv))
      return rv;

    MOZ_MTLOG(ML_DEBUG, "Negotiated m= line"
              << " index=" << i
              << " type=" << lm.GetMediaType()
              << " sending=" << sending
              << " receiving=" << receiving);

    UniquePtr<JsepTrackPair> jpair = MakeUnique<JsepTrackPair>();

    // TODO(ekr@rtfm.com): Set the bundle level. Issue 159.
    jpair->mLevel = i;

    if (sending) {
      rv = CreateTrack(rm, JsepTrack::kJsepTrackSending,
                       &jpair->mSending);
      if (NS_FAILED(rv))
        return rv;
    }
    if (receiving) {
      rv = CreateTrack(rm, JsepTrack::kJsepTrackReceiving,
                       &jpair->mReceiving);
      if (NS_FAILED(rv))
        return rv;
    }

    rv = SetupTransport(rm.GetAttributeList(),
                        answer.GetAttributeList(),
                        is_offerer,
                        transport);
    if (NS_FAILED(rv))
      return rv;
    jpair->mRtpTransport = transport;

    // RTCP MUX or not.
    // TODO(ekr@rtfm.com): verify that the PTs are consistent with mux.
    // Issue 162.
    if (offer.GetAttributeList().HasAttribute(
            SdpAttribute::kRtcpMuxAttribute) &&
        answer.GetAttributeList().HasAttribute(
            SdpAttribute::kRtcpMuxAttribute)) {
      jpair->mRtcpTransport = nullptr;  // We agree on mux.
      MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is on");
    } else {
      MOZ_MTLOG(ML_DEBUG, "RTCP-MUX is off");
      rv = SetupTransport(rm.GetAttributeList(),
                          answer.GetAttributeList(),
                          is_offerer,
                          transport);
      if (NS_FAILED(rv))
        return rv;
      jpair->mRtcpTransport = transport;
    }

    track_pairs.push_back(jpair.release());
  }

  // Ouch, this probably needs some dirty bit instead of just clearing
  // stuff for renegotiation.
  ClearNegotiatedPairs();
  mNegotiatedTrackPairs = track_pairs;
  return NS_OK;
}

nsresult JsepSessionImpl::CreateTrack(const SdpMediaSection& remote_msection,
                                      JsepTrack::Direction direction,
                                      UniquePtr<JsepTrack>* trackp) {
  UniquePtr<JsepTrackImpl> track = MakeUnique<JsepTrackImpl>();
  track->mDirection = direction;
  track->mMediaType = remote_msection.GetMediaType();
  track->mProtocol = remote_msection.GetProtocol();

  // Insert all the codecs we jointly support.
  const std::vector<std::string>& formats = remote_msection.GetFormats();
  const SdpRtpmapAttributeList& rtpmap = remote_msection.
       GetAttributeList().GetRtpmap();

  for (auto fmt = formats.begin(); fmt != formats.end(); ++fmt) {
    if (!rtpmap.HasEntry(*fmt)) {
      continue;
    }

    const SdpRtpmapAttributeList::Rtpmap& entry = rtpmap.GetEntry(*fmt);
    JsepCodecDescription* codec = FindMatchingCodec(entry, remote_msection);
    if (codec) {
      bool sending = (direction == JsepTrack::kJsepTrackSending);
      // We need to take the remote side's parameters into account so we can
      // configure our send media.
      // |codec| is assumed to have the necessary state about our own config
      // in order to negotiate.
      JsepCodecDescription* negotiated = codec->MakeNegotiatedCodec(
          remote_msection,
          entry,
          sending);

      if (!negotiated) {
        // TODO: What should we do here? Issue 158.
        continue;
      }

      track->mCodecs.push_back(negotiated);
    }
  }

  *trackp = Move(track);
  return NS_OK;
}

nsresult JsepSessionImpl::CreateTransport(const SdpMediaSection& msection,
                                          RefPtr<JsepTransport>* transport) {
  size_t components;

  switch (msection.GetMediaType()) {
    case SdpMediaSection::kAudio:
    case SdpMediaSection::kVideo:
      components = 2;
      break;
    case SdpMediaSection::kApplication:
      components = 1;
      break;
    default:
      MOZ_CRASH(); // This shouldn't happen.
  }

  RefPtr<JsepTransport> trans = new JsepTransport("transport-id",
                                                  components);
  *transport = trans;

  return NS_OK;
}

nsresult JsepSessionImpl::SetupTransport(const SdpAttributeList& remote,
                                         const SdpAttributeList& answer,
                                         bool is_offerer,
                                         const RefPtr<JsepTransport>&
                                         transport) {
  UniquePtr<JsepIceTransportImpl> ice = MakeUnique<JsepIceTransportImpl>();

  // TODO(ekr@rtfm.com): ICE lite, ICE trickle. Issue 163.
  // We do sanity-checking for these in ParseSdp
  ice->mUfrag = remote.GetIceUfrag();
  ice->mPwd = remote.GetIcePwd();
  if (remote.HasAttribute(SdpAttribute::kCandidateAttribute, true)) {
    ice->mCandidates = remote.GetCandidate();
  }

 // RFC 5763 says:
 //
 //   The endpoint MUST use the setup attribute defined in [RFC4145].
 //   The endpoint that is the offerer MUST use the setup attribute
 //   value of setup:actpass and be prepared to receive a client_hello
 //   before it receives the answer.  The answerer MUST use either a
 //   setup attribute value of setup:active or setup:passive.  Note that
 //   if the answerer uses setup:passive, then the DTLS handshake will
 //   not begin until the answerer is received, which adds additional
 //   latency. setup:active allows the answer and the DTLS handshake to
 //   occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
 //   party is active MUST initiate a DTLS handshake by sending a
 //   ClientHello over each flow (host/port quartet).
  UniquePtr<JsepDtlsTransportImpl> dtls = MakeUnique<JsepDtlsTransportImpl>();
  dtls->mFingerprints = remote.GetFingerprint();
  if (!answer.HasAttribute(mozilla::SdpAttribute::kSetupAttribute)) {
    dtls->mRole = is_offerer ?
        JsepDtlsTransport::kJsepDtlsServer :
        JsepDtlsTransport::kJsepDtlsClient;
  } else {
    if (is_offerer) {
      dtls->mRole = (answer.GetSetup().mRole == SdpSetupAttribute::kActive) ?
        JsepDtlsTransport::kJsepDtlsServer :
        JsepDtlsTransport::kJsepDtlsClient;
    } else {
      dtls->mRole = (answer.GetSetup().mRole == SdpSetupAttribute::kActive) ?
        JsepDtlsTransport::kJsepDtlsClient:
        JsepDtlsTransport::kJsepDtlsServer;
    }
  }

  transport->mIce = Move(ice);
  transport->mDtls = Move(dtls);
  transport->mState = JsepTransport::kJsepTransportAccepted;

  return NS_OK;
}

nsresult JsepSessionImpl::DetermineSendingDirection(
    SdpDirectionAttribute::Direction offer,
    SdpDirectionAttribute::Direction answer,
    bool is_offerer,
    bool* sending, bool* receiving) {
  if (answer == SdpDirectionAttribute::kSendrecv) {
    if (offer != SdpDirectionAttribute::kSendrecv) {
      MOZ_MTLOG(ML_ERROR,
                "Answer tried to change m-line to sendrecv");
      mLastError = "Answer tried to change m-line to sendrecv";
      return NS_ERROR_INVALID_ARG;
    }

    *sending = true;
    *receiving = true;
  } else if (answer ==
             SdpDirectionAttribute::kRecvonly) {
    if ((offer != SdpDirectionAttribute::kSendrecv) &&
        (offer != SdpDirectionAttribute::kSendonly)) {
      MOZ_MTLOG(ML_ERROR,
                "Answer tried to change m-line to recvonly");
      mLastError = "Answer tried to change m-line to recvonly";
      return NS_ERROR_INVALID_ARG;
    }
    if (is_offerer) {
      *sending = true; *receiving = false;
    } else {
      *sending = false; *receiving = true;
    }
  } else if (answer ==
             SdpDirectionAttribute::kSendonly) {
    if ((offer != SdpDirectionAttribute::kSendrecv) &&
        (offer != SdpDirectionAttribute::kRecvonly)) {
      MOZ_MTLOG(ML_ERROR,
                "Answer tried to change m-line to sendonly");
      mLastError = "Answer tried to change m-line to sendonly";
      return NS_ERROR_INVALID_ARG;
    }
    if (is_offerer) {
      *sending = false; *receiving = true;
    } else {
      *sending = true; *receiving = false;
    }

  } else {
    MOZ_CRASH(); // Can't happen.
  }

  return NS_OK;
}

nsresult JsepSessionImpl::AddTransportAttributes(SdpMediaSection* msection,
                                                 JsepSdpType type,
                                                 SdpSetupAttribute::Role
                                                 dtls_role) {
  if (mIceUfrag.empty() || mIcePwd.empty()) {
    MOZ_MTLOG(ML_ERROR, "Missing ICE ufrag or password");
    mLastError = "Missing ICE ufrag or password";
    return NS_ERROR_FAILURE;
  }

  if (mDtlsFingerprints.empty()) {
    MOZ_MTLOG(ML_ERROR, "Missing DTLS fingerprint");
    mLastError = "Missing DTLS fingerprint";
    return NS_ERROR_FAILURE;
  }

  msection->GetAttributeList().SetAttribute(new SdpStringAttribute(
      SdpAttribute::kIceUfragAttribute, mIceUfrag));
  msection->GetAttributeList().SetAttribute(new SdpStringAttribute(
      SdpAttribute::kIcePwdAttribute, mIcePwd));


  UniquePtr<SdpFingerprintAttributeList> fpl =
      MakeUnique<SdpFingerprintAttributeList>();
  for (auto fp = mDtlsFingerprints.begin();
       fp != mDtlsFingerprints.end(); ++fp) {
    fpl->PushEntry(fp->mAlgorithm, fp->mValue);
  }
  msection->GetAttributeList().SetAttribute(fpl.release());

  msection->GetAttributeList().SetAttribute(new SdpSetupAttribute(dtls_role));

  return NS_OK;
}

nsresult JsepSessionImpl::ParseSdp(const std::string& sdp,
                                   UniquePtr<Sdp>* parsedp) {
  UniquePtr<Sdp> parsed = mParser.Parse(sdp);
  if (!parsed) {
    MOZ_MTLOG(ML_ERROR, "Bogus SDP = " + sdp);
    const std::vector<std::pair<size_t, std::string>>& errors =
        mParser.GetParseErrors();
    for (auto err = errors.begin(); err != errors.end(); ++err) {
      MOZ_MTLOG(ML_ERROR, " Error at "
                << err->first
                << ": "
                << err->second);
    }
    mLastError = "Failed to parse SDP: ";
    appendSdpParseErrors(mParser.GetParseErrors(), &mLastError);
    return NS_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < parsed->GetMediaSectionCount(); ++i) {
    if (parsed->GetMediaSection(i).GetAttributeList().GetIceUfrag().empty()) {
      mLastError = "Invalid description, no ice-ufrag attribute";
      return NS_ERROR_INVALID_ARG;
    }

    if (parsed->GetMediaSection(i).GetAttributeList().GetIcePwd().empty()) {
      mLastError = "Invalid description, no ice-pwd attribute";
      return NS_ERROR_INVALID_ARG;
    }

    if (!parsed->GetMediaSection(i).GetAttributeList().HasAttribute(
          SdpAttribute::kFingerprintAttribute)) {
      mLastError = "Invalid description, no fingerprint attribute";
      return NS_ERROR_INVALID_ARG;
    }
  }

  *parsedp = Move(parsed);
  return NS_OK;
}

nsresult JsepSessionImpl::SetRemoteDescriptionOffer(UniquePtr<Sdp> offer) {
  // TODO(ekr@rtfm.com): Check state. Issue 154.
  size_t num_m_lines = offer->GetMediaSectionCount();

  for (size_t i = 0; i < num_m_lines; ++i) {
    const SdpMediaSection& msection = offer->GetMediaSection(i);
    JsepMediaStreamTrackRemote* remote = new JsepMediaStreamTrackRemote(
        msection.GetMediaType());
    JsepReceivingTrack rtrack;
    rtrack.mTrack = remote;
    mRemoteTracks.push_back(rtrack);
  }

  mPendingRemoteDescription = Move(offer);

  SetState(kJsepStateHaveRemoteOffer);
  return NS_OK;
}

nsresult JsepSessionImpl::SetRemoteDescriptionAnswer(
    JsepSdpType type, UniquePtr<Sdp> answer) {
  mPendingRemoteDescription = Move(answer);

  nsresult rv = HandleNegotiatedSession(mPendingLocalDescription,
                                        mPendingRemoteDescription,
                                        true);
  if(NS_FAILED(rv))
    return rv;

  mCurrentRemoteDescription = Move(mPendingRemoteDescription);
  mCurrentLocalDescription = Move(mPendingLocalDescription);

  SetState(kJsepStateStable);
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


  auto origin = MakeUnique<SdpOrigin>("-",
                                      mSessionId,
                                      mSessionVersion,
                                      sdp::kIPv4,
                                      "0.0.0.0");

  UniquePtr<Sdp> sdp = MakeUnique<SipccSdp>(origin.release());

  *sdpp = Move(sdp);
  return NS_OK;
}

void JsepSessionImpl::SetupDefaultCodecs() {
  // Supported audio codecs.
  mCodecs.push_back(new JsepAudioCodecDescription(
      "109",
      "opus",
      48000,
      2,
      960,
      16000));

  mCodecs.push_back(new JsepAudioCodecDescription(
      "9",
      "G722",
      8000,
      1,
      320,
      64000));

  mCodecs.push_back(new JsepAudioCodecDescription(
      "0",
      "PCMU",
      8000,
      1
                      ));

  mCodecs.push_back(new JsepAudioCodecDescription(
      "8",
      "PCMA",
      8000,
      1
                      ));

  // Supported video codecs.
  JsepVideoCodecDescription* vp8 = new JsepVideoCodecDescription(
      "120",
      "VP8",
      90000
      );
  // Defaults for mandatory params
  vp8->mMaxFs = 3600;
  vp8->mMaxFr = 30;
  mCodecs.push_back(vp8);

  JsepVideoCodecDescription* h264_1 = new JsepVideoCodecDescription(
      "126",
      "H264",
      90000
                      );
  h264_1->mPacketizationMode = 1;

  // Defaults for mandatory params
  h264_1->mProfileLevelId = 0x42E00D;
  mCodecs.push_back(h264_1);

  JsepVideoCodecDescription* h264_0 = new JsepVideoCodecDescription(
      "97",
      "H264",
      90000
                      );
  h264_0->mPacketizationMode = 0;

  // Defaults for mandatory params
  h264_0->mProfileLevelId = 0x42E00D;
  mCodecs.push_back(h264_0);
}

void JsepSessionImpl::SetState(JsepSignalingState state) {
  if (state == mState)
    return;

  MOZ_MTLOG(ML_NOTICE, "[" << mName << "]: " <<
            state_str(mState) << " -> " << state_str(state));
  mState = state;
}

nsresult
JsepSessionImpl::AddIceCandidate(const std::string& candidate,
                                 const std::string& mid,
                                 uint16_t level) {
  mozilla::Sdp* sdp = 0;

  if (mPendingRemoteDescription) {
    sdp = mPendingRemoteDescription.get();
  } else if (mCurrentRemoteDescription) {
    sdp = mCurrentRemoteDescription.get();
  } else {
    mLastError = "Cannot add ICE candidate in current state";
    return NS_ERROR_UNEXPECTED;
  }

  SdpAttributeList& attr_list =
    sdp->GetMediaSection(level).GetAttributeList();

  SdpMultiStringAttribute *candidates = nullptr;
  if (!attr_list.HasAttribute(SdpAttribute::kCandidateAttribute)) {
    candidates =
      new SdpMultiStringAttribute(SdpAttribute::kCandidateAttribute);
  } else {
    // Copy existing
    candidates = new SdpMultiStringAttribute(
        *static_cast<const SdpMultiStringAttribute*>(
          attr_list.GetAttribute(SdpAttribute::kCandidateAttribute)));
  }
  candidates->PushEntry(candidate);
  attr_list.SetAttribute(candidates);

  return NS_OK;
}

nsresult JsepSessionImpl::Close() {
  SetState(kJsepStateClosed);
  return NS_OK;
}


const std::string
JsepSessionImpl::last_error() const {
  return mLastError;
}

}  // namespace jsep
}  // namespace mozilla
