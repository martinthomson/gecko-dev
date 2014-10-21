/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that uses various bad certificates.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the certificate.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>
#include "prenv.h"
#include "sslt.h"
#include "sslproto.h"

#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

uint16_t
GetEnvTLSVersion(const char* envName, uint16_t defVersion)
{
  const char* val = PR_GetEnv(envName);
  if (!val) {
    return defVersion;
  }
  char* endptr;
  unsigned long int envValue = strtoul(val, &endptr, 10);
  if (endptr == val || envValue > 3) { // 3 == TLS 1.2
    return defVersion;
  }
  uint16_t version = SSL_LIBRARY_VERSION_3_0 + static_cast<uint16_t>(envValue);
  fprintf(stderr, "%s = %lu => %x\n", envName, envValue, version);

  return version;
}

void
ConfigureTLSVersion() {
  uint16_t minVersion = GetEnvTLSVersion("MOZ_TLS_SERVER_VERSION_MIN",
                                         SSL_LIBRARY_VERSION_TLS_1_0);
  uint16_t maxVersion = GetEnvTLSVersion("MOZ_TLS_SERVER_VERSION_MAX",
                                         SSL_LIBRARY_VERSION_TLS_1_2);

  SSLVersionRange range = { minVersion, maxVersion };
  SSL_VersionRangeSetDefault(ssl_variant_stream, &range);
}

int32_t
BeVersionIntolerant(PRFileDesc* aFd, const SECItem* aSrvNameArr,
                  uint32_t aSrvNameArrSize, void* aArg)
{
  // an SSLv3 fatal alert (handshake_failure)
  // which should trigger fallback in PSM
  static const char message[] = {21, 3, 3, 0, 2, 2, 40};

  SSLPreliminaryChannelInfo info;
  SECStatus rv = SSL_GetPreliminaryChannelInfo(aFd, &info, sizeof(info));
  if (rv != SECSuccess) {
    fprintf(stderr, "SSL_GetPreliminaryChannelInfo failed\n");
    return SSL_SNI_SEND_ALERT;
  }
  if (!(info.valuesSet & ssl_preinfo_version)) {
    fprintf(stderr, "SSL_GetPreliminaryChannelInfo didn't produce a version\n");
    return SSL_SNI_SEND_ALERT;
  }
  fprintf(stderr, "New TLS handshake %x\n", info.protocolVersion);

  uint16_t intolerantVersion = *reinterpret_cast<uint16_t*>(aArg);
  if (info.protocolVersion >= intolerantVersion) {
    fprintf(stderr, "Simulating intolerance\n");

    // This is going to seriously mess up the state machine, intentionally.
    // Do that by injecting an alert at the TCP layer.
    PRFileDesc* removedLayer =  PR_PopIOLayer(aFd, PR_TOP_IO_LAYER);
    PR_Write(aFd, message, sizeof(message));
    PR_PushIOLayer(aFd, PR_TOP_IO_LAYER, removedLayer);

    // !!!Note that simulating intolerance this way means we only get to
    // fallback once.  Any handshakes that are retried with lower versions will
    // trigger the fallback SCSV code before it even gets here.
  }

  ScopedCERTCertificate cert;
  SSLKEAType certKEA;
  rv = ConfigSecureServerWithNamedCert(aFd, "localhostAndExampleCom",
                                       &cert, &certKEA);
  if (rv != SECSuccess) {
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int
main(int argc, char* argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }
  static uint16_t intolerant_version =
      GetEnvTLSVersion("MOZ_TLS_SERVER_VERSION_INTOLERANT", UINT16_MAX);

  if (InitServer(argv[1])) {
    return 1;
  }
  ConfigureTLSVersion();
  return StartServer(BeVersionIntolerant, &intolerant_version);
}
