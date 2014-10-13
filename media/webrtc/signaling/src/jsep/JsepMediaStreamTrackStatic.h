/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPMEDIASTREAMTRACKFAKE_H_
#define _JSEPMEDIASTREAMTRACKFAKE_H_

#include <string>
#include <mozilla/RefPtr.h>
#include "nsCOMPtr.h"

#include "signaling/src/jsep/JsepMediaStreamTrack.h"

namespace mozilla {
namespace jsep {

class JsepMediaStreamTrackStatic : public JsepMediaStreamTrack {
 public:
JsepMediaStreamTrackStatic(mozilla::SdpMediaSection::MediaType type,
                           const std::string& streamid,
                           const std::string& trackid)
    : mType(type),
      mStreamId(streamid),
      mTrackId(trackid) {}

  virtual mozilla::SdpMediaSection::MediaType media_type() const MOZ_OVERRIDE {
    return mType;
  }

  virtual const std::string& stream_id() const MOZ_OVERRIDE {
    return mStreamId;
  }

  virtual const std::string& track_id() const MOZ_OVERRIDE {
    return mTrackId;
  }

 private:
  virtual ~JsepMediaStreamTrackStatic() {}

  const mozilla::SdpMediaSection::MediaType mType;
  const std::string mStreamId;
  const std::string mTrackId;
};

}  // namespace jsep
}  // namespace mozilla


#endif
