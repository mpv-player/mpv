"use strict";
(function main_default_js(g) {
// - g is the global object.
// - User callbacks called without 'this', global only if callee is non-strict.
// - The names of function expressions are not required, but are used in stack
//   traces. We name them where useful to show up (fname:#line always shows).

mp.msg = { log: mp.log };
mp.msg.verbose = mp.log.bind(null, "v");
var levels = ["fatal", "error", "warn", "info", "debug", "trace"];
levels.forEach(function(l) { mp.msg[l] = mp.log.bind(null, l) });

// same as {} but without inherited stuff, e.g. o["toString"] doesn't exist.
// used where we try to fetch items by keys which we don't absolutely trust.
function new_cache() {
    return Object.create(null, {});
}

/**********************************************************************
 *  event handlers, property observers, idle, client messages, hooks, async
 *********************************************************************/
var ehandlers = new_cache() // items of event-name: array of {maybe cb: fn}

mp.register_event = function(name, fn) {
    if (!ehandlers[name])
        ehandlers[name] = [];
    ehandlers[name] = ehandlers[name].concat([{cb: fn}]);  // replaces the arr
    return mp._request_event(name, true);
}

mp.unregister_event = function(fn) {
    for (var name in ehandlers) {
        ehandlers[name] = ehandlers[name].filter(function(h) {
                              if (h.cb != fn)
                                  return true;
                              delete h.cb;  // dispatch could have a ref to h
                          });  // replacing, not mutating the array
        if (!ehandlers[name].length) {
            delete ehandlers[name];
            mp._request_event(name, false);
        }
    }
}

// call only pre-registered handlers, but not ones which got unregistered
function dispatch_event(e) {
    var handlers = ehandlers[e.event];
    if (handlers) {
        for (var len = handlers.length, i = 0; i < len; i++) {
            var cb = handlers[i].cb;  // 'handlers' won't mutate, but unregister
            if (cb)                   // could remove cb from some items
                cb(e);
        }
    }
}

//  ----- idle observers -----
var iobservers = [],  // array of callbacks
    ideleted = false;

mp.register_idle = function(fn) {
    iobservers.push(fn);
}

mp.unregister_idle = function(fn) {
    iobservers.forEach(function(f, i) {
        if (f == fn)
             delete iobservers[i];  // -> same length but [more] sparse
    });
    ideleted = true;
}

function notify_idle_observers() {
    // forEach and filter skip deleted items and newly added items
    iobservers.forEach(function(f) { f() });
    if (ideleted) {
        iobservers = iobservers.filter(function() { return true });
        ideleted = false;
    }
}

//  ----- property observers -----
var next_oid = 1,
    observers = new_cache();  // items of id: fn

mp.observe_property = function(name, format, fn) {
    var id = next_oid++;
    observers[id] = fn;
    return mp._observe_property(id, name, format || undefined);  // allow null
}

mp.unobserve_property = function(fn) {
    for (var id in observers) {
        if (observers[id] == fn) {
            delete observers[id];
            mp._unobserve_property(id);
        }
    }
}

function notify_observer(e) {
    var cb = observers[e.id];
    if (cb)
        cb(e.name, e.data);
}

//  -----  Client messages -----
var messages = new_cache();  // items of name: fn

// overrides name. no libmpv API to reg/unreg specific messages.
mp.register_script_message = function(name, fn) {
    messages[name] = fn;
}

mp.unregister_script_message = function(name) {
    delete messages[name];
}

function dispatch_message(ev) {
    var cb = ev.args.length ? messages[ev.args[0]] : false;
    if (cb)
        cb.apply(null, ev.args.slice(1));
}

//  ----- hooks -----
var hooks = [];  // array of callbacks, id is index+1

function run_hook(ev) {
    var state = 0;  // 0:initial, 1:deferred, 2:continued
    function do_cont() { return state = 2, mp._hook_continue(ev.hook_id) }

    function err() { return mp.msg.error("hook already continued"), undefined }
    function usr_defer() { return state == 2 ? err() : (state = 1, true) }
    function usr_cont()  { return state == 2 ? err() : do_cont() }

    var cb = ev.id > 0 && hooks[ev.id - 1];
    if (cb)
        cb({ defer: usr_defer, cont: usr_cont });
    return state == 0 ? do_cont() : true;
}

mp.add_hook = function add_hook(name, pri, fn) {
    hooks.push(fn);
    // 50 (scripting docs default priority) maps to 0 (default in C API docs)
    return mp._hook_add(name, pri - 50, hooks.length);
}

// ----- async commands -----
var async_callbacks = new_cache();  // items of id: fn
var async_next_id = 1;

mp.command_native_async = function command_native_async(node, cb) {
    var id = async_next_id++;
    cb = cb || function dummy() {};
    if (!mp._command_native_async(id, node)) {
        var le = mp.last_error();
        setTimeout(cb, 0, false, undefined, le);  /* callback async */
        mp._set_last_error(le);
        return undefined;
    }
    async_callbacks[id] = cb;
    return id;
}

function async_command_handler(ev) {
    var cb = async_callbacks[ev.id];
    delete async_callbacks[ev.id];
    if (ev.error)
        cb(false, undefined, ev.error);
    else
        cb(true, ev.result, "");
}

mp.abort_async_command = function abort_async_command(id) {
    // cb will be invoked regardless, possibly with the abort result
    if (async_callbacks[id])
        mp._abort_async_command(id);
}

// shared-script-properties - always an object, even if without properties
function shared_script_property_set(name, val) {
    if (arguments.length > 1)
        return mp.commandv("change-list", "shared-script-properties", "append", "" + name + "=" + val);
    else
        return mp.commandv("change-list", "shared-script-properties", "remove", name);
}

function shared_script_property_get(name) {
    return mp.get_property_native("shared-script-properties")[name];
}

function shared_script_property_observe(name, cb) {
    return mp.observe_property("shared-script-properties", "native",
        function shared_props_cb(_name, val) { cb(name, val[name]) }
    );
}

mp.utils.shared_script_property_set = shared_script_property_set;
mp.utils.shared_script_property_get = shared_script_property_get;
mp.utils.shared_script_property_observe = shared_script_property_observe;

// osd-ass
var next_assid = 1;
mp.create_osd_overlay = function create_osd_overlay(format) {
    return {
        format: format || "ass-events",
        id: next_assid++,
        data: "",
        res_x: 0,
        res_y: 720,
        z: 0,

        update: function ass_update() {
            var cmd = {};  // shallow clone of `this', excluding methods
            for (var k in this) {
                if (typeof this[k] != "function")
                    cmd[k] = this[k];
            }

            cmd.name = "osd-overlay";
            cmd.res_x = Math.round(this.res_x);
            cmd.res_y = Math.round(this.res_y);

            return mp.command_native(cmd);
        },

        remove: function ass_remove() {
            mp.command_native({
                name: "osd-overlay",
                id: this.id,
                format: "none",
                data: "",
            });
            return mp.last_error() ? undefined : true;
        },
    };
}

// osd-ass legacy API
mp.set_osd_ass = function set_osd_ass(res_x, res_y, data) {
    if (!mp._legacy_overlay)
        mp._legacy_overlay = mp.create_osd_overlay("ass-events");

    var lo = mp._legacy_overlay;
    if (lo.res_x == res_x && lo.res_y == res_y && lo.data == data)
        return true;

    mp._legacy_overlay.res_x = res_x;
    mp._legacy_overlay.res_y = res_y;
    mp._legacy_overlay.data = data;
    return mp._legacy_overlay.update();
}

// the following return undefined on error, null passthrough, or legacy object
mp.get_osd_size = function get_osd_size() {
    var d = mp.get_property_native("osd-dimensions");
    return d && {width: d.w, height: d.h, aspect: d.aspect};
}
mp.get_osd_margins = function get_osd_margins() {
    var d = mp.get_property_native("osd-dimensions");
    return d && {left: d.ml, right: d.mr, top: d.mt, bottom: d.mb};
}

/**********************************************************************
 *  key bindings
 *********************************************************************/
// binds: items of (binding) name which are objects of:
// {cb: fn, forced: bool, maybe input: str, repeatable: bool, complex: bool}
var binds = new_cache();

function dispatch_key_binding(name, state, key_name) {
    var cb = binds[name] ? binds[name].cb : false;
    if (cb)  // "script-binding [<script_name>/]<name>" command was invoked
        cb(state, key_name);
}

var binds_tid = 0;  // flush timer id. actual id's are always true-thy
mp.flush_key_bindings = function flush_key_bindings() {
    function prioritized_inputs(arr) {
        return arr.sort(function(a, b) { return a.id > b.id })
                  .map(function(bind) { return bind.input });
    }

    var def = [], forced = [];
    for (var n in binds)
        if (binds[n].input)
            (binds[n].forced ? forced : def).push(binds[n]);
    // newer bindings for the same key override/hide older ones
    def = prioritized_inputs(def);
    forced = prioritized_inputs(forced);

    var sect = "input_" + mp.script_name;
    mp.commandv("define-section", sect, def.join("\n"), "default");
    mp.commandv("enable-section", sect, "allow-hide-cursor+allow-vo-dragging");

    sect = "input_forced_" + mp.script_name;
    mp.commandv("define-section", sect, forced.join("\n"), "force");
    mp.commandv("enable-section", sect, "allow-hide-cursor+allow-vo-dragging");

    clearTimeout(binds_tid);  // cancel future flush if called directly
    binds_tid = 0;
}

function sched_bindings_flush() {
    if (!binds_tid)
        binds_tid = setTimeout(mp.flush_key_bindings, 0);  // fires on idle
}

// name/opts maybe omitted. opts: object with optional bool members: repeatable,
// complex, forced, or a string str which is evaluated as object {str: true}.
var next_bid = 1;
function add_binding(forced, key, name, fn, opts) {
    if (typeof name == "function") {  // as if "name" is not part of the args
        opts = fn;
        fn = name;
        name = false;
    }
    var key_data = {forced: forced};
    switch (typeof opts) {  // merge opts into key_data
        case "string": key_data[opts] = true; break;
        case "object": for (var o in opts) key_data[o] = opts[o];
    }
    key_data.id = next_bid++;
    if (!name)
        name = "__keybinding" + key_data.id;  // new unique binding name

    if (key_data.complex) {
        mp.register_script_message(name, function msg_cb() {
            fn({event: "press", is_mouse: false});
        });
        var KEY_STATES = { u: "up", d: "down", r: "repeat", p: "press" };
        key_data.cb = function key_cb(state, key_name) {
            fn({
                event: KEY_STATES[state[0]] || "unknown",
                is_mouse: state[1] == "m",
                key_name: key_name || undefined
            });
        }
    } else {
        mp.register_script_message(name, fn);
        key_data.cb = function key_cb(state) {
            // Emulate the semantics at input.c: mouse emits on up, kb on down.
            // Also, key repeat triggers the binding again.
            var e = state[0],
                emit = (state[1] == "m") ? (e == "u") : (e == "d");
            if (emit || e == "p" || e == "r" && key_data.repeatable)
                fn();
        }
    }

    if (key)
        key_data.input = key + " script-binding " + mp.script_name + "/" + name;
    binds[name] = key_data;  // used by user and/or our (key) script-binding
    sched_bindings_flush();
}

mp.add_key_binding = add_binding.bind(null, false);
mp.add_forced_key_binding = add_binding.bind(null, true);

mp.remove_key_binding = function(name) {
    mp.unregister_script_message(name);
    delete binds[name];
    sched_bindings_flush();
}

/**********************************************************************
 Timers: compatible HTML5 WindowTimers - set/clear Timeout/Interval
 - Spec: https://www.w3.org/TR/html5/webappapis.html#timers
 - Guaranteed to callback a-sync to [re-]insertion (event-loop wise).
 - Guaranteed to callback by expiration order, or, if equal, by insertion order.
 - Not guaranteed schedule accuracy, though intervals should have good average.
 *********************************************************************/

// pending 'timers' ordered by expiration: latest at index 0 (top fires first).
// Earlier timers are quicker to handle - just push/pop or fewer items to shift.
var next_tid = 1,
    timers = [],  // while in process_timers, just insertion-ordered (push)
    tset_is_push = false,  // signal set_timer that we're in process_timers
    tcanceled = false,  // or object of items timer-id: true
    now = mp.get_time_ms;  // just an alias

function insert_sorted(arr, t) {
    for (var i = arr.length - 1; i >= 0 && t.when >= arr[i].when; i--)
        arr[i + 1] = arr[i];  // move up timers which fire earlier than t
    arr[i + 1] = t;  // i is -1 or fires later than t
}

// args (is "arguments"): fn_or_str [,duration [,user_arg1 [, user_arg2 ...]]]
function set_timer(repeat, args) {
    var fos = args[0],
        duration = Math.max(0, (args[1] || 0)),  // minimum and default are 0
        t = {
            id: next_tid++,
            when: now() + duration,
            interval: repeat ? duration : -1,
            callback: (typeof fos == "function") ? fos : Function(fos),
            args: (args.length < 3) ? false : [].slice.call(args, 2),
        };

    if (tset_is_push) {
        timers.push(t);
    } else {
        insert_sorted(timers, t);
    }
    return t.id;
}

g.setTimeout  = function setTimeout()  { return set_timer(false, arguments) };
g.setInterval = function setInterval() { return set_timer(true,  arguments) };

g.clearTimeout = g.clearInterval = function(id) {
    if (id < next_tid) {  // must ignore if not active timer id.
        if (!tcanceled)
            tcanceled = {};
        tcanceled[id] = true;
    }
}

// arr: ordered timers array. ret: -1: no timers, 0: due, positive: ms to wait
function peek_wait(arr) {
    return arr.length ? Math.max(0, arr[arr.length - 1].when - now()) : -1;
}

function peek_timers_wait() {
    return peek_wait(timers);  // must not be called while in process_timers
}

// Callback all due non-canceled timers which were inserted before calling us.
// Returns wait in ms till the next timer (possibly 0), or -1 if nothing pends.
function process_timers() {
    var wait = peek_wait(timers);
    if (wait != 0)
        return wait;

    var actives = timers;  // only process those already inserted by now
    timers = [];  // we'll handle added new timers at the end of processing.
    tset_is_push = true;  // signal set_timer to just push-insert

    do {
        var t = actives.pop();
        if (tcanceled && tcanceled[t.id])
            continue;

        if (t.args) {
            t.callback.apply(null, t.args);
        } else {
            (0, t.callback)();  // faster, nicer stack trace than t.cb.call()
        }

        if (t.interval >= 0) {
            // allow 20 ms delay/clock-resolution/gc before we skip and reset
            t.when = Math.max(now() - 20, t.when + t.interval);
            timers.push(t);  // insertion order only
        }
    } while (peek_wait(actives) == 0);

    // new 'timers' are insertion-ordered. remains of actives are fully ordered
    timers.forEach(function(t) { insert_sorted(actives, t) });
    timers = actives;  // now we're fully ordered again, and with all timers
    tset_is_push = false;
    if (tcanceled) {
        timers = timers.filter(function(t) { return !tcanceled[t.id] });
        tcanceled = false;
    }
    return peek_wait(timers);
}

/**********************************************************************
 CommonJS module/require

 Spec: http://wiki.commonjs.org/wiki/Modules/1.1.1
 - All the mandatory requirements are implemented, all the unit tests pass.
 - The implementation makes the following exception:
   - Allows the chars [~@:\\] in module id for meta-dir/builtin/dos-drive/UNC.

 Implementation choices beyond the specification:
 - A module may assign to module.exports (rather than only to exports).
 - A module's 'this' is the global object, also if it sets strict mode.
   - No 'global'/'self'. Users can do "this.global = this;" before require(..)
   - A module has "privacy of its top scope", runs in its own function context.
 - No id identity with symlinks - a valid choice which others make too.
 - require("X") always maps to "X.js" -> require("foo.js") is file "foo.js.js".
 - Global modules search paths are 'scripts/modules.js/' in mpv config dirs.
 - A main script could e.g. require("./abc") to load a non-global module.
 - Module id supports mpv path enhancements, e.g. ~/foo, ~~/bar, ~~desktop/baz
 *********************************************************************/

mp.module_paths = [];  // global modules search paths
if (mp.script_path !== undefined)  // loaded as a directory
    mp.module_paths.push(mp.utils.join_path(mp.script_path, "modules"));

// Internal meta top-dirs. Users should not rely on these names.
var MODULES_META = "~~modules",
    SCRIPTDIR_META = "~~scriptdir",  // relative script path -> meta absolute id
    main_script = mp.utils.split_path(mp.script_file);  // -> [ path, file ]

function resolve_module_file(id) {
    var sep = id.indexOf("/"),
        base = id.substring(0, sep),
        rest = id.substring(sep + 1) + ".js";

    if (base == SCRIPTDIR_META)
        return mp.utils.join_path(main_script[0], rest);

    if (base == MODULES_META) {
        for (var i = 0; i < mp.module_paths.length; i++) {
            try {
                var f = mp.utils.join_path(mp.module_paths[i], rest);
                mp.utils.read_file(f, 1);  // throws on any error
                return f;
            } catch (e) {}
        }
        throw(Error("Cannot find module file '" + rest + "'"));
    }

    return id + ".js";
}

// Delimiter '/', remove redundancies, prefix with modules meta-root if needed.
// E.g. c:\x -> c:/x, or ./x//y/../z -> ./x/z, or utils/x -> ~~modules/utils/x .
function canonicalize(id) {
    var path = id.replace(/\\/g,"/").split("/"),
        t = path[0],
        base = [];

    // if not strictly relative then must be top-level. figure out base/rest
    if (t != "." && t != "..") {
        // global module if it's not fs-root/home/dos-drive/builtin/meta-dir
        if (!(t == "" || t == "~" || t[1] == ":" ||  t == "@" || t.match(/^~~/)))
            path.unshift(MODULES_META);  // add an explicit modules meta-root

        if (id.match(/^\\\\/))  // simple UNC handling, preserve leading \\srv
            path = ["\\\\" + path[2]].concat(path.slice(3));  // [ \\srv, shr..]

        if (t[1] == ":" && t.length > 2) {  // path: [ "c:relative", "path" ]
            path[0] = t.substring(2);
            path.unshift(t[0] + ":.");  //  ->  [ "c:.", "relative", "path" ]
        }
        base = [path.shift()];
    }

    // path is now logically relative. base, if not empty, is its [meta] root.
    // normalize the relative part - always id-based (spec Module Id, 1.3.6).
    var cr = [];  // canonicalized relative
    for (var i = 0; i < path.length; i++) {
        if (path[i] == "." || path[i] == "")
            continue;
        if (path[i] == ".." && cr.length && cr[cr.length - 1] != "..") {
            cr.pop();
            continue;
        }
        cr.push(path[i]);
    }

    if (!base.length && cr[0] != "..")
        base = ["."];  // relative and not ../<stuff> so must start with ./
    return base.concat(cr).join("/");
}

function resolve_module_id(base_id, new_id) {
    new_id = canonicalize(new_id);
    if (!new_id.match(/^\.\/|^\.\.\//))  // doesn't start with ./ or ../
        return new_id;  // not relative, we don't care about base_id

    var combined = mp.utils.join_path(mp.utils.split_path(base_id)[0], new_id);
    return canonicalize(combined);
}

var req_cache = new_cache();  // global for all instances of require

// ret: a require function instance which uses base_id to resolve relative id's
function new_require(base_id) {
    return function require(id) {
        id = resolve_module_id(base_id, id);  // id is now top-level
        if (req_cache[id])
            return req_cache[id].exports;

        var new_module = {id: id, exports: {}};
        req_cache[id] = new_module;
        try {
            var filename = resolve_module_file(id);
            // we need dedicated free vars + filename in traces + allow strict
            var str = "mp._req = function(require, exports, module) {" +
                          mp.utils.read_file(filename) +
                      "\n;}";
            mp.utils.compile_js(filename, str)();  // only runs the assignment
            var tmp = mp._req;  // we have mp._req, or else we'd have thrown
            delete mp._req;
            tmp.call(g, new_require(id), new_module.exports, new_module);
        } catch (e) {
            delete req_cache[id];
            throw(e);
        }

        return new_module.exports;
    };
}

g.require = new_require(SCRIPTDIR_META + "/" + main_script[1]);

/**********************************************************************
 *  mp.options
 *********************************************************************/
function read_options(opts, id, on_update, conf_override) {
    id = String(id ? id : mp.get_script_name());
    mp.msg.debug("reading options for " + id);

    var conf, fname = "~~/script-opts/" + id + ".conf";
    try {
        conf = arguments.length > 3 ? conf_override : mp.utils.read_file(fname);
    } catch (e) {
        mp.msg.verbose(fname + " not found.");
    }

    // data as config file lines array, or empty array
    var data = conf ? conf.replace(/\r\n/g, "\n").split("\n") : [],
        conf_len = data.length;  // before we append script-opts below

    // Append relevant script-opts as <key-sans-id>=<value> to data
    var sopts = mp.get_property_native("options/script-opts"),
        prefix = id + "-";
    for (var key in sopts) {
        if (key.indexOf(prefix) == 0)
            data.push(key.substring(prefix.length) + "=" + sopts[key]);
    }

    // Update opts from data
    data.forEach(function(line, i) {
        if (line[0] == "#" || line.trim() == "")
            return;

        var key = line.substring(0, line.indexOf("=")),
            val = line.substring(line.indexOf("=") + 1),
            type = typeof opts[key],
            info = i < conf_len ? fname + ":" + (i + 1)  // 1-based line number
                                : "script-opts:" + prefix + key;

        if (!opts.hasOwnProperty(key))
            mp.msg.warn(info, "Ignoring unknown key '" + key + "'");
        else if (type == "string")
            opts[key] = val;
        else if (type == "boolean" && (val == "yes" || val == "no"))
            opts[key] = (val == "yes");
        else if (type == "number" && val.trim() != "" && !isNaN(val))
            opts[key] = Number(val);
        else
            mp.msg.error(info, "Error: can't convert '" + val + "' to " + type);
    });

    if (on_update) {
        mp.observe_property("options/script-opts", "native", function(_n, _v) {
            var saved = JSON.parse(JSON.stringify(opts));  // clone
            var changelist = {}, changed = false;
            read_options(opts, id, 0, conf);  // re-apply orig-file + script-opts
            for (var key in opts) {
                if (opts[key] != saved[key])  // type always stays the same
                    changelist[key] = changed = true;
            }
            if (changed)
                on_update(changelist);
        });
    }
}

mp.options = { read_options: read_options };

/**********************************************************************
 *  various
 *********************************************************************/
g.print = mp.msg.info;  // convenient alias
mp.get_script_name = function() { return mp.script_name };
mp.get_script_file = function() { return mp.script_file };
mp.get_script_directory = function() { return mp.script_path };
mp.get_time = function() { return mp.get_time_ms() / 1000 };
mp.utils.getcwd = function() { return mp.get_property("working-directory") };
mp.utils.getpid = function() { return mp.get_property_number("pid") }
mp.get_mouse_pos = function() { return mp.get_property_native("mouse-pos") };
mp.utils.write_file = mp.utils._write_file.bind(null, false);
mp.utils.append_file = mp.utils._write_file.bind(null, true);
mp.dispatch_event = dispatch_event;
mp.process_timers = process_timers;
mp.notify_idle_observers = notify_idle_observers;
mp.peek_timers_wait = peek_timers_wait;

mp.get_opt = function(key, def) {
    var v = mp.get_property_native("options/script-opts")[key];
    return (typeof v != "undefined") ? v : def;
}

mp.osd_message = function osd_message(text, duration) {
    mp.commandv("show_text", text, Math.round(1000 * (duration || -1)));
}

mp.utils.subprocess = function subprocess(t) {
    var cmd = { name: "subprocess", capture_stdout: true };
    var new_names = { cancellable: "playback_only", max_size: "capture_size" };
    for (var k in t)
        cmd[new_names[k] || k] = t[k];

    var rv = mp.command_native(cmd);
    if (mp.last_error())  /* typically on missing/incorrect args */
        rv = { error_string: mp.last_error(), status: -1 };
    if (rv.error_string)
        rv.error = rv.error_string;
    return rv;
}

mp.utils.subprocess_detached = function subprocess_detached(t) {
    return mp.commandv.apply(null, ["run"].concat(t.args));
}


// ----- dump: like print, but expands objects/arrays recursively -----
function replacer(k, v) {
    var t = typeof v;
    if (t == "function" || t == "undefined")
        return "<" + t + ">";
    if (Array.isArray(this) && t == "object" && v !== null) {  // "safe" mode
        if (this.indexOf(v) >= 0)
            return "<VISITED>";
        this.push(v);
    }
    return v;
}

function obj2str(v) {
    try {  // can process objects more than once, but throws on cycles
        return JSON.stringify(v, replacer.bind(null), 2);
    } catch (e) { // simple safe: exclude visited objects, even if not cyclic
        return JSON.stringify(v, replacer.bind([]), 2);
    }
}

g.dump = function dump() {
    var toprint = [];
    for (var i = 0; i < arguments.length; i++) {
        var v = arguments[i];
        toprint.push((typeof v == "object") ? obj2str(v) : replacer(0, v));
    }
    print.apply(null, toprint);
}

/**********************************************************************
 *  main listeners and event loop
 *********************************************************************/
mp.keep_running = true;
g.exit = function() { mp.keep_running = false };  // user-facing too
mp.register_event("shutdown", g.exit);
mp.register_event("property-change", notify_observer);
mp.register_event("hook", run_hook);
mp.register_event("command-reply", async_command_handler);
mp.register_event("client-message", dispatch_message);
mp.register_script_message("key-binding", dispatch_key_binding);

g.mp_event_loop = function mp_event_loop() {
    var wait = 0;  // seconds
    do {  // distapch events as long as they arrive, then do the timers/idle
        var e = mp.wait_event(wait);
        if (e.event != "none") {
            dispatch_event(e);
            wait = 0;  // poll the next one
        } else {
            wait = process_timers() / 1000;
            if (wait != 0 && iobservers.length) {
                notify_idle_observers();  // can add timers -> recalculate wait
                wait = peek_timers_wait() / 1000;
            }
        }
    } while (mp.keep_running);
};


// let the user extend us, e.g. by adding items to mp.module_paths
var initjs = mp.find_config_file("init.js");  // ~~/init.js
if (initjs)
    require(initjs.slice(0, -3));  // remove ".js"
else if ((initjs = mp.find_config_file(".init.js")))
    mp.msg.warn("Use init.js instead of .init.js (ignoring " + initjs + ")");

})(this)
