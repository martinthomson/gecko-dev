/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _dom_media_peerconnectionidpscope_h
#define _dom_media_peerconnectionidpscope_h

#include "mozilla/Attributes.h"

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "jsapi.h"
#include "WorkerScope.h"
#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class MessagePortBase;

namespace workers {

class PeerConnectionIdpScopePort;

class PeerConnectionIdpScope MOZ_FINAL : public WorkerGlobalScope
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PeerConnectionIdpScope,
                                           WorkerGlobalScope)

  explicit PeerConnectionIdpScope(WorkerPrivate* aWorkerPrivate);

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) MOZ_OVERRIDE;

  already_AddRefed<MessagePortBase> WebrtcIdentityPort() const;

private:
  ~PeerConnectionIdpScope() {}
  nsRefPtr<PeerConnectionIdpScopePort> mPort;
};

class PeerConnectionIdpScopePort MOZ_FINAL : public MessagePortBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PeerConnectionIdpScopePort,
                                           MessagePortBase)

  explicit PeerConnectionIdpScopePort(WorkerPrivate* aWorker)
    : mWorker(aWorker) {}

  JSObject*
  WrapObject(JSContext* aCx);

  virtual void
  PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                 const Optional<Sequence<JS::Value>>& aTransferable,
                 ErrorResult& aRv) MOZ_OVERRIDE;
  virtual void Start() MOZ_OVERRIDE {}
  virtual void Close() MOZ_OVERRIDE {}
  virtual EventHandlerNonNull* GetOnmessage() MOZ_OVERRIDE;
  virtual void SetOnmessage(EventHandlerNonNull* aCallback) MOZ_OVERRIDE;
  virtual already_AddRefed<MessagePortBase> Clone() MOZ_OVERRIDE;

private:
  ~PeerConnectionIdpScopePort() {}

  nsRefPtr<WorkerPrivate> mWorker;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif
