/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSION_H_
#define _JSEPSESSION_H_

#include <string>
#include <vector>
#include "mozilla/UniquePtr.h"

#include "signaling/src/sdp/Sdp.h"

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>

#include "signaling/src/sdp/Sdp.h"

namespace mozilla {
namespace jsep {

// Forward declarations tracks and track pairs
class JsepMediaStreamTrack;
class JsepTrackPair;

enum JsepSignalingState {
  kJsepStateStable,
  kJsepStateHaveLocalOffer,
  kJsepStateHaveRemoteOffer,
  kJsepStateHaveLocalPranswer,
  kJsepStateHaveRemotePranswer,
  kJsepStateClosed
};

enum JsepSdpType {
  kJsepSdpOffer,
  kJsepSdpAnswer,
  kJsepSdpPranswer,
};

class JsepOAOptions {};
class JsepOfferOptions : public JsepOAOptions {};
class JsepAnswerOptions : public JsepOAOptions {};

class JsepSession {
 public:
  JsepSession(const std::string& name) : mName(name) {}
  virtual ~JsepSession() {}

  // Accessors for basic properties.
  virtual const std::string& name() const { return mName; }
  virtual JsepSignalingState state() const { return mState; }

  // Manage tracks. We take shared ownership of any track.
  virtual nsresult AddTrack(const RefPtr<JsepMediaStreamTrack>& track) = 0;
  virtual nsresult RemoveTrack(size_t track_index) = 0;
  virtual nsresult ReplaceTrack(size_t track_index,
                                const RefPtr<JsepMediaStreamTrack>& track) = 0;
  virtual size_t num_tracks() const = 0;
  virtual nsresult track(size_t index, RefPtr<JsepMediaStreamTrack>* track)
      const = 0;

  // Basic JSEP operations.
  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) = 0;
  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) = 0;
  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) = 0;
  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) = 0;

  // Access the negotiated track pairs.
  virtual nsresult num_negotiated_track_pairs(size_t* pairs) const = 0;
  virtual nsresult track_pair(size_t index, const JsepTrackPair** pair)
      const = 0;

 private:
  const std::string mName;
  JsepSignalingState mState;
};

}  // namespace jsep
}  // namespace mozilla

#endif
