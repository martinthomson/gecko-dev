/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["IdpProxy"];

const {
  classes: Cc,
  interfaces: Ci,
  utils: Cu,
  results: Cr
} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/** This little function ensures that redirects maintain an https:// origin */
function RedirectHttpsOnly() {}
RedirectHttpsOnly.prototype = {
  asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
    if (newChannel.URI.scheme !== 'https') {
      throw Cr.NS_ERROR_ENTITY_CHANGED;
    }
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink])
};

function IdpLoaderStreamListener(res, rej) {
  this.resolve = res;
  this.reject = rej;
  this.data = '';
}
IdpLoaderStreamListener.prototype = {
  onDataAvailable: function(request, context, input, offset, count) {
    let stream = Cc["@mozilla.org/scriptableinputstream;1"]
      .createInstance(Ci.nsIScriptableInputStream);
    stream.init(input);
    this.data += stream.read(count)
  },

  onStartRequest: function (request, context) {},

  onStopRequest: function(request, context, status) {
    if (Components.isSuccessCode(status)) {
      this.resolve({ request: request, data: this.data });
    } else {
      this.reject(new Error('Load failed: ' + status));
    }
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStreamListener])
};

/**
 * Creates an object that is superficially equivalent to window.location
 * for the sandboxed code to look at.
 */
