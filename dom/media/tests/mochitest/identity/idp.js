(function(global) {
  "use strict";

  // rather than create a million different IdP configurations and litter the
  // world with files all containing near-identical code, let's use the hash/URL
  // fragment as a way of generating instructions for the IdP
  var instructions = global.location.hash.replace("#", "").split(":");
  function is(target) {
    return function(instruction) {
      return instruction === target;
    };
  }

  function IDPJS() {
    this.domain = global.location.host;
    var p = global.location.pathname;
    this.protocol = p.substring(p.lastIndexOf('/') + 1) + global.location.hash;
    this.username = "someone@" + this.domain;
  }

  function borkResult(result) {
    if (instructions.some(is("throw"))) {
      throw new Error('Throwing!');
    }
    if (instructions.some(is("fail"))) {
      return Promise.reject(new Error('Failing!'));
    }
    if (instructions.some(is("hang"))) {
      return new Promise(r => {});
    }
    return Promise.resolve(result);
  };

  IDPJS.prototype = {
    _selectUsername: function(usernameHint) {
      var username = this.username;
      if (usernameHint) {
        var at = usernameHint.indexOf("@");
        if (at < 0) {
          username = usernameHint + "@" + this.domain;
        } else if (usernameHint.substring(at + 1) === this.domain) {
          username = usernameHint;
        }
      }
      return username;
    },

    generateAssertion: function(payload, origin, usernameHint) {
      var idpDetails = {
        domain: this.domain,
        protocol: this.protocol
      };
      if (instructions.some(is("bad-assert"))) {
        idpDetails = {};
      }
      return borkResult({
        idp: idpDetails,
        assertion: JSON.stringify({
          username: this._selectUsername(usernameHint),
          contents: payload
        })
      });
    },

    validateAssertion: function(assertion, origin) {
      var assertion = JSON.parse(assertion);
      if (instructions.some(is("bad-validate"))) {
        assertion.contents = {};
      }
      return borkResult({
          identity: assertion.username,
          contents: assertion.contents
        });
    }
  };

  if (!instructions.some(is("dont_register"))) {
    global.registerIdentityProvider(new IDPJS());
  }
}(this));
