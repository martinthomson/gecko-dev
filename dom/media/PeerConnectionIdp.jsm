/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PeerConnectionIdp"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "IdpSandbox",
  "resource://gre/modules/media/IdpSandbox.jsm");

function TimerPromise(resolve) {}
TimerPromise.prototype = {
  notify: function() {
    this.resolve();
  },
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback])
}
function delay(delay) {
  return new Promise(resolve => {
    let timer = Cc['@mozilla.org/timer;1']
      .getService(Ci.nsITimer);
    timer.initWithCallback(new TimerResolver(resolve), delay, 0); // One shot
  });
}

/**
 * Creates an IdP helper.
 *
 * @param window (object) the window object to use for miscellaneous goodies
 * @param timeout (int) the timeout in milliseconds
 * @param warningFunc (function) somewhere to dump warning messages
 * @param dispatchEventFunc (function) somewhere to dump error events
 */
function PeerConnectionIdp(window, timeout, warningFunc, dispatchEventFunc) {
  this._win = window;
  this._timeout = timeout || 5000;
  this._warning = warningFunc;
  this._dispatchEvent = dispatchEventFunc;

  this.assertion = null;
  this.provider = null;
}

(function() {
  PeerConnectionIdp._mLinePattern = new RegExp("^m=", "m");
  // attributes are funny, the 'a' is case sensitive, the name isn't
  let pattern = "^a=[iI][dD][eE][nN][tT][iI][tT][yY]:(\\S+)";
  PeerConnectionIdp._identityPattern = new RegExp(pattern, "m");
  pattern = "^a=[fF][iI][nN][gG][eE][rR][pP][rR][iI][nN][tT]:(\\S+) (\\S+)";
  PeerConnectionIdp._fingerprintPattern = new RegExp(pattern, "m");
})();

