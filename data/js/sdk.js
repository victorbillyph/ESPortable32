(function() {
  'use strict';

  var inIframe = window !== window.parent;
  var origin = window.location.origin;
  var ready = false;
  var readyCallbacks = [];
  var eventHandlers = {};
  var requestId = 0;
  var pending = {};

  function generateId() {
    return 'rpc_' + (++requestId) + '_' + Date.now();
  }

  function handleMessage(event) {
    if (event.origin !== origin && inIframe) return;
    var msg = event.data;
    if (!msg || !msg._esp) return;

    if (msg._type === 'response') {
      var cb = pending[msg._id];
      if (cb) {
        delete pending[msg._id];
        cb(msg._error, msg._data);
      }
      return;
    }

    if (msg._type === 'event') {
      var handlers = eventHandlers[msg._event];
      if (handlers) {
        for (var i = 0; i < handlers.length; i++) {
          handlers[i](msg._data);
        }
      }
      return;
    }

    if (msg._type === 'ready') {
      ready = true;
      for (var j = 0; j < readyCallbacks.length; j++) {
        readyCallbacks[j]();
      }
      readyCallbacks = [];
    }
  }

  function post(msg) {
    if (inIframe) {
      window.parent.postMessage(msg, origin);
    }
  }

  function sendRequest(method, path, body) {
    return new Promise(function(resolve, reject) {
      var id = generateId();
      var msg = {
        _esp: true,
        _type: 'request',
        _id: id,
        method: method,
        path: path,
        body: body
      };

      if (inIframe) {
        pending[id] = function(err, data) {
          if (err) reject(new Error(err));
          else resolve(data);
        };
        post(msg);
      } else {
        var opts = {
          method: method,
          headers: { 'Content-Type': 'application/json' }
        };
        if (body) opts.body = JSON.stringify(body);

        fetch(path, opts)
          .then(function(r) { return r.json(); })
          .then(resolve)
          .catch(reject);
      }
    });
  }

  if (inIframe) {
    window.addEventListener('message', handleMessage);
    post({ _esp: true, _type: 'init' });
  } else {
    ready = true;
    setTimeout(function() {
      for (var k = 0; k < readyCallbacks.length; k++) {
        readyCallbacks[k]();
      }
      readyCallbacks = [];
    }, 0);
  }

  var SDK = {
    api: {
      get: function(path) { return sendRequest('GET', path); },
      post: function(path, body) { return sendRequest('POST', path, body); }
    },
    onReady: function(cb) {
      if (ready) cb();
      else readyCallbacks.push(cb);
    },
    getStatus: function() {
      return sendRequest('GET', '/api/status');
    },
    on: function(name, cb) {
      if (!eventHandlers[name]) eventHandlers[name] = [];
      eventHandlers[name].push(cb);
      return function() {
        var idx = eventHandlers[name].indexOf(cb);
        if (idx > -1) eventHandlers[name].splice(idx, 1);
      };
    },
    emit: function(name, data) {
      post({
        _esp: true,
        _type: 'event',
        _event: name,
        _data: data
      });
    },
    close: function() {
      post({ _esp: true, _type: 'close' });
    },
    setTitle: function(title) {
      post({
        _esp: true,
        _type: 'setTitle',
        title: title
      });
    }
  };

  window.ESPortableSDK = SDK;
})();
