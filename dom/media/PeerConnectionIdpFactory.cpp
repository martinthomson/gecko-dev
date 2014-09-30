/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZILLA_INTERNAL_API

#include "PeerConnectionIdpFactory.h"

#include "mozilla/Assertions.h"

#include "nsAutoPtr.h"
#include "MainThreadUtils.h"
#include "nsProxyRelease.h"
#include "nsISupportsImpl.h"

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

using namespace mozilla::dom::workers;

class IdpWorkerScopeFactory MOZ_FINAL : public WorkerGlobalScopeFactory
{
public:
  static nsRefPtr<WorkerGlobalScopeFactory> instance;

  virtual already_AddRefed<WorkerGlobalScope>
  CreateGlobalScope(WorkerPrivate* aWorkerPrivate,
                    const nsACString& aWorkerName)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<WorkerGlobalScope> scope;
    scope = new workers::PeerConnectionIdpScope(aWorkerPrivate, mPort);
    mPort = nullptr;
    return scope.forget();
  }

  void
  SetPort(nsRefPtr<workers::MessagePort>& port)
  {
    mPort = port;
  }

private:
  nsRefPtr<workers::MessagePort> mPort;
};


// This has to be a control runnable because control runnables are able to
// preempt execution of a sync runnable.  The main script loading runnable runs
// synchronously on the worker thread, and it includes both the creation of the
// global and execution of the worker script.
// This needs to run before the latter of those two events.
class MessagePortInjectorRunnable MOZ_FINAL : public WorkerControlRunnable
{
public:
  MessagePortInjectorRunnable(WorkerPrivate* aWorkerPrivate,
                              nsRefPtr<IdpWorkerScopeFactory>& aScopeFactory,
                              uint64_t aSerial)
    : WorkerControlRunnable(aWorkerPrivate,
                            WorkerRunnable::WorkerThreadUnchangedBusyCount)
    , mScopeFactory(aScopeFactory), mSerial(aSerial) {}
  ~MessagePortInjectorRunnable() {}

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate == mWorkerPrivate);

    nsRefPtr<workers::MessagePort> workerPort;
    workerPort = new workers::MessagePort(aWorkerPrivate, mSerial);
    aWorkerPrivate->ConnectMessagePort(workerPort);

    // the global scope for the worker might not be present when this runs
    PeerConnectionIdpScope* globalScope
      = static_cast<PeerConnectionIdpScope*>(aWorkerPrivate->GlobalScope());
    if (globalScope) {
      // if the scope is present, set the port
      globalScope->SetPort(workerPort);
    } else {
      // if this is called *before* the creation of a global scope,
      // set the port on the factory
      mScopeFactory->SetPort(workerPort);
    }
    return true;
  }

private:
  // the factory that creates the scope
  nsRefPtr<IdpWorkerScopeFactory> mScopeFactory;
  // we have to hold this because it can only be obtained on the main thread
  uint64_t mSerial;
};


NS_IMPL_ISUPPORTS0(PeerConnectionIdpFactory)

/**
 * Creates a new shared worker and injects a special port into it.
 */
/* static */ already_AddRefed<workers::MessagePort>
PeerConnectionIdpFactory::CreateIdpInstance(const GlobalObject& aGlobal,
                                            const nsAString& aScriptUrl,
                                            const nsAString& aName,
                                            ErrorResult& rv)
{
  nsRefPtr<IdpWorkerScopeFactory> scopeFactory
    = new IdpWorkerScopeFactory();
  nsRefPtr<WorkerGlobalScopeFactory> workerScopeFactory
    = scopeFactory.get();  // bleargh
  mozilla::dom::Optional<nsAString> idpName;
  idpName = &aName;
  nsRefPtr<SharedWorker> worker
    = SharedWorker::Constructor(aGlobal, aGlobal.Context(), aScriptUrl,
                                idpName, workerScopeFactory, rv);

  WorkerPrivate* workerPrivate = worker->GetWorkerPrivate();
  uint64_t serial = workerPrivate->NextMessagePortSerial();

  nsCOMPtr<nsPIDOMWindow> window;
  window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);
  nsRefPtr<workers::MessagePort> mainPort
    = new workers::MessagePort(window, worker, serial);
  workerPrivate->RegisterMessagePort(aGlobal.Context(), mainPort);

  // this needs to happen *now*, we can't yield or there is a race
  // we only ensure that the port is present in the worker before script is run
  // by virtue of the worker setup code having to hit the main thread (this thread)
  // once more before executing script; defer here and we lose the race
  nsRefPtr<MessagePortInjectorRunnable> runnable
    = new MessagePortInjectorRunnable(workerPrivate, scopeFactory, serial);
  if (!runnable->Dispatch(aGlobal.Context())) {
    JS_ReportPendingException(aGlobal.Context());
    return nullptr;
  }

  return mainPort.forget();
}

} // namespace dom
} // namespace mozilla

#endif // MOZILLA_INTERNAL_API
