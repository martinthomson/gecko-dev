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

/**
 * A proxy for the IdPWrapper in IdpChrome.jsm
 *
 * Looks like the real thing, but everything is message-passing
 */
function IdpChannel(id, url, sendMessage, onMessage, onFinalize) {
  this._id = id;
  this._url = url;
  this._send = sendMessage;
  this._message = onMessage;
  this._finalizer = onFinalize;
}

IdpChannel.prototype = {
  open: function(callback) {
    this._ready = callback;
    this._send(this._id, "CREATE", this._url);
  },

  send: function(message) {
    this._send(this._id, "MESSAGE", message);
  },

  close: function(callback) {
    this._closed = callback;
    this._send(this._id, "CLOSE");
  },

  MESSAGE: function(message) {
    this._message(message);
  },

  CREATED: function(message) {
    this._ready();
  },

  CREATE_ERROR: function(message) {
    this._finalizer();
    this._ready(message);
  },

  CLOSED: function(message) {
    this._finalizer();
    this._closed();
  }
};


/**
 * Something that tracks all the IdpChannel instances content-side.
 * This is a central point for all message passing too.
 */
function IdpProxyChannelManager() {
  this._channels = {};

  this._mm = Cc["@mozilla.org/childprocessmessagemanager;1"]
    .getService(Ci.nsIMessageSender);
  this._mm.addMessageListener("WebRTC:IdP", this);
}

IdpProxyChannelManager.prototype = {
  createChannel: function(url, messageCallback) {
    let id = this._generateUuid();

    let channel = new IdpChannel(id, url, this._send.bind(this),
                                 messageCallback, () => delete this._channels[id]);
    this._channels[id] = channel;
    return channel;
  },

  receiveMessage: function(message) {
    dump("Received at child: " + JSON.stringify(message));
    let channel = this._channels[message.data.id];
    if (channel && typeof channel[message.data.action] === "function") {
      channel[message.data.action](message.data.message);
    } else {
      throw new Error("WebRTC IdP (content) unhandled message: " +
                      JSON.stringify(message.data));
    }
  },

  _generateUuid: function() {
    if (!this.uuidGenerator) {
      this.uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator);
    }
    return this.uuidGenerator.generateUUID().toString();
  },

  _send: function(id, action, message) {
    let data = {
      id: id,
      action: action,
      message: message
    };
    dump("Sending from child: " + JSON.stringify(data));
    this._mm.sendAsyncMessage("WebRTC:IdP", data);
  }
};

/**
 * A message channel between the RTC PeerConnection and a designated IdP Proxy.
 *
 * @param domain (string) the domain to load up
 * @param protocol (string) Optional string for the IdP protocol
 */
function IdpProxy(domain, protocol) {
  IdpProxy.validateDomain(domain);
  IdpProxy.validateProtocol(protocol);

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
  } catch (e if (e.result === Cr.NS_ERROR_MALFORMED_URI)) {
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

IdpProxy.channelManager = new IdpProxyChannelManager();

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
   * Get a sandboxed iframe for hosting the idp-proxy's js. Create a message
   * channel down to the frame.
   *
   * @param errorCallback (function) a callback that will be invoked if there
   *                is a fatal error starting the proxy
   */
  start: function(errorCallback) {
    if (this.channel) {
      return;
    }
    let well_known = "https://" + this.domain;
    well_known += "/.well-known/idp-proxy/" + this.protocol;
    let manager = IdpProxy.channelManager;
    this.channel = manager.createChannel(well_known, this._messageReceived.bind(this));
    this.channel.open(error => {
      if (error) {
        this.close();
        if (typeof errorCallback === "function") {
          errorCallback(error);
        }
      }
    });
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
    dump("Message from IdP: " + JSON.stringify(message));
    if (!this.ready && message.type === "READY") {
      this.ready = true;
      this.pending.forEach(p => this.send(p.message, p.callback));
      this.pending = [];
    } else if (this.tracking[message.id]) {
      var callback = this.tracking[message.id];
      delete this.tracking[message.id];
      callback(message);
    } else {
      let console = Cc["@mozilla.org/consoleservice;1"].
        getService(Ci.nsIConsoleService);
      console.logStringMessage("WebRTC Identity: Received bad message from IdP: " +
                               message.id + ":" + message.type);
    }
  },

  /**
   * Performs cleanup.  The object should be OK to use again.
   */
  close: function() {
    if (!this.channel) {
      return;
    }

    // clear out before letting others know in case they do something bad
    let trackingCopy = this.tracking;
    let pendingCopy = this.pending;

    this.channel.close();
    this._reset();

    // dump a message of type "ERROR" in response to all outstanding
    // messages to the IdP
    let error = { type: "ERROR", message: "IdP closed" };
    Object.keys(trackingCopy).forEach(k => this.trackingCopy[k](error));
    pendingCopy.forEach(p => p.callback(error));
  },

  toString: function() {
    return this.domain + '/.../' + this.protocol;
  }
};

this.IdpProxy = IdpProxy;
