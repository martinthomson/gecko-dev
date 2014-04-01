/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["webrtcIdpManager"];

const {
  classes: Cc,
  interfaces: Ci,
  utils: Cu,
  results: Cr
} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Sandbox",
                                  "resource://gre/modules/identity/Sandbox.jsm");

/**
 * An invisible iframe for hosting the idp shim.
 *
 * There is no visible UX here, as we assume the user has already
 * logged in elsewhere (on a different screen in the web site hosting
 * the RTC functions).
 */
function IdpSandbox(uri, messageCallback, targetBrowser) {
  this.sandbox = null;
  this.messagechannel = null;
  this.source = uri;
  this.messageCallback = messageCallback;
}

IdpSandbox.prototype = {
  /**
   * Create a hidden, sandboxed iframe for hosting the IdP's js shim.
   *
   * @param callback
   *                (function) invoked when this completes, with an error
   *                argument if there is a problem, no argument if everything is
   *                ok
   */
  open: function(callback) {
    if (this.sandbox) {
      return callback(new Error("IdP wrapper already open"));
    }

    let ready = this._sandboxReady.bind(this, callback);
    this.sandbox = new Sandbox(this.source, ready);
  },

  _sandboxReady: function(aCallback, aSandbox) {
    // Inject a message channel into the subframe.
    try {
      this.messagechannel = new aSandbox._frame.contentWindow.MessageChannel();
      Object.defineProperty(
        aSandbox._frame.contentWindow.wrappedJSObject,
        "rtcwebIdentityPort",
        {
          value: this.messagechannel.port2
        }
      );
    } catch (e) {
      this.close();
      aCallback(e); // oops, the IdP proxy overwrote this.. bad
      return;
    }
    this.messagechannel.port1.onmessage = msg => {
      this.messageCallback(msg.data);
    };
    this.messagechannel.port1.start();
    aCallback();
  },

  send: function(msg) {
    this.messagechannel.port1.postMessage(msg);
  },

  close: function() {
    if (this.sandbox) {
      if (this.messagechannel) {
        this.messagechannel.port1.close();
      }
      this.sandbox.free();
    }
    this.messagechannel = null;
    this.sandbox = null;
  }
};

function IdpSandboxManager() {
  this._wrappers = {};

  let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
    .getService(Ci.nsIMessageListenerManager);
  ppmm.addMessageListener("WebRTC:IdP", this);
  ppmm.addMessageListener("child-process-shutdown", this);
}

IdpSandboxManager.prototype = {

  /**
   * Over the e10s message manager we get { name:, target:, data: }
   * Within 'data', we have another protocol with the following fields:
   *   id: a unique identifier for the instance (sadly, selected by the content side)
   *   action: the type of action, which maps to a method name here
   *   message: data that goes along with this, contents of which depend on type
   */
  receiveMessage: function(message) {
    dump("Received at the parent: " + JSON.stringify(message) + "\n");
    if (message.name === "child-process-shutdown") {
      this._cleanup(message.target);
    } else if (typeof this[message.data.action] === "function") {
      this[message.data.action](message.target, message.data.id, message.data.message);
    } else {
      // blow up
      throw new Error("WebRTC IdP handler (chrome) received unknown message type");
    }
  },

  /**
   * When a process shuts down, we need to make doubly certain that all the
   * sandboxes are removed.  This collection uses weak references, so they
   * shouldn't leak if everything goes well, only cleanup if things go poorly.
   */
  _cleanup: function(caller) {
    dump("Shutting down: " + JSON.stringify(caller));
    Object.keys(caller.idpSandboxes).forEach(id => {
      let wrapper = caller.idpSandboxes[id].get();
      if (wrapper) {
        wrapper.close();
      }
    });
    delete caller.idpSandboxes;
  },

  /**
   * Create an instance of the IdpSandbox and initialize it.
   * Signals "CREATED" when everything is established;
   * "CREATE_ERROR" indicates an error.
   *
   * Messages cannot be sent until "CREATED" is signaled because nothing is
   * hooked up until then (a "MESSAGE" containing "READY" is a stronger signal
   * because that's end-to-end).  Though unlikely in practice, it's possible to
   * receive a "MESSAGE" before receiving "CREATED".
   */
  CREATE: function(caller, id, url) {
    if (this._wrappers[id]) {
      throw new Error("WebRTC IdP (chrome) encountered an ID collision");
    }

    let wrapper = new IdpSandbox(url, message => {
      this._send(caller, id, "MESSAGE", message);
    });
    wrapper.open(e => {
      if (e) {
        wrapper.close();
        this._send(caller, id, "CREATE_ERROR", e);
      } else {
        this._wrappers[id] = wrapper;
        if (!caller.idpSandboxes) {
          caller.idpSandboxes = {};
        }
        caller.idpSandboxes[id] = Cu.getWeakReference(wrapper);
        dump("created in: " + JSON.stringify(caller) + "\n");
        this._send(caller, id, "CREATED");
      }
    });
  },

  /**
   * Forward a message to the IdpSandbox.
   */
  MESSAGE: function(caller, id, data) {
    if (!this._wrappers[id]) {
      throw new Error("WebRTC IdP (chrome) message directed nowhere");
    }

    this._wrappers[id].send(data);
  },

  /**
   * Close the IdpSandbox and allow it to be reaped.
   */
  CLOSE: function(caller, id) {
    if (!this._wrappers[id]) {
      return; // that's OK, close can be idempotent
    }

    this._wrappers[id].close();
    delete this._wrappers[id];
    delete caller.idpSandboxes[id];
    this._send(caller, id, "CLOSED");
  },

  _send: function(target, id, action, message) {
    let data = {
      id: id,
      action: action,
      message: message
    };
    dump("Sending in the parent: " + JSON.stringify(data));
    let sender = target.QueryInterface(Ci.nsIMessageSender);
    sender.sendAsyncMessage("WebRTC:IdP", data);
  }
};

this.webrtcIdpManager = new IdpSandboxManager();
