// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test that version tolerance is properly handled.
// These verify that fallback occurs and that version-based prefs are
// correctly applied.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function set_version_prefs(vmin, vmax, vfb) {
  Services.prefs.setIntPref("security.tls.version.min", vmin);
  Services.prefs.setIntPref("security.tls.version.max", vmax);
  Services.prefs.setIntPref("security.tls.version.fallback-limit", vfb);
}

var serverProcess;

function run_server_with_version(vmin, vmax) {
  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  envSvc.set("TLS_SERVER_VERSION_MIN", vmin.toString(10));
  envSvc.set("TLS_SERVER_VERSION_MAX", vmax.toString(10));
  let serverStarted = start_tls_server("BadCertServer");
  addCertFromFile(certdb, "tlsserver/other-test-ca.der", "CTu,u,u");
  return serverStarted.then(p => serverProcess = p);
}

function stop_server() {
  serverProcess.kill();
}

function snapshot_handshake_version_telemetry() {
  let service = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  return service.getHistogramById("SSL_HANDSHAKE_VERSION").snapshot().counts;
}

function test_version_range(connectVersion, vmin, vmax, vfb) {
  let initial_snapshot = snapshot_handshake_version_telemetry();
  set_version_prefs(vmin, vmax, vfb);
  let expectSuccess = (connectVersion >= 0);

  // connect to a known-good host
  return connect_to_host("good.include-subdomains.pinning.example.com",
                             expectSuccess ? Cr.NS_OK :
                             getXPCOMStatusFromNSS(SSL_ERROR_NO_CYPHER_OVERLAP))
    .then(() => {
      let final_snapshot = snapshot_handshake_version_telemetry();
      if (expectSuccess) {
        initial_snapshot[connectVersion]++;
      }
      deepEqual(initial_snapshot, final_snapshot);
    });
}

function test_with_ssl3_to_tls12() {
  // this is the most permissive server configuration
  yield run_server_with_version(0, 3);

  yield test_version_range(3, 1, 3, 1); // default pref
  yield test_version_range(0, 0, 0, 0); // SSL 3.0 only
  yield test_version_range(3, 3, 3, 3); // TLS 1.2 only
  yield test_version_range(3, 0, 3, 0); // most permissive pref

  stop_server();
}

function test_with_tls10_to_tls12() {
  // this is the currently tolerable server configuration
  // which should be about the same as previous attempts
  yield run_server_with_version(1, 3);

  yield test_version_range(3, 1, 3, 1);
  yield test_version_range(-1, 0, 0, 0); // SSL 3.0 only fails
  yield test_version_range(1, 1, 1, 1); // TLS 1.0 is OK
  yield test_version_range(3, 3, 3, 3);
  yield test_version_range(3, 0, 3, 0);

  stop_server();
}

function test_with_sslv3_only() {
  // test the crazy end of the version spectrum
  yield run_server_with_version(0, 0);

  yield test_version_range(-1, 1, 3, 1); // defaults fail
  yield test_version_range(0, 0, 0, 0);  // SSL 3.0 only is OK
  yield test_version_range(-1, 3, 3, 3); // TLS 1.2 only fails
  yield test_version_range(0, 0, 3, 3);  // Full range, no fallback is OK

  stop_server();
}

function test_with_tls12_only() {
  // this is a brave server operator
  yield run_server_with_version(3, 3);

  yield test_version_range(3, 1, 3, 1); // defaults fail
  yield test_version_range(-1, 0, 0, 0);  // SSL 3.0 only is OK
  yield test_version_range(3, 3, 3, 3); // TLS 1.2 only is OK
  yield test_version_range(3, 0, 3, 3);  // Full range, no fallback is OK

  stop_server();
}

function run_test() {
  add_task(test_with_ssl3_to_tls12);
  add_task(test_with_tls10_to_tls12);
  add_task(test_with_sslv3_only);
  add_task(test_with_tls12_only);
  run_next_test();
}
