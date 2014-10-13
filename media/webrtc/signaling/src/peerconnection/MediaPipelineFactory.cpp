/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsIPrincipal.h"
#include "nsIDocument.h"
#include "mozilla/Preferences.h"
#endif

#include "GmpVideoCodec.h"
#ifdef MOZ_WEBRTC_OMX
#include "OMXVideoCodec.h"
#include "OMXCodecWrapper.h"
#endif
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "MediaPipelineFactory.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"

#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTransport.h"

#include <stdlib.h>

namespace mozilla {

MOZ_MTLOG_MODULE("MediaPipelineFactory")

// Trivial wrapper class around a vector of ptrs.
template <class T> class PtrVector {
 public:
  ~PtrVector() {
    for (auto it = values.begin(); it != values.end(); ++it) {
      delete *it;
    }
  }

  std::vector<T*> values;
};

static nsresult JsepCodecDescToCodecConfig(const
                                           jsep::JsepCodecDescription& d,
                                           AudioCodecConfig** config) {
  MOZ_ASSERT(d.mType == mozilla::SdpMediaSection::kAudio);
  if (d.mType != mozilla::SdpMediaSection::kAudio)
    return NS_ERROR_INVALID_ARG;

  const jsep::JsepAudioCodecDescription& desc =
      static_cast<const jsep::JsepAudioCodecDescription&>(d);

  char* end;
  long int pt = strtol(desc.mDefaultPt.c_str(), &end, 10);
  size_t length = static_cast<size_t>(end - desc.mDefaultPt.c_str());
  if ((pt < 0) ||
      (pt > UINT16_MAX) ||
      (length != desc.mDefaultPt.size())) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  mozilla::AudioCodecConfig *config_raw;
  config_raw = new mozilla::AudioCodecConfig(
      pt,
      desc.mName,
      desc.mClock,
      desc.mPacketSize,
      desc.mChannels,
      desc.mBitrate);

  *config = config_raw;
  return NS_OK;
}

static nsresult JsepCodecDescToCodecConfig(const
                                           jsep::JsepCodecDescription& d,
                                           VideoCodecConfig** config) {
  MOZ_ASSERT(d.mType == mozilla::SdpMediaSection::kVideo);
  if (d.mType != mozilla::SdpMediaSection::kVideo) {
    MOZ_ASSERT(false, "JsepCodecDescription has wrong type");
    return NS_ERROR_INVALID_ARG;
  }

  const jsep::JsepVideoCodecDescription& desc =
      static_cast<const jsep::JsepVideoCodecDescription&>(d);

  char* end;
  long int pt = strtol(desc.mDefaultPt.c_str(), &end, 10);
  size_t length = static_cast<size_t>(end - desc.mDefaultPt.c_str());
  if ((pt < 0) ||
      (pt > UINT16_MAX) ||
      (length != desc.mDefaultPt.size())) {
    MOZ_MTLOG(ML_ERROR, "Invalid payload type: " << desc.mDefaultPt);
    return NS_ERROR_INVALID_ARG;
  }

  ScopedDeletePtr<VideoCodecConfigH264> h264_config;

  if (desc.mName == "H264") {
    h264_config = new VideoCodecConfigH264;
    strncpy(h264_config->sprop_parameter_sets,
            desc.mSpropParameterSets.c_str(),
            sizeof(h264_config->sprop_parameter_sets));
    h264_config->packetization_mode = desc.mPacketizationMode;
    h264_config->profile_level_id = desc.mProfileLevelId;
    h264_config->max_mbps = desc.mMaxMbps;
    h264_config->max_fs = desc.mMaxFs;
    h264_config->max_cpb = desc.mMaxCpb;
    h264_config->max_dpb = desc.mMaxDpb;
    h264_config->max_br = desc.mMaxBr;
    h264_config->tias_bw = 0; // TODO. Issue 165.
  }

  mozilla::VideoCodecConfig *config_raw;
  config_raw = new mozilla::VideoCodecConfig(
      pt,
      desc.mName,
      desc.mMaxFs,
      desc.mMaxFr,
      h264_config);

  config_raw->mAckFbTypes = desc.mAckFbTypes;
  config_raw->mNackFbTypes = desc.mNackFbTypes;
  config_raw->mCcmFbTypes = desc.mCcmFbTypes;

  *config = config_raw;
  return NS_OK;
}

nsresult MediaPipelineFactory::CreateOrGetTransportFlow(
    size_t level,
    bool rtcp,
    const RefPtr<mozilla::jsep::JsepTransport>& transport,
    RefPtr<mozilla::TransportFlow>* out) {
  nsresult rv;
  mozilla::RefPtr<TransportFlow> flow;

  flow = mPCMedia->GetTransportFlow(level, rtcp);
  if (flow) {
    *out = flow;
    return NS_OK;
  }

  char id[32];
  PR_snprintf(id, sizeof(id), "%s:%d,%s",
              mPC->GetHandle().c_str(),
              level, rtcp ? "rtcp" : "rtp");
  flow = new TransportFlow(id);

  // The media streams are made on STS so we need to defer setup.
  auto ice = MakeUnique<mozilla::TransportLayerIce>(mPC->GetHandle());
  auto dtls = MakeUnique<mozilla::TransportLayerDtls>();
  dtls->SetRole(transport->mDtls->role() ==
                mozilla::jsep::JsepDtlsTransport::kJsepDtlsClient ?
                TransportLayerDtls::CLIENT : TransportLayerDtls::SERVER);

  mozilla::RefPtr<DtlsIdentity> pcid = mPC->GetIdentity();
  if (!pcid) {
    MOZ_MTLOG(ML_ERROR, "Failed to get DTLS identity.");
    return NS_ERROR_FAILURE;
  }
  dtls->SetIdentity(pcid);

  const SdpFingerprintAttributeList& fingerprints =
    transport->mDtls->fingerprints();
  for (auto fp = fingerprints.mFingerprints.begin();
       fp != fingerprints.mFingerprints.end(); ++fp) {
    std::ostringstream ss;
    ss << fp->hashFunc;
    rv = dtls->SetVerificationDigest(ss.str(), &fp->fingerprint[0],
                                     fp->fingerprint.size());
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Could not set fingerprint");
      return rv;
    }
  }

