(function main_default_js(){
// Note: the names of anonymous functions are not required.
//   e.g. var x = function some_name(){...};
//   would work just as well as: var x = function(){...};
// However, the names are used at stack traces on errors.
// So where it's unlikely to have errors - names are omitted.
// (the filename and line number will still show on traces
//  but the function name will be empty).

/**********************************************************************
 *  CommonJS style module/require

 Spec: http://wiki.commonjs.org/wiki/Modules/1.1.1

 The code tries to follow the spec enough to be useful, but implements
 somewhere between the spec and the node.js implementation (where require
 id can map to file paths and as such considered as relative also for a/b
 style paths where the spec says that a/b should be considered absolute
 because it doesn't start with . or ..).

 Reasonably implemented:
 - Require - only the mandatory requirements.
 - Module Context - everything.
 - Module Identifiers - so so. internally it normalizes all relative id's
   to start with . or .., but has relaxed interpretation where a/b style
   id "path" will be normalized to ./a/b (derived from OS path interpretation).
   Also, there's no "conceptual module namespcae root" - relative are resolved
   relative to the script's path and absolute resolve to filesystem paths.
 *********************************************************************/
mp._modules = {cache: {}, stack: []};
function new_module(id, uri, _isFirst) {
  if (!_isFirst)
    mp._modules.stack.push(module);
  module = {id: id, uri: uri, exports: {}};
  exports = module.exports;
  return module;
}

function done_module() {
  module = mp._modules.stack.pop();
  exports = module.exports;
}

// Mechanical, as if we never go ../ from symlinks. Not fully tested.
// For full symlinks compliance, we'd need to use OS normalization.
// relative result will always start with either ./ or ../
function normalize_id(id) {
  var path = id.split("\\").join("/").split("/");
  var isRel = !(path[0] == "" || path[0] == "~" || path[0][1] == ":");
  var mustKeep = isRel ? 0 : 1;

  var res = [];
  for (var i = 0; i < path.length; i++) {
    var last = res.length ? res[res.length - 1] : undefined;
    if (res.length >= mustKeep && (path[i] == "." || path[i] == "" && i < path.length - 1))
      continue;
    if (path[i] == ".." && res.length > mustKeep && last != "..") {
      res.pop();
      continue;
    }
    res.push(path[i]);
  }

  if (isRel && res[0] != ".." && res[0] != ".")
    res.unshift(".");
  return res.join("/");
}

// return normalized id (if relative, then to current module.id)
function resolve_id(id) {
  var res = mp.utils.join_path(mp.utils.split_path(module.id)[0], id);
  // Must normalize to support cyclic referencing with relative 'require' id,
  // but currently has a cost where going upwards from symlinks might break.
  return normalize_id(res);
}

// not normalized, symlinks work.
function resolve_uri(id) {
  if (id[0] == "@") // internal file - don't normalize
    return id + ".js";
  return mp.utils.join_path(mp.utils.split_path(module.uri)[0], id + ".js");
}

// Supports relative id, symlinks and cyclic referencing (inc. of relative id),
// broken: cyclic references which go ../ from symlinks link point might
//         load from disk as a new module despite being cached already.
require = function require(id) {
  var orig_id = String(id);
  id = resolve_id(id);
  if (mp._modules.cache[id])
    return mp._modules.cache[id].exports;

  mp._modules.cache[id] = new_module(id, resolve_uri(orig_id));
  try {
    mp.utils.run_file(module.uri);
  } catch (e) {
    delete mp._modules.cache[id];
    done_module();
    throw(e);
  }
  done_module();
  return mp._modules.cache[id].exports;
}
new_module("main", mp.script_path, true);


/**********************************************************************
 *  timers: standard DOM-style set/clear Timeout/Interval/Immediate
 *          - extra arguments at set* are not supported.
 *********************************************************************/
var nextTimerId = 1;
var timers = [];
var canceledTimers = false; // or an object of id's to be cancelled with at least one item
var processingTimers = false;

function insertTimerObj(timer) {
  if (processingTimers) {
    timers.push(timer); // timers will get merged soon, no need to sort twice.
  } else {
    // Insert the timer, (inversely) ordered by timer.when and insertion order
    // soonest-to-expire timers are at the end such they're quicker to handle.
    for (var i = timers.length - 1; i >= 0 && timer.when >= timers[i].when; i--)
      {}

    timers.splice(i + 1, 0, timer);
  }
  return timer.id;
}

var MIN_TIMEOUT_MS = 4; // must be positive (possibly fractional). 4ms is common in browsers
// assumes values are valid
function addTimer(callback, duration, isRepeat) {
  var timer = {
    id: nextTimerId++,
    when: duration ? mp.get_time_ms() + duration : 0,
    callback: callback,
  };

  if (isRepeat)
    timer.interval = duration;

  return insertTimerObj(timer);
}

// Immediate callbacks will execute as soon as possible when the event queue is empty
setImmediate = function(fn) {
  return addTimer(fn, 0);
}

// timeouts and intervals are clipped to a minimum of 4 ms
setTimeout = function(fn, duration) {
  return addTimer(fn, Math.max(MIN_TIMEOUT_MS, duration));
}

setInterval = function(fn, interval) {
  return addTimer(fn, Math.max(MIN_TIMEOUT_MS, interval), true);
}

// Cancelled timers will get applied lazily during the next processTimers().
clearTimeout = clearInterval = clearImmediate = function(id) {
  if (!canceledTimers)
    canceledTimers = {};
  canceledTimers[id] = true;
}

// called once per event loop.
// executes all due pending timer handlers (_not_ timers which were added during this iteration)
// and which were not cancelled during this iteration.
// returns the wait in ms till the next timer handler (possibly 0) or -1 if no pending timers.
function processTimers(allowImmediate) {
  if (!timers.length) {
    if (canceledTimers)
      canceledTimers = false;
    return -1;
  }

  // don't process timers which were added during the callbacks.
  var current = timers;
  timers = [];
  var hadImmediate = false;
  processingTimers = true;

  var i;
  for (i = current.length - 1; i >= 0; i--) {
    var timer = current[i];
    if (timer.when && timer.when > mp.get_time_ms())
      break;

    // don't process timers which were cancelled during the callbacks
    if (canceledTimers && canceledTimers[timer.id])
      continue;

    var execute = true;
    if (!timer.when) { // immediate - allow at most one per processTimers()
      if (allowImmediate && !hadImmediate)
        hadImmediate = true;
      else
        execute = false;
    }

    if (execute) {
      current.splice(i, 1);
      try {
        timer.callback();
      } catch (e) {
        mp.msg.fatal(e);
      }
    }

    if (timer.interval) {
      // TODO?: uncongest if ((now - when) > SOME_LIMIT * interval)
      timer.when += timer.interval;
      insertTimerObj(timer);
    }
  }
  // done executing whatever was pending. Now restore order.
  // restore pending timers and merge newly added timers.

  // remove cancelled from the timers we didn't process.
  if (canceledTimers) {
    for (i = current.length - 1; i >= 0; i--)
      if (canceledTimers[current[i].id])
        current.splice(i, 1);
  }

  var newlyAdded = timers;
  timers = current; // pending but not due
  processingTimers = false;

  for (i = 0; i < newlyAdded.length ; i++)
    if (!canceledTimers || !canceledTimers[newlyAdded[i].id])
      insertTimerObj(newlyAdded[i]);

  canceledTimers = false;

  //mp.msg.fatal("timers pending: " + timers.length);
  return !timers.length ? -1 : Math.max(0, timers[timers.length - 1].when - mp.get_time_ms());
}


/**********************************************************************
 *  event handlers
 *********************************************************************/
var ehandlers = {}; // with event names as keys and array of callbacks for each

mp.register_event = function(name, fn) {
  if (!ehandlers[name]) {
    var rv = mp._request_event(name, true);
    if (!rv)
      return rv;

    ehandlers[name] = [];
  }
  ehandlers[name].push(fn);
  return true;
}

mp.unregister_event = function(fn) {
  for (var name in ehandlers) {
    for (var i = ehandlers[name].length - 1; i >= 0; i--) {
      if (ehandlers[name][i] == fn)
        ehandlers[name].splice(i, 1);
    }

    if (!ehandlers[name].length) {
      delete ehandlers[name];
      mp._request_event(name, false);
    }
  }
  return true;
}


// separated function because functions with try/catch are slower to invoke
function dispatch_event_name(e, handlers) {
  for (var i in handlers)
    try {
      handlers[i](e);
    } catch (ex) {
      mp.msg.fatal(ex);
    }
}

function dispatch_event(e) {
  var handlers = ehandlers[e.event];
  if (handlers)
    dispatch_event_name(e, handlers);
}


/**********************************************************************
 *  property observers
 *********************************************************************/
var ohandlers = {nextId: 1, callbacks: {}};

mp.observe_property = function(name, format, fn) {
  format = (mp._formats[format]);
  if (typeof format == "undefined") {
    mp.last_error_string = "unknown format";
    return undefined;
  }
  var id = ohandlers.nextId++;
  if (!mp._observe_property(id, name, format))
    return undefined;
  ohandlers.callbacks[id] = fn;
  return id;
}

mp.unobserve_property = function(fn) {
  var unobserved = false;
  for (var id in ohandlers.callbacks)
    if (ohandlers.callbacks[id] == fn) {
      delete ohandlers.callbacks[id];
      unobserved = true;
      mp._unobserve_property(id);
    }
  return unobserved;
}

mp.unobserve_property_id = function(id) {
  delete ohandlers.callbacks[id];
  return mp._unobserve_property(id);
}

function notifyObserver(e) {
  var observer = ohandlers.callbacks[e.id];
  if (!observer)
    return; // probably the event was sent/queued before cancelled.

  try {
    observer(e.name, e.data);
  } catch (ex) {
    mp.msg.fatal(ex);
  }
}


/**********************************************************************
 *  Key bindings and client messages
 *********************************************************************/
var messages = {};

mp.register_script_message = function(name, fn) {
    messages[name] = fn;
}

mp.unregister_script_message = function(name) {
    delete messages[name];
}

function message_dispatch(ev) {
  if (ev.args.length) {
    var handler = messages[ev.args[0]];
    if (handler)
      handler.apply(this, ev.args.slice(1));
  }
}

var dispatch_key_bindings = {};

var message_id = 0;
function reserve_binding() {
    return "__keybinding" + (++message_id);
}

function dispatch_key_binding(name, state) {
  var fn = dispatch_key_bindings[name];
  if (fn)
    fn(name, state);
}

var key_bindings = {};

function update_key_bindings() {
  for (var i = 0; i < 2; i++) {
    var section, flags;
    var def = i == 0;
    if (def) {
      section = "input_" + mp.script_name;
      flags = "default";
    } else {
      section = "input_forced_" + mp.script_name;
      flags = "force";
    }
    var cfg = "";
    for (var name in key_bindings) {
      var attrs = key_bindings[name];
      if (attrs.forced != def)
        cfg = cfg + attrs.bind + "\n";
    }
    mp.input_define_section(section, cfg, flags);
    // TODO: remove the section if the script is stopped
    mp.input_enable_section(section, "allow-hide-cursor|allow-vo-dragging");
  }
}

function add_binding(attrs, key, name, fn, rp) {
  var named = true;
  if (typeof name != "string") {
    // shift args left
    rp = fn;
    fn = name || function() {};
    name = reserve_binding();
    named = false;
  }

  rp = rp || "";
  var bind = key;
  var repeatable = rp == "repeatable" || rp["repeatable"];
  if (rp["forced"])
    attrs.forced = true;

  var key_cb, msg_cb;
  if (rp["complex"]) {
    var key_states = {
      u: "up",
      d: "down",
      r: "repeat",
      p: "press"
    };
    key_cb = function key_cb(name, state) {
      fn({
        event: key_states[state[0]] || "unknown",
        is_mouse: state[1] == "m"
      });
    }
    msg_cb = function msg_cb() {
      fn({event: "press", is_mouse: false});
    }
  } else {
    key_cb = function key_cb(name, state) {
      // Emulate the same semantics as input.c uses for most bindings:
      // For keyboard, "down" runs the command, "up" does nothing;
      // for mouse, "down" does nothing, "up" runs the command.
      // Also, key repeat triggers the binding again.
      var event = state[0];
      var is_mouse = state[1] == "m";
      if (event == "r" && !repeatable)
        return;
      if (is_mouse && (event == "u" || event == "p"))
        fn();
      else if (!is_mouse && (event == "d" || event == "r" || event == "p"))
        fn();
    }
    msg_cb = fn;
  }

  attrs.bind = bind + " script_binding " + mp.script_name + "/" + name;
  attrs.name = name;
  key_bindings[name] = attrs;
  update_key_bindings();
  dispatch_key_bindings[name] = key_cb;
  if (named)
    mp.register_script_message(name, msg_cb);
}

mp.add_key_binding = function add_key_binding(a, b, c, d) {
  add_binding({forced: false}, a, b, c, d);
}

mp.add_forced_key_binding = function add_forced_key_binding(a, b, c, d) {
  add_binding({forced: true}, a, b, c, d);
}

mp.remove_key_binding = function(name) {
  delete key_bindings[name];
  update_key_bindings();
  delete dispatch_key_bindings[name];
  mp.unregister_script_message(name);
}


/**********************************************************************
 *  hooks
 *********************************************************************/
var hooks = [];
var hook_registered = false;

function hook_run(id, cont) {
  var fn = hooks[id];
  if (fn)
    fn();
  mp.commandv("hook_ack", cont);
}

mp.add_hook = function add_hook(name, pri, cb) {
  if (!hook_registered) {
    mp.register_script_message("hook_run", hook_run);
    hook_registered = true;
  }
  hooks.push(cb);
  mp.commandv("hook_add", name, hooks.length - 1, pri);
}


/**********************************************************************
 *  various
 *********************************************************************/
print = mp.msg.info;

// Currently (2014-12-13) replacer is ignored on mujs' JSON.stringify
// but will work if the user 'require's another JSON implementation
function replacer(k, v) {
  var t = typeof v;
  if (t == "function" || t == "undefined")
    return "<" + t + ">";
  return v;
}

function JSON_stringify_any(x, space) {
  if (typeof x == "undefined")
    return "undefined";

  return JSON.stringify(x, replacer, space);
}

dump = function dump(x, name) {
  name = name ? "" + name + ": " : "";
  print(name + JSON_stringify_any(x, 2));
}

mp.get_script_name = function() {
  return mp.script_name;
}

mp.get_opt = function(key, def) {
  var opts = mp.get_property_native("options/script-opts");
  var val = opts[key];
  if (typeof val == "undefined")
    val = def;
  return val;
}

mp.osd_message = function osd_message(text, duration) {
  if (isNaN(duration))
    duration = -1;
  else
    duration = Math.floor(duration * 1000);
  mp.commandv("show_text", text, duration);
}


/**********************************************************************
 *  main listeners and event loop
 *********************************************************************/
mp.keep_running = true;
mp.register_event("shutdown", function() { mp.keep_running = false; });
mp.register_event("property-change", notifyObserver);
mp.register_event("client-message", message_dispatch);
mp.register_script_message("key-binding", dispatch_key_binding);

mp_event_loop = function mp_event_loop() {
  var e = {};
  while(mp.keep_running) {
    var wait = processTimers(e.event == "none");
    e = mp.wait_event(wait / 1000);
    dispatch_event(e);
  }
};

})()
