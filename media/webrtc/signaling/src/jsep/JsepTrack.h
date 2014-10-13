/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRACK_H_
#define _JSEPTRACK_H_

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/jsep/JsepTransport.h"
#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla {
namespace jsep {

// Forward reference.
struct JsepCodecDescription;  // Is this a VideoCodecConfig
class JsepTransport;

class JsepTrack {
 public:
  virtual ~JsepTrack() {}

  enum Direction {
    kJsepTrackSending,
    kJsepTrackReceiving
  };

  virtual Direction direction() const = 0;
  virtual mozilla::SdpMediaSection::MediaType media_type() const = 0;
  virtual mozilla::SdpMediaSection::Protocol protocol() const = 0;
  virtual Maybe<std::string> bandwidth(const std::string& type) const = 0;
  virtual size_t num_codecs() const = 0;
  virtual nsresult get_codec(size_t index,
                             const JsepCodecDescription** config)
      const = 0;
  virtual RefPtr<JsepMediaStreamTrack> media_stream_track() const = 0;
};


struct JsepTrackPair {
  size_t mLevel;
  UniquePtr<JsepTrack> mSending;
  UniquePtr<JsepTrack> mReceiving;
  RefPtr<JsepTransport> mRtpTransport;
  RefPtr<JsepTransport> mRtcpTransport;
};

}  // namespace jsep
}  // namespace mozilla


#endif