  std::vector<uint16_t> srtp_ciphers;
  srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
  srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

  rv = dtls->SetSrtpCiphers(srtp_ciphers);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set SRTP ciphers");
    return rv;
  }

  nsAutoPtr<std::queue<TransportLayer *> > layers(new std::queue<TransportLayer *>);
  layers->push(ice.release());
  layers->push(dtls.release());

  rv = mPCMedia->GetSTSThread()->Dispatch(
    WrapRunnableNM(&MediaPipelineFactory::FinalizeTransportFlow_s,
                   mPCMedia, flow, level, rtcp, layers),
    NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Failed to dispatch FinalizeTransportFlow_s");
    return rv;
  }

  mPCMedia->AddTransportFlow(level, rtcp, flow);

  *out = flow;

  return NS_OK;
}

// Accessing the PCMedia should be safe here because we shouldn't
// have enqueued this function unless it was still active and
// the ICE data is destroyed on the STS.
void MediaPipelineFactory::FinalizeTransportFlow_s(
    RefPtr<sipcc::PeerConnectionMedia> media,
    RefPtr<TransportFlow> flow,
    size_t level, bool rtcp,
    nsAutoPtr<std::queue<TransportLayer *> > layers) {
  TransportLayerIce* ice =
      static_cast<TransportLayerIce*>(layers->front());
  ice->SetParameters(media->ice_ctx(),
                     media->ice_media_stream(level),
                     rtcp ? 2 : 1);
  (void)flow->PushLayers(layers);  // TODO(ekr@rtfm.com): Process errors. Issue 164
}

