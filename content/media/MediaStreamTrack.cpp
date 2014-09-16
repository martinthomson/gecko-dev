/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrack.h"

#include "DOMMediaStream.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID)
  : mStream(aStream), mTrackID(aTrackID), mEnded(false), mEnabled(true)
{
  SetIsDOMBinding();

  mID.Truncate();
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (uuidgen) {
    nsID id;
    uuidgen->GenerateUUIDInPlace(&id);
    char buffer[NSID_LENGTH];
    id.ToProvidedString(buffer);
    mID = NS_ConvertUTF8toUTF16(buffer);
  }
}

MediaStreamTrack::MediaStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                                   const nsAString& aID)
  : mStream(aStream), mTrackID(aTrackID), mID(aID), mEnded(false),
  mEnabled(true)
{
  SetIsDOMBinding();
}

MediaStreamTrack::~MediaStreamTrack()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaStreamTrack, DOMEventTargetHelper,
                                   mStream)

NS_IMPL_ADDREF_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrack, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamTrack)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

void
MediaStreamTrack::SetEnabled(bool aEnabled)
{
  mEnabled = aEnabled;
  mStream->SetTrackEnabled(mTrackID, aEnabled);
}

void
MediaStreamTrack::Stop()
{
  mStream->StopTrack(mTrackID);
}

}
}
