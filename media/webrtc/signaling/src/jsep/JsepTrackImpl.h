/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACKIMPL_H_
#define _JSEPTRACKIMPL_H_

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/jsep/JsepCodecDescription.h"
#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla {
namespace jsep {

// Forward reference.
class JsepTransport;

class JsepTrackImpl : public JsepTrack {
 public:
  virtual ~JsepTrackImpl() {
    for (auto c = mCodecs.begin(); c != mCodecs.end(); ++c) {
      delete *c;
    }
  }

  // Implement JsepTrack.
  virtual Direction direction() const { return mDirection; }
  virtual mozilla::SdpMediaSection::MediaType media_type() const
      MOZ_OVERRIDE { return mMediaType; }
  virtual mozilla::SdpMediaSection::Protocol protocol() const
      MOZ_OVERRIDE { return mProtocol; }
  virtual Maybe<std::string> bandwidth(const std::string& type) const
      MOZ_OVERRIDE { return mBandwidth; }
  virtual size_t num_codecs() const MOZ_OVERRIDE { return mCodecs.size(); }
  virtual nsresult get_codec(size_t index, const JsepCodecDescription** config)
      const MOZ_OVERRIDE {
    if (index >= mCodecs.size()) {
      return NS_ERROR_INVALID_ARG;
    }
    *config = mCodecs[index];
    return NS_OK;
  }
  virtual RefPtr<JsepMediaStreamTrack> media_stream_track() const MOZ_OVERRIDE {
    return mMediaStreamTrack;
  }

 private:
  // Make these friends to JsepSessionImpl to avoid having to
  // write setters.
  friend class JsepSessionImpl;

  Direction mDirection;
  mozilla::SdpMediaSection::MediaType mMediaType;
  mozilla::SdpMediaSection::Protocol mProtocol;
  Maybe<std::string> mBandwidth;
  std::vector<JsepCodecDescription*> mCodecs;
  RefPtr<JsepMediaStreamTrack> mMediaStreamTrack;
};


class JsepTrackImplPair {
  UniquePtr<JsepTrackImpl> mSending;
  UniquePtr<JsepTrackImpl> mReceiving;
};

}  // namespace jsep
}  // namespace mozilla


#endif