nsresult MediaPipelineFactory::CreateMediaPipeline(
    const mozilla::UniquePtr<mozilla::jsep::JsepSession>& session,
    const mozilla::jsep::JsepTrackPair& trackPair,
    const mozilla::UniquePtr<mozilla::jsep::JsepTrack>& track) {
  MOZ_MTLOG(ML_DEBUG,
            "Creating media pipeline"
            << " m=line index=" << trackPair.mLevel
            << " type=" << track->media_type()
            << " direction="
            << track->direction());

  // First make sure the transport flow exists.
  mozilla::RefPtr<mozilla::TransportFlow> rtp_flow;
  nsresult rv = CreateOrGetTransportFlow(trackPair.mLevel,
                                         false,
                                         trackPair.mRtpTransport,
                                         &rtp_flow);
  if (NS_FAILED(rv))
    return rv;
  MOZ_ASSERT(rtp_flow);

  mozilla::RefPtr<mozilla::TransportFlow> rtcp_flow;
  if (trackPair.mRtcpTransport) {
    rv = CreateOrGetTransportFlow(trackPair.mLevel,
                                  true,
                                  trackPair.mRtcpTransport,
                                  &rtcp_flow);
    if (NS_FAILED(rv))
      return rv;
    MOZ_ASSERT(rtcp_flow);
  }

  bool receiving =
      track->direction() == jsep::JsepTrack::Direction::kJsepTrackReceiving;

  mozilla::RefPtr<mozilla::MediaSessionConduit> conduit;
  if (track->media_type() == mozilla::SdpMediaSection::kAudio) {
    rv = CreateAudioConduit(session, trackPair, track, &conduit);
    if (NS_FAILED(rv))
      return rv;
  } else if (track->media_type() == mozilla::SdpMediaSection::kVideo) {
    rv = CreateVideoConduit(session, trackPair, track, &conduit);
    if (NS_FAILED(rv))
      return rv;
  } else {
    // We've created the TransportFlow, nothing else to do here.
    return NS_OK;
  }

  if (receiving) {
    rv = CreateMediaPipelineReceiving(rtp_flow, rtcp_flow, nullptr, nullptr,
                                      session, trackPair, track, conduit);
    if (NS_FAILED(rv))
      return rv;
  } else {
    rv = CreateMediaPipelineSending(rtp_flow, rtcp_flow, nullptr, nullptr,
                                    session, trackPair, track, conduit);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

nsresult MediaPipelineFactory::CreateMediaPipelineReceiving(
    RefPtr<TransportFlow> rtp_flow,
    RefPtr<TransportFlow> rtcp_flow,
    RefPtr<TransportFlow> bundle_rtp_flow,
    RefPtr<TransportFlow> bundle_rtcp_flow,
    const mozilla::UniquePtr<mozilla::jsep::JsepSession>& session,
    const mozilla::jsep::JsepTrackPair& trackPair,
    const mozilla::UniquePtr<mozilla::jsep::JsepTrack>& track,
    const mozilla::RefPtr<mozilla::MediaSessionConduit>& conduit) {

  // Find the stream we need
  nsRefPtr<sipcc::RemoteSourceStreamInfo> stream =
      mPCMedia->GetRemoteStreamById(track->media_stream_track()->
                                    stream_id());
  MOZ_ASSERT(stream);
  if (!stream) {
    // This should never happen
    return NS_ERROR_FAILURE;
  }

  mozilla::RefPtr<mozilla::MediaPipelineReceive> pipeline;

  // TODO(ekr@rtfm.com): Need bundle filter. Issue 159.
  nsAutoPtr<mozilla::MediaPipelineFilter> filter (nullptr);

  if (track->media_type() == mozilla::SdpMediaSection::kAudio) {
    pipeline = new mozilla::MediaPipelineReceiveAudio(
        mPC->GetHandle(),
        mPC->GetMainThread().get(),
        mPC->GetSTSThread(),
        stream->GetMediaStream()->GetStream(),
        trackPair.mLevel,  // Use the level as the track ID.
        trackPair.mLevel,
        static_cast<AudioSessionConduit*>(conduit.get()),  // Ugly downcast.
        rtp_flow,
        rtcp_flow,
        bundle_rtp_flow,
        bundle_rtcp_flow,
        filter);

  } else if (track->media_type() == mozilla::SdpMediaSection::kVideo) {
    pipeline = new mozilla::MediaPipelineReceiveVideo(
        mPC->GetHandle(),
        mPC->GetMainThread().get(),
        mPC->GetSTSThread(),
        stream->GetMediaStream()->GetStream(),
        trackPair.mLevel,   // Use the level as the track ID.
        trackPair.mLevel,
        static_cast<VideoSessionConduit*>(conduit.get()),  // Ugly downcast.
        rtp_flow,
        rtcp_flow,
        bundle_rtp_flow,
        bundle_rtcp_flow,
        filter);
  } else {
    MOZ_CRASH();
    return NS_ERROR_FAILURE;
  }

  nsresult rv = pipeline->Init();
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't initialize receiving pipeline");
    return rv;
  }

  stream->StorePipeline(trackPair.mLevel, false, pipeline);
  return NS_OK;
}

nsresult MediaPipelineFactory::CreateMediaPipelineSending(
    RefPtr<TransportFlow> rtp_flow,
    RefPtr<TransportFlow> rtcp_flow,
    RefPtr<TransportFlow> bundle_rtp_flow,
    RefPtr<TransportFlow> bundle_rtcp_flow,
    const mozilla::UniquePtr<mozilla::jsep::JsepSession>& session,
    const mozilla::jsep::JsepTrackPair& trackPair,
    const mozilla::UniquePtr<mozilla::jsep::JsepTrack>& track,
    const mozilla::RefPtr<mozilla::MediaSessionConduit>& conduit) {
  nsresult rv;

  nsRefPtr<sipcc::LocalSourceStreamInfo> stream =
      mPCMedia->GetLocalStreamById(track->media_stream_track()->
                                   stream_id());
  MOZ_ASSERT(stream);
  if (!stream) {
    MOZ_MTLOG(ML_ERROR, "Stream not found: "
              << track->media_stream_track()->stream_id());
    return NS_ERROR_FAILURE;
  }

  // Now we have all the pieces, create the pipeline
  mozilla::RefPtr<mozilla::MediaPipelineTransmit> pipeline =
      new mozilla::MediaPipelineTransmit(
          mPC->GetHandle(),
          mPC->GetMainThread().get(),
          mPC->GetSTSThread(),
          stream->GetMediaStream(),
          -1,   // TODO(ekr@rtfm.com): Ignored.
          trackPair.mLevel,
          track->media_type() == SdpMediaSection::kVideo,
          conduit, rtp_flow, rtcp_flow);

#ifdef MOZILLA_INTERNAL_API
  // implement checking for peerIdentity (where failure == black/silence)
  nsIDocument* doc = mPC->GetWindow()->GetExtantDoc();
  if (doc) {
    pipeline->UpdateSinkIdentity_m(doc->NodePrincipal(), mPC->GetPeerIdentity());
  } else {
    MOZ_MTLOG(ML_ERROR, "Initializing pipeline without attached doc");
    MOZ_CRASH();  // Don't remove this till we know it's safe.
  }
#endif

  rv = pipeline->Init();
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't initialize sending pipeline");
    return rv;
  }

 // TODO(ekr@rtfm.com): Copied from vcmSIPCCBinding. Need to unifdef and port in.
 // Issue 159.
#if 0
  // This tells the receive MediaPipeline (if there is one) whether we are
  // doing bundle, and if so, updates the filter. Once the filter is finalized,
  // it is then copied to the transmit pipeline so it can filter RTCP.
  if (attrs->bundle_level) {
    nsAutoPtr<mozilla::MediaPipelineFilter> filter (new MediaPipelineFilter);
    for (int s = 0; s < attrs->num_ssrcs; ++s) {
      filter->AddRemoteSSRC(attrs->ssrcs[s]);
    }
    pc.impl()->media()->SetUsingBundle_m(level, true);
    pc.impl()->media()->UpdateFilterFromRemoteDescription_m(level, filter);
  } else {
    // This will also clear the filter.
    pc.impl()->media()->SetUsingBundle_m(level, false);
  }
#endif

  stream->StorePipeline(trackPair.mLevel, pipeline);

  return NS_OK;
}


nsresult MediaPipelineFactory::CreateAudioConduit(
    const mozilla::UniquePtr<mozilla::jsep::JsepSession>& session,
    const mozilla::jsep::JsepTrackPair& trackPair,
    const mozilla::UniquePtr<mozilla::jsep::JsepTrack>& track,
    mozilla::RefPtr<mozilla::MediaSessionConduit>* conduitp) {
  bool receiving =
      track->direction() == jsep::JsepTrack::Direction::kJsepTrackReceiving;

  mozilla::RefPtr<mozilla::MediaSessionConduit> other_conduit =
      mPCMedia->GetConduit(trackPair.mLevel, receiving);
  MOZ_ASSERT_IF(other_conduit, other_conduit->type() == MediaSessionConduit::AUDIO);
  // The two sides of a send/receive pair of conduits each keep a raw pointer
  // to the other, and are responsible for cleanly shutting down.
  mozilla::RefPtr<mozilla::AudioSessionConduit> conduit =
      mozilla::AudioSessionConduit::Create(
          static_cast<AudioSessionConduit *>(other_conduit.get()));

  if (!conduit) {
    MOZ_MTLOG(ML_ERROR, "Could not create audio conduit");
    return NS_ERROR_FAILURE;
  }

  mPCMedia->AddConduit(trackPair.mLevel, receiving, conduit);
  size_t num_codecs = track->num_codecs();
  if (num_codecs == 0) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  if (receiving) {
    PtrVector<mozilla::AudioCodecConfig> configs;

    for (size_t i=0; i <num_codecs; i++) {
      const jsep::JsepCodecDescription* cdesc;
      nsresult rv = track->get_codec(i, &cdesc);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_FAILED(rv)) {
        MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track");
        return rv;
      }

      mozilla::AudioCodecConfig *config_raw;
      rv = JsepCodecDescToCodecConfig(*cdesc, &config_raw);
      if (NS_FAILED(rv))
        return rv;

      configs.values.push_back(config_raw);
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }
  } else {
    const jsep::JsepCodecDescription* cdesc;

    nsresult rv = track->get_codec(0, &cdesc);  // Best codec.
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track");
      return rv;
    }

    mozilla::AudioCodecConfig *config_raw;
    rv = JsepCodecDescToCodecConfig(*cdesc, &config_raw);
    if (NS_FAILED(rv))
      return rv;

    UniquePtr<mozilla::AudioCodecConfig> config(config_raw);
    auto error = conduit->ConfigureSendMediaCodec(config.get());
    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }


    // TODO(ekr@rtfm.com): Audio level extension. Issue 169.
  }

  *conduitp = conduit;

  return NS_OK;
}

