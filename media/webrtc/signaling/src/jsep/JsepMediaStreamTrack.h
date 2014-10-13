/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPMEDIASTREAMTRACK_H_
#define _JSEPMEDIASTREAMTRACK_H_

#include <mozilla/RefPtr.h>
#include "nsCOMPtr.h"

#include "signaling/src/sdp/SdpMediaSection.h"

namespace mozilla {
namespace jsep {

// This class represents a MST that comes in from the browser.
// It's only used to add to JsepSession to indicate what we
// intend to send.
class JsepMediaStreamTrack {
 public:
  virtual mozilla::SdpMediaSection::MediaType media_type() const = 0;
  virtual const std::string& stream_id() const = 0;
  virtual const std::string& track_id() const = 0;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JsepMediaStreamTrack);

 protected:
  virtual ~JsepMediaStreamTrack() {}
};

}  // namespace jsep
}  // namespace mozilla


#endif
