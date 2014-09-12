/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _dom_media_peerconnectionidpscope_h
#define _dom_media_peerconnectionidpscope_h

#include "mozilla/Attributes.h"

#include "nsAutoPtr.h"

#include "jsapi.h"
#include "Principal.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"
#include "mozilla/dom/PeerConnectionIdpScopeBinding.h"

namespace mozilla {
namespace dom {
namespace workers {

class PeerConnectionIdpScope MOZ_FINAL : public workers::WorkerGlobalScope
{
public:
  PeerConnectionIdpScope(workers::WorkerPrivate* aWorkerPrivate,
                         nsRefPtr<workers::MessagePort>& aPort)
    : workers::WorkerGlobalScope(aWorkerPrivate), mPort(aPort) {}

  virtual JSObject*
  WrapGlobalObject(JSContext* aCx) MOZ_OVERRIDE
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(mWorkerPrivate->IsSharedWorker());

    JS::CompartmentOptions options;
    mWorkerPrivate->CopyJSCompartmentOptions(options);

    JSPrincipals* jsPrincipal = workers::GetWorkerPrincipal();
    return PeerConnectionIdpScopeBinding_workers::Wrap(aCx, this, this,
                                                       options,
                                                       jsPrincipal,
                                                       true);
  }

  already_AddRefed<workers::MessagePort>
  WebrtcIdentityPort() const
  {
    nsRefPtr<workers::MessagePort> port = mPort;
    return port.forget();
  }

  void SetPort(nsRefPtr<workers::MessagePort>& aPort)
  {
    mPort = aPort;
  }

private:
  ~PeerConnectionIdpScope() {}
  nsRefPtr<workers::MessagePort> mPort;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif
