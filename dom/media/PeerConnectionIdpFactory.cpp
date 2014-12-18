/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZILLA_INTERNAL_API

#include "PeerConnectionIdpFactory.h"

#include "mozilla/Assertions.h"

#include "nsAutoPtr.h"
#include "nsISupportsImpl.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"

#include "js/Value.h"
#include "jsapi.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/workers/bindings/MessagePort.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "mtransport/runnable_utils.h"

#include "PeerConnectionIdpScope.h"

namespace mozilla {
namespace dom {

class IdpWorkerScopeFactory MOZ_FINAL : public workers::WorkerGlobalScopeFactory
{
public:
  virtual already_AddRefed<workers::WorkerGlobalScope>
  CreateGlobalScope(workers::WorkerPrivate* aWorkerPrivate,
                    const nsACString& aWorkerName)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    nsRefPtr<workers::WorkerGlobalScope> scope;
    scope = new workers::PeerConnectionIdpScope(aWorkerPrivate);
    return scope.forget();
  }

  static already_AddRefed<workers::WorkerGlobalScopeFactory>
  Instance() {
    if (!sInstance) {
      sInstance = new IdpWorkerScopeFactory();
      ClearOnShutdown(&sInstance);
    }
    nsRefPtr<workers::WorkerGlobalScopeFactory> f = sInstance.get();
    return f.forget();
  }

private:
  static StaticRefPtr<workers::WorkerGlobalScopeFactory> sInstance;
};

StaticRefPtr<workers::WorkerGlobalScopeFactory>
IdpWorkerScopeFactory::sInstance;


NS_IMPL_ISUPPORTS0(PeerConnectionIdpFactory)

/**
 * Creates a new special IdP worker.
 */
/* static */ already_AddRefed<workers::WorkerPrivate>
PeerConnectionIdpFactory::CreateIdpInstance(const GlobalObject& aGlobal,
                                            const nsAString& aScriptUrl,
                                            ErrorResult& aRv)
{
  nsRefPtr<workers::WorkerGlobalScopeFactory> workerScopeFactory
    = IdpWorkerScopeFactory::Instance();
  nsRefPtr<workers::WorkerPrivate> worker
    = workers::WorkerPrivate::Constructor(aGlobal, aScriptUrl, false,
                                          workers::WorkerTypeDedicated,
                                          EmptyCString(), nullptr,
                                          workerScopeFactory, aRv);
  return worker.forget();
}

} // namespace dom
} // namespace mozilla

#endif // MOZILLA_INTERNAL_API
