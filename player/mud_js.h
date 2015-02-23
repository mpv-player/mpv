/**********************************************************************
  MUD: MuJS -> Duktape relatively generic wrapper.
       (Write MuJS code and support Duktape backend too)

  Notes:

- Incomplete and could be more efficient, but it's enough for mpv for now.

- include 'mud_js.h' regardless if compiling with MuJS or Duktape.
  When compiling with MuJS it's mostly no-op except for a thin
  compatibility layer (e.g. it adds mud_ret_t etc)

- #define MUD_USE_DUK 1 (before including mud_js.h) to indicate that
  Duktape is used, or as 0 to use MuJS.

- Add MUD_WRAPPER(fn); to add a Duktape wrapper for MuJS' c-function fn.
  Alternatively, replace the prototype with MUD_C_FUNC(foo, js_State *bar)

- MUD's js_newcfunction works only for function name literals at compile time.
  When adding a function pointer dynamically, use mud_newcfunction_runtime
  and make sure it points to the wrapper when MUD_USE_DUK is 1,
  at which case the macro MUD_FNAME could be useful (in compile time).

- Use mud_push_next_key instead of js_nextiterator (Duktape semantics).

- mud_ret_t is defined as duk_ret_t with Duktape, or void with MuJS.

- An error object can be printed as stacktrace at MuJS, but with Duktape
  it's its 'stack' property. Use mud_top_error_to_str(J) before printing.

- userdata tags and prototypes are ignored when using Duktape.

 *********************************************************************/

#if MUD_USE_DUK
  #define mud_ret_t duk_ret_t
  // js_newcfunction uses this internally, but may also be useful elsewhere.
  #define MUD_FNAME(fn)  fn ## __mud
#else
  #define MUD_FNAME(fn)  fn
  #define mud_ret_t void
  #define MUD_WRAPPER(fn) /* no-op */
  #define mud_top_error_to_str(J) /* no-op */
  #define mud_newcfunction_runtime js_newcfunction
  // replace js_nextiterator with mud_push_next_key (with Duktape semantics)
#endif

#if MUD_USE_DUK
/**********************************************************************
 *  1:1 mapping as far as mpv is concerned (and mostly also otherwise)
 *********************************************************************/
#define js_State          duk_context
#define js_newstate(a,b)  duk_create_heap_default()
#define js_freestate      duk_destroy_heap

#define js_gettop         duk_get_top
#define js_pcall          duk_pcall_method
#define js_call           duk_call_method
#define js_pop            duk_pop_n
#define js_throw          duk_throw
#define js_copy           duk_dup
#define js_gc             duk_gc

#define js_pushglobal     duk_push_global_object
#define js_pushundefined  duk_push_undefined
#define js_pushboolean    duk_push_boolean
#define js_pushstring     duk_push_string
#define js_pushlstring    duk_push_lstring
#define js_pushnumber     duk_push_number
#define js_pushnull       duk_push_null

#define js_newobject      duk_push_object
#define js_newarray       duk_push_array

#define js_setproperty    duk_put_prop_string
#define js_getproperty    duk_get_prop_string
#define js_getglobal      duk_get_global_string
#define js_getlength      duk_get_length
#define js_setindex       duk_put_prop_index
#define js_getindex       duk_get_prop_index
#define js_getlength      duk_get_length

#define js_iscallable     duk_is_function
#define js_isundefined    duk_is_undefined
#define js_isobject       duk_is_object_coercible
#define js_isstring       duk_is_string
#define js_tostring       duk_safe_to_string
#define js_isnull         duk_is_null
#define js_isboolean      duk_is_boolean
#define js_isnumber       duk_is_number
#define js_isarray        duk_is_array

#define js_tonumber       duk_to_number
#define js_toboolean      duk_to_boolean
#define js_tointeger      duk_to_int

/**********************************************************************
 *  c-functions interface
 *********************************************************************/
// Duktape doesn't support MIN_ARGS style nargs which MuJS uses.
// instead, Duktape supports VAR_ARGS and EXACT_ARGS styles.
//
// MUD implements this by always pushing with DUK_VARARGS, but also
// storing nargs at the function instance 'magic' store when pushing
// the function, and later MUD_WRAPPER reads this value at runtime and
// validates the arguments according to the MuJS semantics.
//
// - Duktape's use 0 based args index, MuJS use 1 (for actual args)
//   - add insert.
// - MuJS functions are always returning something
//   - return 1.
// - MuJS functions expect min_args, while duk has abs_args or var_args
//   - use var_args + pad.

/* example:
js_call(j, 1) - one arg for a func which expects 2 (nargs==2).
MuJS: top==3, args=[this, arg1, undefined]
duk:  top==1, args=[arg1]
     after insertion of 'this': top==2 [this, arg1]
     after padding: top==3             [this, arg1, undefined]
     -- and the access indices are now identical (positive and negative)
*/

#define js_newcfunction(J, fname, js_name, nargs)           \
{                                                           \
    duk_push_c_function(J, MUD_FNAME(fname), DUK_VARARGS);  \
    duk_set_magic(J, -1, nargs);                            \
}

#define mud_newcfunction_runtime(J, f_ptr, js_name, nargs)  \
{                                                           \
    duk_push_c_function(J, f_ptr, DUK_VARARGS);             \
    duk_set_magic(J, -1, nargs);                            \
}

// includes a forward declaration for the wrapped function.
// Note: hardcoded 'static'. Modify if required.
#define MUD_WRAPPER(fn)                             \
  static void fn(js_State*);                        \
  static duk_ret_t MUD_FNAME(fn)(js_State *J)       \
  {                                                 \
      duk_push_this(J);                             \
      duk_insert(J, 0);                             \
      int nargs = duk_get_current_magic(J);         \
      for (int i = duk_get_top(J); i <= nargs; i++) \
          duk_push_undefined(J);                    \
      fn(J);                                        \
      return 1;                                     \
  }

// Macro to support empty va_args. the extra arg (0) should be ignored.
#define _MUD_ERR(fn, J, fmt, ...) fn(J, DUK_ERR_UNCAUGHT_ERROR, fmt, __VA_ARGS__)
#define js_error(...)    _MUD_ERR(duk_error,             __VA_ARGS__, 0)
// MuJS doesn't use printf args for js_new*error, so careful with % at the string.
#define js_newerror(...) _MUD_ERR(duk_push_error_object, __VA_ARGS__, 0)

// userdata - tag and prototype are ignored.
#define js_newuserdata(J, tag, ptr) { duk_pop(J); duk_push_pointer(J, ptr); }
#define js_touserdata(J, idx, tag)  duk_require_pointer(J, idx)

static inline void js_setlength(js_State *J, int idx, unsigned int len)
{
    idx = duk_normalize_index(J, idx);
    duk_push_number(J, len);
    duk_put_prop_string(J, idx, "length");
}

static inline void js_setcontext(js_State *J, void *ctx)
{
    duk_push_heap_stash(J);
    duk_push_pointer(J, ctx);
    duk_put_prop_string(J, -2, "_mud_ctx");
    duk_pop(J);
}

static inline void *js_getcontext(js_State *J)
{
    duk_push_heap_stash(J);
    duk_get_prop_string(J, -1, "_mud_ctx");
    void *ctx = duk_require_pointer(J, -1);
    duk_pop_n(J, 2);  // -
    return ctx;
}

static inline int js_ploadstring(js_State *J, const char *as_filename,
                                 const char *data)
{
    js_pushstring(J, data);
    js_pushstring(J, as_filename);
    return duk_pcompile(J, 0);
}

#define js_loadstring(J, af, d) { if (js_ploadstring(J, af, d)) duk_throw(J); }

#define js_pushiterator(J, idx, isOwn) \
               duk_enum(J, idx, isOwn ? DUK_ENUM_OWN_PROPERTIES_ONLY : 0)

// If Duktape was compile with stack traces (the default), then it's at the
// 'stack' property of the error object. With MuJS it's the error object itself.
#define mud_top_error_to_str(J)           \
{                                         \
    duk_get_prop_string(J, -1, "stack");  \
    if (duk_is_undefined(J, -1))          \
        duk_pop(J);                       \
    else                                  \
        duk_replace(J, -2);               \
}

#endif /* MUD_USE_DUK */

static int mud_push_next_key(js_State *J, int idx)
{
#if MUD_USE_DUK
    return duk_next(J, idx, 0);
#else
    const char *key = js_nextiterator(J, idx);
    if (key)
        js_pushstring(J, key);
    return key ? 1 : 0;
#endif
}

#define MUD_C_FUNC(fname, state_decleration) \
  MUD_WRAPPER(fname);                        \
  static void fname(state_decleration) /* body should follow */
