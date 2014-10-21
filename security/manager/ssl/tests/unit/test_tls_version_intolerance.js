// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test that version tolerance is properly handled.
// These verify that fallback occurs correctly.

"use strict";

function do_xhr(aHost) {
  const PORT = 8443;
  let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
    .createInstance(Ci.nsIXMLHttpRequest);
  req.open("HEAD", "https://" + aHost + ":" + PORT + "/", true);
  let resolve;
  let promise = new Promise(r => resolve = r);
  req.onload = () => resolve(req.status);
  req.onerror = () => {
    let error = req.channel.QueryInterface(Ci.nsIRequest).status;
    do_print(error);
    resolve(error);
  };
  req.send();
  return promise;
}

function check_server(name, expected, vmin, vmax, vfb) {
  set_version_prefs(vmin, vmax, vfb);

  let hostname = "intolerant-" + name + ".example.com";
  Services.prefs.setCharPref("network.dns.localDomains", hostname);
  return do_xhr(hostname)
    .then(err => strictEqual(getXPCOMStatusFromNSS(expected), err));
}

function sum_telemetry(x) {
  return snapshot_telemetry(x).reduce((a, b) => a + b, 0);
}

// returns an array containing one object for each TLS version
// each object has .pre and .post containing a single count
// no need to check the individual fallback reasons at this point
function collect_fallback_telemetry() {
  return [
    { pre: "SSL_TLS10_INTOLERANCE_REASON_PRE",
      post: "SSL_TLS10_INTOLERANCE_REASON_POST" },
    { pre: "SSL_TLS11_INTOLERANCE_REASON_PRE",
      post: "SSL_TLS11_INTOLERANCE_REASON_POST" },
    { pre: "SSL_TLS12_INTOLERANCE_REASON_PRE",
      post: "SSL_TLS12_INTOLERANCE_REASON_POST" }
  ].map(x => {
    return {
      pre: sum_telemetry(x.pre),
      post: sum_telemetry(x.post)
    };
  });
}

function *run_tls_fallback_scsv_test() {
  // This server has forced intolerance at TLS 1.1, but it is configured for TLS
  // 1.2. When we connect, we fallback to TLS 1.0, which then triggers an
  // inappropriate fallback alert from the server.
  yield run_server_with_version(1, 3, 2);

  // need to run this twice because we need to forget
  for (let i = 0; i < 2; ++i) {
    let handshakesBaseline = snapshot_telemetry("SSL_HANDSHAKE_VERSION");
    let fallbackBaseline = collect_fallback_telemetry();

    yield check_server("scsv", SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT,
                       1, 2, 1);

    // expect one level of fallback
    ++fallbackBaseline[1].pre;
    ++fallbackBaseline[1].post;

    deepEqual(handshakesBaseline, snapshot_telemetry("SSL_HANDSHAKE_VERSION"));
    deepEqual(fallbackBaseline, collect_fallback_telemetry());
  }

  stop_server();
}

function *run_tls_intolerance_test() {
  // this server has forced intolerance at TLS 1.1, period
  yield run_server_with_version(1, 2, 2);

  let handshakesBaseline = snapshot_telemetry("SSL_HANDSHAKE_VERSION");
  let fallbackBaseline = collect_fallback_telemetry();

  yield check_server("fail", SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT,
                     1, 3, 1); // fallback hard

  // expect fallback at all versions
  for (let i = 1; i <= 2; ++i) {
    ++fallbackBaseline[i].pre;
    ++fallbackBaseline[i].post;
  }

  deepEqual(handshakesBaseline, snapshot_telemetry("SSL_HANDSHAKE_VERSION"));
  deepEqual(fallbackBaseline, collect_fallback_telemetry());

  stop_server();
}

function *run_tls_fallback_pref_test() {
  yield run_server_with_version(1, 2, 2);

  let handshakesBaseline = snapshot_telemetry("SSL_HANDSHAKE_VERSION");
  let fallbackBaseline = collect_fallback_telemetry();

  yield check_server("nofallback", SSL_ERROR_NO_CYPHER_OVERLAP,
                     2, 2, 2);

  ++fallbackBaseline[1].pre; // no post because we block fallback

  deepEqual(handshakesBaseline, snapshot_telemetry("SSL_HANDSHAKE_VERSION"));
  deepEqual(fallbackBaseline, collect_fallback_telemetry());

  stop_server();
}

function run_test() {
  [
    run_tls_fallback_scsv_test,
    run_tls_intolerance_test,
    run_tls_fallback_scsv_test
  ].forEach(test => add_task(test));

  run_next_test();
}
