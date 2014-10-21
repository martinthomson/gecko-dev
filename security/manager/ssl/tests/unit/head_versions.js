// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test that version tolerance is properly handled.
// These verify that fallback occurs correctly.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function set_version_prefs(vmin, vmax, vfallback) {
  Services.prefs.setIntPref("security.tls.version.min", vmin);
  Services.prefs.setIntPref("security.tls.version.max", vmax);
  if (typeof vfb === "undefined") {
    vfallback = vmin;
  }
  Services.prefs.setIntPref("security.tls.version.fallback-limit", vfallback);
}

let serverProcess;

function stop_server() {
  if (serverProcess && serverProcess.isRunning) {
    serverProcess.kill();
  }
}

function run_server_with_version(vmin, vmax, vintolerant, sendScsv) {
  const intolerantPref = "MOZ_TLS_SERVER_VERSION_INTOLERANT";
  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  envSvc.set("MOZ_TLS_SERVER_VERSION_MIN", vmin.toString(10));
  envSvc.set("MOZ_TLS_SERVER_VERSION_MAX", vmax.toString(10));
  if (typeof vintolerant === "number") {
    envSvc.set(intolerantPref, vintolerant.toString(10));
  } else {
    envSvc.set(intolerantPref, "");
  }
  stop_server();
  let serverStarted = start_tls_server("IntolerantServer");
//  addCertFromFile(certdb, "tlsserver/other-test-ca.der", "CTu,u,u");
  return serverStarted.then(p => serverProcess = p);
}

function snapshot_telemetry(name) {
  let service = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  return service.getHistogramById(name).snapshot().counts;
}

function disable_weak_cipher_suites() {
  // bug 1088915 added a layer of fallback that is inconvenient
  // disabling all the "weak" cipher suites disables that fallback step
  Services.prefs.setBoolPref("security.ssl3.ecdhe_ecdsa_rc4_128_sha", false);
  Services.prefs.setBoolPref("security.ssl3.ecdhe_rsa_rc4_128_sha", false);
  Services.prefs.setBoolPref("security.ssl3.rsa_rc4_128_md5", false);
  Services.prefs.setBoolPref("security.ssl3.rsa_rc4_128_sha", false);
}