nsresult MediaPipelineFactory::CreateVideoConduit(
    const mozilla::UniquePtr<mozilla::jsep::JsepSession>& session,
    const mozilla::jsep::JsepTrackPair& trackPair,
    const mozilla::UniquePtr<mozilla::jsep::JsepTrack>& track,
    mozilla::RefPtr<mozilla::MediaSessionConduit>* conduitp) {
  bool receiving =
      track->direction() == jsep::JsepTrack::Direction::kJsepTrackReceiving;

  // Instantiate an appropriate conduit
  mozilla::RefPtr<mozilla::MediaSessionConduit> peer_conduit =
      mPCMedia->GetConduit(trackPair.mLevel, true);
  MOZ_ASSERT_IF(peer_conduit, peer_conduit->type() ==
                MediaSessionConduit::VIDEO);

  // The two sides of a send/receive pair of conduits each keep a raw
  // pointer to the other, and are responsible for cleanly shutting down.
  mozilla::RefPtr<mozilla::VideoSessionConduit> conduit =
    mozilla::VideoSessionConduit::Create(static_cast<VideoSessionConduit *>(
        peer_conduit.get()));

  if (!conduit) {
    MOZ_MTLOG(ML_ERROR, "Could not create video conduit");
    return NS_ERROR_FAILURE;
  }

  mPCMedia->AddConduit(trackPair.mLevel, receiving, conduit);

  size_t num_codecs = track->num_codecs();
  if (num_codecs == 0) {
    MOZ_MTLOG(ML_ERROR, "Can't set up a conduit with 0 codecs");
    return NS_ERROR_FAILURE;
  }

  if (receiving) {
    PtrVector<mozilla::VideoCodecConfig> configs;

    for (size_t i=0; i <num_codecs ; i++) {
      const jsep::JsepCodecDescription* cdesc;

      nsresult rv = track->get_codec(i, &cdesc);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_FAILED(rv)) {
        MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track");
        return rv;
      }

      mozilla::VideoCodecConfig *config_raw;
      rv = JsepCodecDescToCodecConfig(*cdesc, &config_raw);
      if (NS_FAILED(rv))
        return rv;

      if (EnsureExternalCodec(conduit, config_raw, false)) {
        delete config_raw;
        continue;
      }

      configs.values.push_back(config_raw);
    }

    auto error = conduit->ConfigureRecvMediaCodecs(configs.values);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureRecvMediaCodecs failed: " << error);
      return NS_ERROR_FAILURE;
    }
  } else {
 // TODO(ekr@rtfm.com): Copied from vcmSIPCCBinding. Need to unifdef and port in.
#if 0
    struct VideoCodecConfigH264 *negotiated = nullptr;

    if (attrs->video.opaque &&
        (payload->codec_type == RTP_H264_P0 || payload->codec_type == RTP_H264_P1)) {
      negotiated = static_cast<struct VideoCodecConfigH264 *>(attrs->video.opaque);
    }
#endif
    const jsep::JsepCodecDescription* cdesc;

    nsresult rv = track->get_codec(0, &cdesc);   // Best codec.
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR, "Failed to get codec from jsep track");
      return rv;
    }

    mozilla::VideoCodecConfig *config_raw;
    rv = JsepCodecDescToCodecConfig(*cdesc, &config_raw);
    if (NS_FAILED(rv))
      return rv;

    // Take possession of this pointer
    mozilla::ScopedDeletePtr<mozilla::VideoCodecConfig> config(config_raw);

    if (EnsureExternalCodec(conduit, config, true)) {
      MOZ_MTLOG(ML_ERROR, "External codec not available");
      return NS_ERROR_FAILURE;
    }

    auto error = conduit->ConfigureSendMediaCodec(config);

    if (error) {
      MOZ_MTLOG(ML_ERROR, "ConfigureSendMediaCodec failed: " << error);
      return NS_ERROR_FAILURE;
    }
  }

  *conduitp = conduit;

  return NS_OK;
}