function createLocationFromURI(uri) {
  return {
    href: uri.spec,
    protocol: uri.scheme + ':',
    host: uri.host + ((uri.port !== 443) ?
                      (':' + uri.port) : ''),
    port: uri.port,
    hostname: uri.host,
    pathname: uri.path.replace(/[#\?].*/, ''),
    search: uri.path.replace(/^[^\?]*/, '').replace(/#.*/, ''),
    hash: uri.hasRef ? ('#' + uri.ref) : '',
    origin: uri.prePath,
    toString: function() {
      return uri.spec;
    }
  };
}

/**
 * An invisible iframe for hosting the idp shim.
 *
 * There is no visible UX here, as we assume the user has already
 * logged in elsewhere (on a different screen in the web site hosting
 * the RTC functions).
 *
 * @param win (object) the hosting window
 * @param uri (nsIURI) URI to load
 * @param name (string) name of the worker
 * @param messageCallback (function) callback to invoke on the arrival
 *     of messages from the IdP
 */
function IdpSandbox(win, uri, messageCallback) {
  this.window = win;
  this.active = false;
  this.sandbox = null;
  this.source = uri;
  this.messageCallback = messageCallback;
}

IdpSandbox.prototype = {
  /**
   * Create a sandbox for hosting the IdP's js shim.
   * @return a promise that resolves when the sandbox opens
   */
  open: function() {
    if (this.active) {
      throw new Error("IdP channel already active");
    }
    this.active = true;

    return new Promise((resolve, reject) => this._load(resolve, reject))
      .then(result => this._createSandbox(result));
  },

  _load: function(resolve, reject) {
    let listener = new IdpLoaderStreamListener(resolve, reject);
    let ioService = Cc['@mozilla.org/network/io-service;1']
      .getService(Ci.nsIIOService);
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    let ioChannel = ioService.newChannelFromURI2(this.source, null, systemPrincipal,
                                                 systemPrincipal, 0, 2);
    ioChannel.notificationCallbacks = new RedirectHttpsOnly();
    ioChannel.asyncOpen(listener, null);
  },

  _createSandbox: function(result) {
    let principal = Services.scriptSecurityManager
      .getChannelResultPrincipal(result.request);

    this.sandbox = Cu.Sandbox(principal, {
      sandboxName: 'WebRTC-IdP-' + this.source.host,
      wantXrays: false,
      wantComponents: false,
      wantExportHelpers: false,
      wantGlobalProperties: [
        'indexedDB', 'XMLHttpRequest', 'TextEncoder', 'TextDecoder',
        'URL', 'URLSearchParams', 'atob', 'btoa', 'Blob', 'File'
      ]
    });

     // this relies on getting an xray on the window
    let msgChannel = new this.window.MessageChannel();
    this.port = msgChannel.port1;
    this.port.addEventListener('message', e => this.messageCallback(e.data));
    this.sandbox.webrtcIdentityPort = Cu.cloneInto(msgChannel.port2,
                                                   this.sandbox,
                                                   {
                                                     wrapReflectors: true,
                                                     cloneFunctions: true
                                                   });

    // a minimal set of extra tools; it is easier to add more, than take back
    this.sandbox.location = Cu.cloneInto(createLocationFromURI(this.source),
                                         this.sandbox,
                                         { cloneFunctions: true });
    this.sandbox.setTimeout = this.window.setTimeout;
    this.sandbox.clearTimeout = this.window.clearTimeout;


    // putting a javascript version of 1.8 here seems fragile
    Cu.evalInSandbox(result.data, this.sandbox,
                     '1.8', result.request.URI.spec, 1);
  },

  send: function(msg) {
    dump('browser>' + JSON.stringify(msg) + '\n');
    this.port.postMessage(msg);
  },

  close: function() {
    if (this.sandbox) {
      Cu.nukeSandbox(this.sandbox);
    }
    this.sandbox = null;
    this.port = null;
    this.active = false;
  }
};

/**
 * A message channel between the RTC PeerConnection and a designated IdP Proxy.
 *
 * @param win (object) the hosting window
 * @param domain (string) the domain to load up
 * @param protocol (string) Optional string for the IdP protocol
 */
function IdpProxy(win, domain, protocol) {
  IdpProxy.validateDomain(domain);
  IdpProxy.validateProtocol(protocol);

  this.window = win;
  this.domain = domain;
  this.protocol = protocol || "default";

  this._reset();
}

/**
 * Checks that the domain is only a domain, and doesn't contain anything else.
 * Adds it to a URI, then checks that it matches perfectly.
 */
IdpProxy.validateDomain = function(domain) {
  let message = "Invalid domain for identity provider; ";
  if (!domain || typeof domain !== "string") {
    throw new Error(message + "must be a non-zero length string");
  }

  message += "must only have a domain name and optionally a port";
  try {
    let ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
    let uri = ioService.newURI('https://' + domain + '/', null, null);

    // this should trap errors
    // we could check uri.userPass, uri.path and uri.ref, but there is no need
    if (uri.hostPort !== domain) {
      throw new Error(message);
    }
  } catch (e if (typeof e.result !== "undefined" &&
                 e.result === Cr.NS_ERROR_MALFORMED_URI)) {
    throw new Error(message);
  }
};

/**
 * Checks that the IdP protocol is sane.  In particular, we don't want someone
 * adding relative paths (e.g., "../../myuri"), which could be used to move
 * outside of /.well-known/ and into space that they control.
 */
IdpProxy.validateProtocol = function(protocol) {
  if (!protocol) {
    return;  // falsy values turn into "default", so they are OK
  }
  let message = "Invalid protocol for identity provider; ";
  if (typeof protocol !== "string") {
    throw new Error(message + "must be a string");
  }
  if (decodeURIComponent(protocol).match(/[\/\\]/)) {
    throw new Error(message + "must not include '/' or '\\'");
  }
};

IdpProxy.prototype = {
  _reset: function() {
    this.channel = null;
    this.ready = false;

    this.counter = 0;
    this.tracking = {};
    this.pending = [];
  },

  isSame: function(domain, protocol) {
    return this.domain === domain && ((protocol || "default") === this.protocol);
  },

  /**
   * Start the IdP proxy.  This completes asynchronously, but the object is
   * immediately usable because we enqueue messages until the proxy indicates
   * that it's ready.
   */
  start: function() {
    if (this.channel) {
      return;
    }
    let well_known = "https://" + this.domain;
    well_known += "/.well-known/idp-proxy/" + this.protocol;
    let ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
    let uri = ioService.newURI(well_known, null, null);
    this.channel = new IdpSandbox(this.window, uri,
                                  this._messageReceived.bind(this));
    this.channel.open();
  },

  /**
   * Send a message up to the idp proxy. This should be an RTC "SIGN" or
   * "VERIFY" message. This method adds the tracking 'id' parameter
   * automatically to the message so that the callback is only invoked for the
   * response to the message.
   *
   * This enqueues the message to send if the IdP hasn't signaled that it is
   * "READY", and sends the message when it is.
   *
   * The caller is responsible for ensuring that a response is received. If the
   * IdP doesn't respond, the callback simply isn't invoked.
   */
  send: function(message, callback) {
    this.start();
    if (this.ready) {
      message.id = "" + (++this.counter);
      this.tracking[message.id] = callback;
      this.channel.send(message);
    } else {
      this.pending.push({ message: message, callback: callback });
    }
  },

  /**
   * Handle a message from the IdP. This automatically sends if the message is
   * 'READY' so there is no need to track readiness state outside of this obj.
   */
  _messageReceived: function(message) {
    if (!message) {
      return;
    }
    if (!this.ready && message.type === "READY") {
      this.ready = true;
      this.pending.forEach(p => {
        this.send(p.message, p.callback);
      });
      this.pending = [];
    } else if (this.tracking[message.id]) {
      var callback = this.tracking[message.id];
      delete this.tracking[message.id];
      callback(message);
    } else {
      let console = Cc["@mozilla.org/consoleservice;1"].
        getService(Ci.nsIConsoleService);
      console.logStringMessage("Received bad message from IdP: " +
                               message.id + ":" + message.type);
    }
  },

  /**
   * Performs cleanup.  The object should be OK to use again.
   */
  stop: function() {
    if (!this.channel) {
      return;
    }

    // clear out before letting others know in case they do something bad
    let trackingCopy = this.tracking;
    let pendingCopy = this.pending;

    this.channel.close();
    this._reset();

    // generate a message of type "ERROR" in response to all outstanding
    // messages to the IdP
    let error = { type: "ERROR", error: "IdP closed" };
    Object.keys(trackingCopy).forEach(k => trackingCopy[k](error));
    pendingCopy.forEach(p => p.callback(error));
  },

  toString: function() {
    return this.domain + '/.../' + this.protocol;
  }
};

this.IdpProxy = IdpProxy;
