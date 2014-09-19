/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPSESSIONIMPL_H_
#define _JSEPSESSIONIMPL_H_


#include <vector>

#include "signaling/src/jsep/JsepMediaStreamTrack.h"
#include "signaling/src/jsep/JsepSession.h"

namespace mozilla {
namespace jsep {

class JsepSessionImpl : public JsepSession {
 public:
  JsepSessionImpl(const std::string& name) :
      JsepSession(name) {}
  JsepSessionImpl() :
      JsepSession("anonymous"),
      mSessionId(0),
      mSessionVersion(0) {}

   virtual ~JsepSessionImpl() {}

  // Implement JsepSession methods.
  virtual nsresult AddTrack(const RefPtr<JsepMediaStreamTrack>& track)
      MOZ_OVERRIDE;
  virtual nsresult RemoveTrack(size_t track_index) MOZ_OVERRIDE {
    MOZ_CRASH();  // Stub
  }
  virtual nsresult ReplaceTrack(
    size_t track_index,
      const RefPtr<JsepMediaStreamTrack>& track) MOZ_OVERRIDE {
    MOZ_CRASH(); // Stub
  }

  virtual size_t num_tracks() const MOZ_OVERRIDE {
    return mSendingTracks.size();
  }
  virtual nsresult track(size_t index, RefPtr<JsepMediaStreamTrack>* track) const MOZ_OVERRIDE;
  virtual nsresult CreateOffer(const JsepOfferOptions& options,
                               std::string* offer) MOZ_OVERRIDE;


// STUBS BELOW THIS POINT
  virtual nsresult CreateAnswer(const JsepAnswerOptions& options,
                                std::string* answer) MOZ_OVERRIDE { MOZ_CRASH(); }
  virtual nsresult SetLocalDescription(JsepSdpType type,
                                       const std::string& sdp) MOZ_OVERRIDE { MOZ_CRASH(); }
  virtual nsresult SetRemoteDescription(JsepSdpType type,
                                        const std::string& sdp) MOZ_OVERRIDE { MOZ_CRASH(); }

  // Access the negotiated track pairs.
  virtual nsresult num_negotiated_track_pairs(size_t* pairs) const MOZ_OVERRIDE { MOZ_CRASH(); }
  virtual nsresult track_pair(size_t index, const JsepTrackPair** pair)
      const MOZ_OVERRIDE { MOZ_CRASH(); }

 private:
  nsresult CreateGenericSDP(UniquePtr<Sdp>* sdp);

  struct JsepSendingTrack {
    RefPtr<JsepMediaStreamTrack> mTrack;
  };

  std::vector<JsepSendingTrack> mSendingTracks;

  uint64_t mSessionId;
  uint64_t mSessionVersion;
};

}  // namespace jsep
}  // namespace mozilla

#endif