PeerConnectionIdp.prototype = {
  get enabled() {
    return !!this._idp;
  },

  setIdentityProvider: function(provider, protocol, username) {
    this.provider = provider;
    this.protocol = protocol || "default";
    this.username = username;
    if (this._idp) {
      if (this._idp.isSame(provider, protocol)) {
        return; // noop
      }
      this._idp.stop();
    }
    this._idp = new IdpSandbox(this._win, provider, protocol);
  },

  close: function() {
    this.assertion = null;
    this.provider = null;
    if (this._idp) {
      this._idp.stop();
      this._idp = null;
    }
  },

  /**
   * Generate an error event of the identified type;
   * and put a little more precise information in the console.
   */
  reportError: function(type, message, extra) {
    let args = {
      idp: this.provider,
      protocol: this.protocol
    };
    if (extra) {
      Object.keys(extra).forEach(function(k) {
        args[k] = extra[k];
      });
    }
    this._warning("RTC identity: " + message, null, 0);
    let ev = new this._win.RTCPeerConnectionIdentityErrorEvent('idp' + type + 'error', args);
    this._dispatchEvent(ev);
  },

  _getFingerprintsFromSdp: function(sdp) {
    let fingerprints = {};
    let m = sdp.match(PeerConnectionIdp._fingerprintPattern);
    while (m) {
      fingerprints[m[0]] = { algorithm: m[1], digest: m[2] };
      sdp = sdp.substring(m.index + m[0].length);
      m = sdp.match(PeerConnectionIdp._fingerprintPattern);
    }

    return Object.keys(fingerprints).map(k => fingerprints[k]);
  },

  _getIdentityFromSdp: function(sdp) {
    // a=identity is session level
    let mLineMatch = sdp.match(PeerConnectionIdp._mLinePattern);
    let sessionLevel = sdp.substring(0, mLineMatch.index);
    let idMatch = sessionLevel.match(PeerConnectionIdp._identityPattern);
    if (idMatch) {
      let assertion = {};
      try {
        assertion = JSON.parse(atob(idMatch[1]));
      } catch (e) {
        this.reportError("validation",
                         "invalid identity assertion: " + e);
      } // for JSON.parse
      if (typeof assertion.idp === "object" &&
          typeof assertion.idp.domain === "string" &&
          typeof assertion.assertion === "string") {
        return assertion;
      }

      this.reportError("validation", "assertion missing" +
                       " idp/idp.domain/assertion");
    }
    // undefined!
  },

  /**
   * Queues a task to verify the a=identity line the given SDP contains, if any.
   * If the verification succeeds callback is called with the message from the
   * IdP proxy as parameter, else (verification failed OR no a=identity line in
   * SDP at all) null is passed to callback.
   */
  verifyIdentityFromSDP: function(sdp, origin) {
    let identity = this._getIdentityFromSdp(sdp);
    let fingerprints = this._getFingerprintsFromSdp(sdp);
    // it's safe to use the fingerprint we got from the SDP here,
    // only because we ensure that there is only one
    if (!identity || fingerprints.length <= 0) {
      return Promise.reject(new this._win.DOMException('No identity'));
    }

    this.setIdentityProvider(identity.idp.domain, identity.idp.protocol);
    return this._verifyIdentity(identity.assertion, fingerprints, origin);
  },

  /**
   * Checks that the name in the identity provided by the IdP is OK.
   *
   * @param error (function) an error function to call
   * @param name (string) the name to validate
   * @throws if the name isn't valid
   */
  _validateName: function(error, name) {
    if (typeof name !== "string") {
      error("name not a string");
    }
    let atIdx = name.indexOf("@");
    if (atIdx <= 0) {
      error("missing authority in name from IdP");
    }

    // no third party assertions... for now
    let tail = name.substring(atIdx + 1);

    // strip the port number, if present
    let provider = this.provider;
    let providerPortIdx = provider.indexOf(":");
    if (providerPortIdx > 0) {
      provider = provider.substring(0, providerPortIdx);
    }
    let idnService = Components.classes["@mozilla.org/network/idn-service;1"].
      getService(Components.interfaces.nsIIDNService);
    if (idnService.convertUTF8toACE(tail) !==
        idnService.convertUTF8toACE(provider)) {
      error('name "' + identity.name +
            '" doesn\'t match IdP: "' + this.provider + '"');
    }
  },

  /**
   * Check the validation response.  We are very defensive here when handling
   * the message from the IdP proxy.  That way, broken IdPs aren't likely to
   * cause catastrophic damage.
   */
  _checkVerifyResponse: function(validation, fingerprints) {
    let error = msg => {
      this._warning('RTC identity: assertion validation failure: ' + msg, null, 0);
      throw new this._win.DOMException('Identity assertion validation failure');
    };

    if (!validation || !validation.payload) {
      error('no payload in validation response');
    }

    let fingerprint = validation.payload.fingerprint;

    let isFingerprint = f => {
      return typeof f.digest === 'string' && f.algorithm === 'string';
    };
    if (!Array.isArray(contents.fingerprint) &&
        !fingerprint.every(isFingerprint)) {
      error("validation doesn't contain an array of fingerprints");
    }

    let isSubsetOf = (outer, inner, cmp) => {
      return inner.every(i => {
        return outer.some(o => cmp(i, o));
      });
    };
    let compareFingerprints = (a, b) => {
      return (a.digest === b.digest) && (a.algorithm === b.algorithm);
    };
    if (isSubsetOf(fingerprint, fingerprints, compareFingerprints)) {
      error("fingerprints in SDP aren't a subset of those in the assertion");
    }
    this._validateName(error, message.identity);
    return validation;
  },

  /**
   * Asks the IdP proxy to verify an identity assertion.
   */
  _verifyIdentity: function(assertion, fingerprints, origin) {
    let validationPromise = this._idp.start()
      .then(idp => idp.validateAssertion(assertion, origin))
      .then(validation => this._checkVerifyResponse(validation, fingerprints));
    return this._safetyNet("validation", validationPromise);
  },

  /**
   * Enriches the given SDP with an `a=identity` line.  getIdentityAssertion()
   * must have already run, otherwise this does nothing to the sdp.
   */
  addIdentityAttribute: function(sdp) {
    if (!this.assertion) {
      return sdp;
    }

    // yes, we assume that this matches; if it doesn't something is *wrong*
    let match = sdp.match(PeerConnectionIdp._mLinePattern);
    return sdp.substring(0, match.index) +
      "a=identity:" + this.assertion + "\r\n" +
      sdp.substring(match.index);
  },

  /**
   * Asks the IdP proxy for an identity assertion.
   */
  getIdentityAssertion: function(fingerprint) {
    if (!this.enabled) {
      throw new this._win.DOMException('InvalidStateError', 'IdP not set');
    }

    let [algorithm, digest] = fingerprint.split(" ", 2);
    let content = {
      fingerprint: [{
        algorithm: algorithm,
        digest: digest
      }]
    };
    let origin = Cu.getWebIDLCallerPrincipal().origin;
    let assertionPromise = this._idp.start()
      .then(idp => idp.generateAssertion(content, origin, this.username))
      .then(assertion => {
        if (!this._isValidAssertion(assertion)) {
          this._warning("RTC identity: assertion generation failure", null, 0);
          this.assertion = null;
        } else {
          // save the base64 + JSON assertion contents
          this.assertion = btoa(JSON.stringify(assertion));
        }
        return this.assertion;
      });

    return this._safetyNet('assertion', assertionPromise);
  },

  /**
   * Wraps a promise, adding a timeout guard on it so that it can't take longer
   * than the specified timeout.  This relies on the fact that p does not
   * resolve to an undefined value, since Promises don't expose any properties
   * that let us learn about their state.
   */
  _safetyNet: function(type, p) {
    return Promise.race([p, delay(this._timeout)])
      .then(r => {
        if (!r) {
          this.reportError(type, 'IdP timeout');
        }
      }).catch(e => {
        this.reportError(type, 'Error reported by IdP ' + e.message);
        throw e;
      });
  }
};

this.PeerConnectionIdp = PeerConnectionIdp;
