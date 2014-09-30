/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _dom_media_peerconnectionidpfactory_h
#define _dom_media_peerconnectionidpfactory_h

#include "mozilla/Attributes.h"

#include "nsISupports.h"
#include "nsISupportsImpl.h"

#include "SharedWorker.h"

class nsAString;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class Promise;
template<typename T> class Optional;

namespace workers {
class MessagePort;
}

class PeerConnectionIdpFactory MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  static already_AddRefed<workers::MessagePort>
  CreateIdpInstance(const GlobalObject& aGlobal, const nsAString& aScriptUrl,
                    const nsAString& aName, ErrorResult& rv);

private:
  static void
  BuildName(const nsAString& aUrl, mozilla::dom::Optional<nsAString>* aName);

  PeerConnectionIdpFactory() {} // not making any of these
  ~PeerConnectionIdpFactory() {}
};

} // namespace dom
} // namespace mozilla

#endif
