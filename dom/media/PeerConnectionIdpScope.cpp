/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeerConnectionIdpScope.h"

#include "Principal.h"
#include "WorkerPrivate.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/PeerConnectionIdpScopeBinding.h"

namespace mozilla {
namespace dom {
namespace workers {

PeerConnectionIdpScope::PeerConnectionIdpScope(WorkerPrivate* aWorkerPrivate)
  : WorkerGlobalScope(aWorkerPrivate)
  , mPort(new PeerConnectionIdpScopePort(aWorkerPrivate))
{}


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PeerConnectionIdpScope)
NS_INTERFACE_MAP_END_INHERITING(WorkerGlobalScope)
NS_IMPL_ADDREF_INHERITED(PeerConnectionIdpScope, WorkerGlobalScope)
NS_IMPL_RELEASE_INHERITED(PeerConnectionIdpScope, WorkerGlobalScope)
NS_IMPL_CYCLE_COLLECTION_INHERITED(PeerConnectionIdpScope,
                                   WorkerGlobalScope, mPort)

JSObject*
PeerConnectionIdpScope::WrapGlobalObject(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->IsDedicatedWorker());

  JS::CompartmentOptions options;
  mWorkerPrivate->CopyJSCompartmentOptions(options);

  JSPrincipals* jsPrincipal = GetWorkerPrincipal();
  return PeerConnectionIdpScopeBinding_workers::Wrap(aCx, this, this, options,
                                                     jsPrincipal, true);
}

already_AddRefed<MessagePortBase>
PeerConnectionIdpScope::WebrtcIdentityPort() const
{
  nsRefPtr<MessagePortBase> port = mPort.get();
  return port.forget();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PeerConnectionIdpScopePort)
NS_INTERFACE_MAP_END_INHERITING(MessagePortBase)
NS_IMPL_ADDREF_INHERITED(PeerConnectionIdpScopePort, MessagePortBase)
NS_IMPL_RELEASE_INHERITED(PeerConnectionIdpScopePort, MessagePortBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(PeerConnectionIdpScopePort,
                                   MessagePortBase, mWorker)

JSObject*
PeerConnectionIdpScopePort::WrapObject(JSContext* aCx)
{
  return MessagePortBinding::Wrap(aCx, this);
}

void
PeerConnectionIdpScopePort::PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
               const Optional<Sequence<JS::Value>>& aTransferable,
               ErrorResult& aRv)
{
  return mWorker->PostMessage(aCx, aMessage, aTransferable, aRv);
}

EventHandlerNonNull*
PeerConnectionIdpScopePort::GetOnmessage()
{
  return mWorker->GetOnmessage();
}

void
PeerConnectionIdpScopePort::SetOnmessage(EventHandlerNonNull* aCallback)
{
  mWorker->SetOnmessage(aCallback);
}

// this doesn't support structured clone; this port can't be transferred anywhere
// TODO test that it can't be transferred over itself, or that it breaks cleanly
already_AddRefed<MessagePortBase>
PeerConnectionIdpScopePort::Clone()
{
  return nullptr;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
