// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test that TLS version handling is OK.
// These verify that version-based prefs are correctly applied.

"use strict";

let counter = 0;

const NO_OVERLAP = -1;
const NET_RESET = -2;

function test_version_range(connectVersion, vmin, vmax, vfb) {
  let initial_snapshot = snapshot_telemetry("SSL_HANDSHAKE_VERSION");
  set_version_prefs(vmin, vmax, vfb);

  let hostname = "version" + (++counter) + ".example.com";
  let expectedCode = Cr.NS_OK;
  if (connectVersion === NET_RESET) {
    expectedCode = Cr.NS_ERROR_NET_RESET;
  } else if (connectVersion < 0) {
    expectedCode = getXPCOMStatusFromNSS(SSL_ERROR_NO_CYPHER_OVERLAP);
  }

  do_print("connecting with " + vmin + "-" + vmax +
           ((typeof vfb === "number") ? ("-" + vfb) : "") +
           ", expect " + connectVersion + "=" + expectedCode);
  return connect_to_host(hostname, expectedCode)
    .then(() => {
      let final_snapshot = snapshot_telemetry("SSL_HANDSHAKE_VERSION");
      if (connectVersion >= 0) {
        initial_snapshot[connectVersion]++;
      }
      deepEqual(initial_snapshot, final_snapshot);
    });
}

function *test_with_ssl3_to_tls12() {
  // this is the most permissive server configuration
  yield run_server_with_version(0, 3);

  yield test_version_range(3, 1, 3); // default pref
  yield test_version_range(0, 0, 0); // SSL 3.0 only
  yield test_version_range(3, 3, 3); // TLS 1.2 only
  yield test_version_range(3, 0, 3); // most permissive pref

  stop_server();
}

function *test_with_tls10_to_tls12() {
  // this is the currently tolerable server configuration
  // which should be about the same as previous attempts
  yield run_server_with_version(1, 3);

  yield test_version_range(3, 1, 3);
  yield test_version_range(NO_OVERLAP, 0, 0); // SSL 3.0 only fails
  yield test_version_range(1, 1, 1); // TLS 1.0 is OK
  yield test_version_range(3, 3, 3);
  yield test_version_range(3, 0, 3);

  stop_server();
}

function *test_with_sslv3_only() {
  // test the crazy end of the version spectrum
  yield run_server_with_version(0, 0);

  // version negotiation in SSL 3.0 is different to other versions
  yield test_version_range(NET_RESET, 1, 3); // defaults fail
  yield test_version_range(0, 0, 0);  // SSL 3.0 only is OK
  yield test_version_range(NO_OVERLAP, 3, 3); // TLS 1.2 only fails
  yield test_version_range(0, 0, 3);  // Full range, no fallback is OK

  stop_server();
}

function *test_with_tls12_only() {
  // this is a brave server operator
  yield run_server_with_version(3, 3);

  yield test_version_range(3, 1, 3); // defaults fail
  yield test_version_range(NO_OVERLAP, 0, 0);  // SSL 3.0 dies
  yield test_version_range(3, 3, 3); // TLS 1.2 only is OK
  yield test_version_range(3, 0, 3);  // Full range, no fallback is OK

  stop_server();
}

function run_test() {
  disable_weak_cipher_suites();

  [
    test_with_ssl3_to_tls12, test_with_tls10_to_tls12,
    test_with_sslv3_only, test_with_tls12_only
  ].forEach(test => add_task(test));

  run_next_test();
}
