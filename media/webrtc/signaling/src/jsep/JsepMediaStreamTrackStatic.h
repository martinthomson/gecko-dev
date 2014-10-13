/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPMEDIASTREAMTRACKFAKE_H_
#define _JSEPMEDIASTREAMTRACKFAKE_H_

#include <mozilla/RefPtr.h>
#include "nsCOMPtr.h"

#include "signaling/src/jsep/JsepMediaStreamTrack.h"

namespace mozilla {
namespace jsep {

class JsepMediaStreamTrackFake : public JsepMediaStreamTrack {
 public:
  JsepMediaStreamTrackFake(mozilla::SdpMediaSection::MediaType type) :
      mType(type) {}

  virtual mozilla::SdpMediaSection::MediaType media_type() const MOZ_OVERRIDE {
    return mType;
  }

 private:
  virtual ~JsepMediaStreamTrackFake() {}

  mozilla::SdpMediaSection::MediaType mType;
};

}  // namespace jsep
}  // namespace mozilla


#endif
