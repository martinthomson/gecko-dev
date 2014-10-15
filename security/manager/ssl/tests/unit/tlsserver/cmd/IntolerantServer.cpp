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

#include <cstdio>
#include "sslt.h"
#include "sslproto.h"

#include "TLSServer.h"

using namespace mozilla;
using namespace mozilla::test;

uint16_t
GetEnvTLSVersion(const char* envName, uint16_t defVersion)
{
  char* val = getenv(envName);
  if (!val) {
    return defVersion;
  }
  char* endptr;
  unsigned long int envValue = strtoul(val, &endptr, 10);
  if (endptr == val || envValue > 3) { // 3 == TLS 1.2
    return defVersion;
  }
  fprintf(stderr, "%s = %lu => %x\n", envName, envValue,
          SSL_LIBRARY_VERSION_3_0 + static_cast<uint16_t>(envValue));

  return SSL_LIBRARY_VERSION_3_0 + static_cast<uint16_t>(envValue);
}

void
ConfigureTLSVersion() {
  uint16_t minVersion = GetEnvTLSVersion("TLS_SERVER_VERSION_MIN",
                                         SSL_LIBRARY_VERSION_TLS_1_0);
  uint16_t maxVersion = GetEnvTLSVersion("TLS_SERVER_VERSION_MAX",
                                         SSL_LIBRARY_VERSION_TLS_1_2);

  SSLVersionRange range = { minVersion, maxVersion };
  SSL_VersionRangeSetDefault(ssl_variant_stream, &range);
}

int32_t
BeVersionIntolerant(PRFileDesc *aFd, const SECItem *aSrvNameArr,
                  uint32_t aSrvNameArrSize, void *aArg)
{
  // an SSLv3 fatal alert (handshake_failure)
  // which should trigger fallback in PSM
  static const char message[] = {21, 3, 3, 0, 2, 2, 40};

  SSLChannelInfo info;
  SECStatus rv = SSL_GetChannelInfo(aFd, &info, sizeof(info));
  if (rv != SECSuccess) {
    return rv;
  }
  fprintf(stderr, "XXXX got version %x\n", info.protocolVersion);

  uint16_t intolerantVersion = *reinterpret_cast<uint16_t*>(aArg);
  if (info.protocolVersion >= intolerantVersion) {
    // This is going to seriously mess up the state machine.
    // Find the plaintext layer and then trash the connection.
    // Note: preserving the typo in the name
    PRDescIdentity plaintextId = PR_GetUniqueIdentity("Plaintxext PSM layer");
    PRFileDesc* rawLayer =  PR_GetIdentitiesLayer(aFd, plaintextId);
    PR_Write(rawLayer, message, sizeof(message));
  }

  ScopedCERTCertificate cert;
  SSLKEAType certKEA;
  if (SECSuccess != ConfigSecureServerWithNamedCert(aFd,
                                                    "localhostAndExampleCom",
                                                    &cert, &certKEA)) {
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }
  static uint16_t intolerant_version =
      GetEnvTLSVersion("TLS_SERVER_VERSION_INTOLERANT", UINT16_MAX);

  if (InitServer(argv[1])) {
    return 1;
  }
  ConfigureTLSVersion();
  return StartServer(BeVersionIntolerant, &intolerant_version);
}