/*
 * Add external H.264 video codec.
 */
MediaConduitErrorCode MediaPipelineFactory::EnsureExternalCodec(
    const mozilla::RefPtr<mozilla::VideoSessionConduit>& conduit,
    mozilla::VideoCodecConfig* config,
    bool send)
{
  if (config->mName == "VP8") {
    return kMediaConduitNoError;
  } else if (config->mName == "H264") {
    // Register H.264 codec.
    if (send) {
      VideoEncoder* encoder = nullptr;
#ifdef MOZ_WEBRTC_OMX
      encoder = OMXVideoCodec::CreateEncoder(OMXVideoCodec::CodecType::CODEC_H264);
#else
      encoder = mozilla::GmpVideoCodec::CreateEncoder();
#endif
      if (encoder) {
        return conduit->SetExternalSendCodec(config, encoder);
      } else {
        return kMediaConduitInvalidSendCodec;
      }
    } else {
      VideoDecoder* decoder;
#ifdef MOZ_WEBRTC_OMX
      decoder = OMXVideoCodec::CreateDecoder(OMXVideoCodec::CodecType::CODEC_H264);
#else
      decoder = mozilla::GmpVideoCodec::CreateDecoder();
#endif
      if (decoder) {
        return conduit->SetExternalRecvCodec(config, decoder);
      } else {
        return kMediaConduitInvalidReceiveCodec;
      }
    }
    NS_NOTREACHED("Shouldn't get here!");
  } else {
    MOZ_MTLOG(ML_ERROR, "Invalid video codec configured: " << config->mName.c_str());
    return send ? kMediaConduitInvalidSendCodec : kMediaConduitInvalidReceiveCodec;
  }

  return kMediaConduitNoError;
}

}  // namespace mozillz
