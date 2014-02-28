/* Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Note: the client API is licensed under ISC (see above) to ease
 * interoperability with other licenses. But keep in mind that the
 * mpv core is still mostly GPLv2+. It's up to lawyers to decide
 * whether applications using this API are affected by the GPL.
 * One argument against this is that proprietary applications
 * using mplayer in slave mode is apparently tolerated, and this
 * API is basically equivalent to slave mode.
 */

#ifndef MPV_CLIENT_API_H_
#define MPV_CLIENT_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Warning: this API is still work in progress. This notice will be removed
 * once the API is considered reasonably stable.
 */

/**
 * Mechanisms provided by this API
 * -------------------------------
 *
 * This API provides general control over mpv playback. It does not give you
 * direct access to individual components of the player, only the whole thing.
 * It's somewhat equivalent to MPlayer's slave mode. You can send commands,
 * retrieve or set playback status or settings with properties, and receive
 * events.
 *
 * The API can be used in two ways:
 * 1) Internally in mpv, to provide additional features to the command line
 *    player. Lua scripting uses this. (Currently there is no plugin API to
 *    get a client API handle in external user code. It has to be a fixed
 *    part of the player at compilation time.)
 * 2) Using mpv as a library with mpv_create(). This basically allows embedding
 *    mpv in other applications.
 *
 * Event loop
 * ----------
 *
 * In general, the API user should run an event loop (with mpv_wait_event())
 * in order to receive events, although it also should be possible to integrate
 * client API usage in other event loops (e.g. GUI toolkits) with the
 * mpv_set_wakeup_callback() function, and then polling for events by calling
 * mpv_wait_event() with a 0 timeout.
 *
 * Note that the event loop is detached from the actual player. Not calling
 * mpv_wait_event() will not stop playback. It will eventually congest the
 * event queue of your API handle, though.
 *
 * Synchronous vs. asynchronous calls
 * ----------------------------------
 *
 * The API allows both synchronous and asynchronous calls. Synchronous calls
 * have to wait until the playback core is ready, which currently can take
 * an unbounded time (e.g. if network is slow or unresponsive). Asynchronous
 * calls just queue operations as requests, and return the result of the
 * operation as events.
 *
 * Asynchronous calls
 * ------------------
 *
 * The client API includes asynchronous functions. These allow you to send
 * requests instantly, and get replies as events at a later point. The
 * requests are made with functions carrying the _async suffix, and replies
 * are returned by mpv_wait_event() (interleaved with the normal event stream).
 *
 * A 64 bit userdata value is used to allow the user to associate requests
 * with replies. The value is passed as reply_userdata parameter to the request
 * function. The reply to the request will have the reply
 * mpv_event->reply_userdata field set to the same value as the
 * reply_userdata parameter of the corresponding request.
 *
 * This userdata value is arbitrary and is never interpreted by the API. Note
 * that the userdata value 0 is also allowed, but then the client must be
 * careful not accidentally interpret the mpv_event->reply_userdata if an
 * event is not a reply. (For non-replies, this field is set to 0.)
 *
 * Currently, asynchronous calls are always strictly ordered (even with
 * synchronous calls) for each client, although that may change in the future.
 *
 * Multithreading
 * --------------
 *
 * The client API is generally fully thread-safe, unless otherwise noted.
 * Currently, there is no real advantage in using more than 1 thread to access
 * the client API, since everything is serialized through a single lock in the
 * playback core.
 *
 * Basic environment requirements
 * ------------------------------
 *
 * This documents basic requirements on the C environment. This is especially
 * important if mpv is used as library with mpv_create().
 *
 * - The LC_NUMERIC locale category must be set to "C". If your program calls
 *   setlocale(), be sure not to use LC_ALL, or if you do, reset LC_NUMERIC
 *   to its sane default: setlocale(LC_NUMERIC, "C").
 * - If a X11 based VO is used, mpv will set the xlib error handler. This error
 *   handler is process-wide, and there's no proper way to share it with other
 *   xlib users within the same process. This might confuse GUI toolkits.
 * - The FPU precision must be set at least to double precision.
 * - On Windows, mpv will call timeBeginPeriod(1).
 *
 * Embedding the video window
 * --------------------------
 *
 * Currently you have to get the raw window handle, and set it as "wid" option.
 * This works on X11 and win32 only. In addition, it works with a few VOs only,
 * and VOs which do not support this will just create a freestanding window.
 *
 * Both on X11 and win32, the player will fill the window referenced by the
 * "wid" option fully and letterbox the video (i.e. add black bars if the
 * aspect ratio of the window and the video mismatch).
 */

/**
 * The version is incremented on each change. The 16 lower bits are incremented
 * if something in mpv is changed that might affect the client API, but doesn't
 * change C API itself (like the removal of an option or a property). The higher
 * 16 bits are incremented if the C API itself changes.
 */
#define MPV_CLIENT_API_VERSION 0x00000000UL

/**
 * Return the MPV_CLIENT_API_VERSION the mpv source has been compiled with.
 */
unsigned long mpv_client_api_version(void);

/**
 * Client context used by the client API. Every client has its own private
 * handle.
 */
typedef struct mpv_handle mpv_handle;

/**
 * List of error codes than can be returned by API functions. 0 and positive
 * return values always mean success, negative values are always errors.
 */
typedef enum mpv_error {
    /**
     * No error happened (used to signal successful operation).
     * Keep in mind that many API functions returning error codes can also
     * return positive values, which also indicate success. API users can
     * hardcode the fact that ">= 0" means success.
     */
    MPV_ERROR_SUCCESS           = 0,
    /**
     * The event ringbuffer is full. This means the client is choked, and can't
     * receive any events. This can happen when too many asynchronous requests
     * have been made, but not answered. Probably never happens in practice,
     * unless the mpv core is frozen for some reason, and the client keeps
     * making asynchronous requests. (Bugs in the client API implementation
     * could also trigger this, e.g. if events become "lost".)
     */
    MPV_ERROR_EVENT_QUEUE_FULL = -1,
    /**
     * Memory allocation failed.
     */
    MPV_ERROR_NOMEM             = -2,
    /**
     * The mpv core wasn't configured and initialized yet. See the notes in
     * mpv_create().
     */
    MPV_ERROR_UNINITIALIZED     = -3,
    /**
     * Generic catch-all error if a parameter is set to an invalid or
     * unsupported value. This is used if there is no better error code.
     */
    MPV_ERROR_INVALID_PARAMETER = -4,
    /**
     * Trying to set an option that doesn't exist.
     */
    MPV_ERROR_OPTION_NOT_FOUND  = -5,
    /**
     * Trying to set an option using an unsupported MPV_FORMAT.
     */
    MPV_ERROR_OPTION_FORMAT     = -6,
    /**
     * Setting the option failed. Typically this happens if the provided option
     * value could not be parsed.
     */
    MPV_ERROR_OPTION_ERROR      = -7,
    /**
     * The accessed property doesn't exist.
     */
    MPV_ERROR_PROPERTY_NOT_FOUND = -8,
    /**
     * Trying to set or get a property using an unsupported MPV_FORMAT.
     */
    MPV_ERROR_PROPERTY_FORMAT    = -9,
    /**
     * The property exists, but is not available. This usually happens when the
     * associated subsystem is not active, e.g. querying audio parameters while
     * audio is disabled.
     */
    MPV_ERROR_PROPERTY_UNAVAILABLE = -10,
    /**
     * Error setting or getting a property.
     */
    MPV_ERROR_PROPERTY_ERROR    = -11
} mpv_error;

/**
 * Return a string describing the error. For unknown errors, the string
 * "unknown error" is returned.
 *
 * @param error error number, see enum mpv_error
 * @return A static string describing the error. The string is completely
 *         static, i.e. doesn't need to be deallocated, and is valid forever.
 */
const char *mpv_error_string(int error);

/**
 * General function to deallocate memory returned by some of the API functions.
 * Call this only if it's explicitly documented as allowed. Calling this on
 * mpv memory not owned by the caller will lead to undefined behavior.
 *
 * @param data A valid pointer returned by the API, or NULL.
 */
void mpv_free(void *data);

/**
 * Return the name of this client handle. Every client has its own unique
 * name, which is mostly used for user interface purposes.
 *
 * @return The client name. The string is read-only and is valid until
 *         mpv_destroy() is called.
 */
const char *mpv_client_name(mpv_handle *ctx);

/**
 * Create a new mpv instance and an associated client API handle to control
 * the mpv instance. This instance is in a pre-initialized state,
 * and needs to be initialized to be actually used with most other API
 * functions.
 *
 * Most API functions will return MPV_ERROR_UNINITIALIZED in the uninitialized
 * state. You can call mpv_set_option() (or mpv_set_option_string() and other
 * variants) to set initial options. After this, call mpv_initialize() to start
 * the player, and then use e.g. mpv_command() to start playback of a file.
 *
 * The point of separating handle creation and actual initialization is that
 * you can configure things which can't be changed during runtime.
 *
 * Unlike the command line player, this will have initial settings suitable
 * for embedding in applications. The following settings are different:
 * - stdin/stdout/stderr and the terminal will never be accessed. This is
 *   equivalent to setting the --no-terminal option.
 *   (Technically, this also suppresses C signal handling.)
 * - No config files will be loaded. This is roughly equivalent to using
 *   --no-config (but actually the code path for loading config files is
 *   disabled).
 * - Idle mode is enabled, which means the playback core will enter idle mode
 *   if there are no more files to play on the internal playlist, instead of
 *   exiting. This is equivalent to the --idle option.
 * - Disable parts of input handling.
 *
 * All this assumes that API users want a mpv instance that is strictly
 * isolated from the command line player's configuration, user settings, and
 * so on. You can re-enable disabled features by setting the appropriate
 * options.
 *
 * The mpv command line parser is not available through this API, but you can
 * set individual options with mpv_set_option(). Files for playback must be
 * loaded with mpv_command() or others.
 *
 * Note that you should avoid doing concurrent accesses on the uninitialized
 * client handle. (Whether concurrent access is definitely allowed or not has
 * yet to be decided.)
 *
 * @return a new mpv client API handle
 */
mpv_handle *mpv_create(void);

/**
 * Initialize an uninitialized mpv instance. If the mpv instance is already
 * running, an error is retuned.
 *
 * This function needs to be called to make full use of the client API if the
 * client API handle was created with mpv_create().
 *
 * @return error code
 */
int mpv_initialize(mpv_handle *ctx);

/**
 * Disconnect and destroy the client context. ctx will be deallocated with this
 * API call. This leaves the player running. If you want to be sure that the
 * player is terminated, send a "quit" command, and wait until the
 * MPV_EVENT_SHUTDOWN event is received.
 */
void mpv_destroy(mpv_handle *ctx);

/**
 * Stop the playback thread. Normally, the client API stops the playback thread
 * automatically in order to process requests. However, the playback thread is
 * restarted again after the request was processed. Then the playback thread
 * will continue to display the next video frame, during which it will not
 * reply to any requests. (This takes up to 50ms.)
 *
 * (Internally, it first renders the video and other things, and then blocks
 * until it can be displayed - and it won't react to anything else in that
 * time. The main reason for that is that the VO is in a "in between" state,
 * in which it can't process normal requests - for example, OSD redrawing or
 * screenshots would be broken.)
 *
 * This is usually a problem: only 1 request per video frame will be executed,
 * which will make the client API to appear extremely slow.
 *
 * Suspending the playback thread allows you to prevent the playback thread from
 * running, so that you can make multiple accesses without being blocked.
 *
 * Suspension is reentrant and recursive for convenience. Any thread can call
 * the suspend function multiple times, and the playback thread will remain
 * suspended until the last thread resumes it. Note that during suspension,
 * clients still have concurrent access to the core, which is serialized through
 * a single mutex.
 *
 * Call mpv_resume() to resume the playback thread. You must call mpv_resume()
 * for each mpv_suspend() call. Calling mpv_resume() more often than
 * mpv_suspend() is not allowed.
 *
 * Calling this on an uninitialized player (see mpv_create()) will deadlock.
 *
 * Note: the need for this call might go away at some point.
 */
void mpv_suspend(mpv_handle *ctx);

/**
 * See mpv_suspend().
 */
void mpv_resume(mpv_handle *ctx);

/**
 * Return the internal time in microseconds. This has an arbitrary start offset,
 * but will never wrap or go backwards (note: the latter is probably a lie in
 * the current implementation, it can go backwards on system clock changes).
 *
 * Note that this is always the real time, and doesn't necessarily have to do
 * with playback time. For example, playback could go faster or slower due to
 * playback speed, or due to playback being paused. Use the "time-pos" property
 * instead to get the playback status.
 */
int64_t mpv_get_time_us(mpv_handle *ctx);

/**
 * Data format for options and properties. The API functions to get/set
 * properties and options support multiple formats, and this enum describes
 * them.
 */
typedef enum mpv_format {
    /**
     * Invalid.
     */
    MPV_FORMAT_NONE             = 0,
    /**
     * The basic type is char*. It returns the raw property string, like
     * using ${=property} in input.conf (see input.rst).
     *
     * NULL isn't an allowed value.
     *
     * Warning: although the encoding is usually UTF-8, this is not always the
     *          case. File tags often store strings in some legacy codepage,
     *          and even filenames don't necessarily have to be in UTF-8 (at
     *          least on Linux). If you pass the strings to code that requires
     *          valid UTF-8, you have to sanitize it in some way.
     *
     * Example for reading:
     *
     *     char *result = NULL;
     *     if (mpv_get_property(ctx, "property", MPV_FORMAT_STRING, &result) < 0)
     *         goto error;
     *     printf("%s\n", result);
     *     mpv_free(result);
     *
     * Or just use mpv_get_property_string().
     *
     * Example for writing:
     *
     *     char *value = "the new value";
     *     // yep, you pass the address to the variable
     *     // (needed for symmetry with other types and mpv_get_property)
     *     mpv_set_property(ctx, "property", MPV_FORMAT_STRING, &value);
     *
     * Or just use mpv_set_property_string().
     *
     */
    MPV_FORMAT_STRING           = 1,
    /**
     * The basic type is char*. It returns the OSD property string, like
     * using ${property} in input.conf (see input.rst). In many cases, this
     * is the same as the raw string, but in other cases it's formatted for
     * display on OSD. It's intended to be human readable. Do not attempt to
     * parse these strings.
     *
     * Only valid when doing read access. The rest works like MPV_FORMAT_STRING.
     */
    MPV_FORMAT_OSD_STRING       = 2,
    /**
     * The basic type is int. The only allowed values are 0 ("no")
     * and 1 ("yes").
     *
     * Example for reading:
     *
     *     int result;
     *     if (mpv_get_property(ctx, "property", MPV_FORMAT_FLAG, &result) < 0)
     *         goto error;
     *     printf("%s\n", result ? "true" : "false");
     *
     * Example for writing:
     *
     *     int flag = 1;
     *     mpv_set_property(ctx, "property", MPV_FORMAT_STRING, &flag);
     */
    MPV_FORMAT_FLAG             = 3,
    /**
     * The basic type is int64_t.
     */
    MPV_FORMAT_INT64            = 4,
    /**
     * The basic type is double.
     */
    MPV_FORMAT_DOUBLE           = 5,
    /**
     * The type is mpv_node.
     *
     * For reading, you usually would pass a pointer to a stack-allocated
     * mpv_node value to mpv, and when you're done you call
     * mpv_free_node_contents(&node).
     * You're expected not to write to the data - if you have to, copy it
     * first (which you have to do manually).
     *
     * For writing, you construct your own mpv_node, and pass a pointer to the
     * API. The API will never write to your data (and copy it if needed), so
     * you're free to use any form of allocation or memory management you like.
     *
     * Warning: when reading, always check the mpv_node.format member. For
     *          example, properties might change their type in future versions
     *          of mpv, or sometimes even during runtime.
     *
     * Example for reading:
     *
     *     mpv_node result;
     *     if (mpv_get_property(ctx, "property", MPV_FORMAT_NODE, &result) < 0)
     *         goto error;
     *     printf("format=%d\n", (int)result.format);
     *     mpv_free_node_contents(&result).
     *
     * Example for writing:
     *
     *     mpv_node value;
     *     value.format = MPV_FORMAT_STRING;
     *     value.u.string = "hello";
     *     mpv_set_property(ctx, "property", MPV_FORMAT_NODE, &value);
     */
    MPV_FORMAT_NODE             = 6,
    /**
     * Used with mpv_node only. Can usually not be used directly.
     */
    MPV_FORMAT_NODE_ARRAY       = 7,
    /**
     * See MPV_FORMAT_NODE_ARRAY.
     */
    MPV_FORMAT_NODE_MAP         = 8
} mpv_format;

/**
 * Generic data storage.
 *
 * If mpv writes this struct (e.g. via mpv_get_property()), you must not change
 * the data. You have to free it with mpv_free_node_contents().
 */
typedef struct mpv_node {
    union {
        char *string;   /** valid if format==MPV_FORMAT_STRING */
        int flag;       /** valid if format==MPV_FORMAT_FLAG   */
        int64_t int64;  /** valid if format==MPV_FORMAT_INT64  */
        double double_; /** valid if format==MPV_FORMAT_DOUBLE */
        /**
         * valid if format==MPV_FORMAT_NODE_ARRAY
         *    or if format==MPV_FORMAT_NODE_MAP
         */
        struct mpv_node_list *list;
    } u;
    /**
     * Type of the data stored in this struct. This value rules what members in
     * the given union can be accessed. The following formats are currently
     * defined to be allowed in mpv_node:
     *
     *  MPV_FORMAT_STRING       (u.string)
     *  MPV_FORMAT_FLAG         (u.flag)
     *  MPV_FORMAT_INT64        (u.int64)
     *  MPV_FORMAT_DOUBLE       (u.double_)
     *  MPV_FORMAT_NODE_ARRAY   (u.list)
     *  MPV_FORMAT_NODE_MAP     (u.list)
     *  MPV_FORMAT_NONE         (no member)
     *
     * If you encounter a value you don't know, you must not make any
     * assumptions about the contents of union u.
     */
    mpv_format format;
} mpv_node;

/**
 * (see mpv_node)
 */
typedef struct mpv_node_list {
    /**
     * Number of entries. Negative values are not allowed.
     */
    int num;
    /**
     * MPV_FORMAT_NODE_ARRAY:
     *  values[N] refers to value of the Nth item
     *
     * MPV_FORMAT_NODE_MAP:
     *  values[N] refers to value of the Nth key/value pair
     *
     * If num > 0, values[0] to values[num-1] (inclusive) are valid.
     * Otherwise, this can be NULL.
     */
    mpv_node *values;
    /**
     * MPV_FORMAT_NODE_ARRAY:
     *  unused (typically NULL), access is not allowed
     *
     * MPV_FORMAT_NODE_MAP:
     *  keys[N] refers to key of the Nth key/value pair. If num > 0, keys[0] to
     *  keys[num-1] (inclusive) are valid. Otherwise, this can be NULL.
     *  The keys are in random order. The only guarantee is that keys[N] belongs
     *  to the value values[N]. NULL keys are not allowed.
     */
    char **keys;
} mpv_node_list;

/**
 * Frees any data referenced by the node. It doesn't free the node itself.
 * Call this only if the mpv client API set the node. If you constructed the
 * node yourself (manually), you have to free it yourself.
 */
void mpv_free_node_contents(mpv_node *node);

/**
 * Set an option. Note that you can't normally set options during runtime. It
 * works in uninitialized state (see mpv_create()), and in some cases in at
 * runtime.
 *
 * Changing options at runtime does not always work. For some options, attempts
 * to change them simply fails. Many other options may require reloading the
 * file for changes to take effect. In general, you should prefer calling
 * mpv_set_property() to change settings during playback, because the property
 * mechanism guarantees that changes take effect immediately.
 *
 * @param name Option name. This is the same as on the mpv command line, but
 *             without the leading "--".
 * @param format see enum mpv_format. Currently, only MPV_FORMAT_STRING is valid.
 * @param[in] data Option value (according to the format).
 * @return error code
 */
int mpv_set_option(mpv_handle *ctx, const char *name, mpv_format format,
                   void *data);

/**
 * Convenience function to set an option to a string value. This is like
 * calling mpv_set_option() with MPV_FORMAT_STRING.
 *
 * @return error code
 */
int mpv_set_option_string(mpv_handle *ctx, const char *name, const char *data);

/**
 * Send a command to the player. Commands are the same as those used in
 * input.conf, except that this function takes parameters in a pre-split
 * form.
 *
 * The commands and their parameters are documented in input.rst.
 *
 * Caveat: currently, commands do not report whether they run successfully. If
 *         the command exists and its arguments are not broken, always success
 *         will be returned.
 *
 * @param[in] args NULL-terminated list of strings. Usually, the first item
 *                 is the command, and the following items are arguments.
 * @return error code
 */
int mpv_command(mpv_handle *ctx, const char **args);

/**
 * Same as mpv_command, but use input.conf parsing for splitting arguments.
 * This is slightly simpler, but also more error prone, since arguments may
 * need quoting/escaping.
 */
int mpv_command_string(mpv_handle *ctx, const char *args);

/**
 * Same as mpv_command, but run the command asynchronously.
 *
 * Commands are executed asynchronously. You will receive a
 * MPV_EVENT_COMMAND_REPLY event. (This event will also have an
 * error code set if running the command failed.)
 *
 * @param reply_userdata see section about asynchronous calls
 * @param args NULL-terminated list of strings (see mpv_command())
 * @return error code
 */
int mpv_command_async(mpv_handle *ctx, uint64_t reply_userdata,
                      const char **args);

/**
 * Set a property to a given value. Properties are essentially variables which
 * can be queried or set at runtime. For example, writing to the pause property
 * will actually pause or unpause playback.
 *
 * If the format doesn't match with the internal format of the property, access
 * usually will fail with MPV_ERROR_PROPERTY_FORMAT. In some cases, the data
 * is automatically converted and access succeeds. For example, MPV_FORMAT_INT64
 * is always converted to MPV_FORMAT_DOUBLE, and access using MPV_FORMAT_STRING
 * usually invokes a string formatter.
 *
 * @param name The property name. See input.rst for a list of properties.
 * @param format see enum mpv_format. Currently, only MPV_FORMAT_STRING is valid.
 * @param[in] data Option value.
 * @return error code
 */
int mpv_set_property(mpv_handle *ctx, const char *name, mpv_format format,
                     void *data);

/**
 * Convenience function to set a property to a string value.
 *
 * This is like calling mpv_set_property() with MPV_FORMAT_STRING.
 */
int mpv_set_property_string(mpv_handle *ctx, const char *name, const char *data);

/**
 * Set a property asynchronously. You will receive the result of the operation
 * as MPV_EVENT_SET_PROPERTY_REPLY event. The mpv_event.error field will contain
 * the result status of the operation. Otherwise, this function is similar to
 * mpv_set_property().
 *
 * @param reply_userdata see section about asynchronous calls
 * @param name The property name.
 * @param format see enum mpv_format. Currently, only MPV_FORMAT_STRING is valid.
 * @param[in] data Option value. The value will be copied by the function. It
 *                 will never be modified by the client API.
 * @return error code if sending the request failed
 */
int mpv_set_property_async(mpv_handle *ctx, uint64_t reply_userdata,
                           const char *name, mpv_format format, void *data);

/**
 * Read the value of the given property.
 *
 * If the format doesn't match with the internal format of the property, access
 * usually will fail with MPV_ERROR_PROPERTY_FORMAT. In some cases, the data
 * is automatically converted and access succeeds. For example, MPV_FORMAT_INT64
 * is always converted to MPV_FORMAT_DOUBLE, and access using MPV_FORMAT_STRING
 * usually invokes a string parser.
 *
 * @param name The property name.
 * @param format see enum mpv_format.
 * @param[out] data Pointer to the variable holding the option value. On
 *                  success, the variable will be set to a copy of the option
 *                  value. For formats that require dynamic memory allocation,
 *                  you can free the value with mpv_free() (strings) or
 *                  mpv_free_node_contents() (MPV_FORMAT_NODE).
 * @return error code
 */
int mpv_get_property(mpv_handle *ctx, const char *name, mpv_format format,
                     void *data);

/**
 * Return the value of the property with the given name as string. This is
 * equivalent to mpv_get_property() with MPV_FORMAT_STRING.
 *
 * See MPV_FORMAT_STRING for character encoding issues.
 *
 * On error, NULL is returned. Use mpv_get_property() if you want fine-grained
 * error reporting.
 *
 * @param name The property name.
 * @return Property value, or NULL if the property can't be retrieved. Free
 *         the string with mpv_free().
 */
char *mpv_get_property_string(mpv_handle *ctx, const char *name);

/**
 * Return the property as "OSD" formatted string. This is the same as
 * mpv_get_property_string, but using MPV_FORMAT_OSD_STRING.
 *
 * @return Property value, or NULL if the property can't be retrieved. Free
 *         the string with mpv_free().
 */
char *mpv_get_property_osd_string(mpv_handle *ctx, const char *name);

/**
 * Get a property asynchronously. You will receive the result of the operation
 * as well as the property data with the MPV_EVENT_GET_PROPERTY_REPLY event.
 * You should check the mpv_event.error field on the reply event.
 *
 * @param reply_userdata see section about asynchronous calls
 * @param name The property name.
 * @param format see enum mpv_format.
 * @return error code if sending the request failed
 */
int mpv_get_property_async(mpv_handle *ctx, uint64_t reply_userdata,
                           const char *name, mpv_format format);

typedef enum mpv_event_id {
    /**
     * Nothing happened. Happens on timeouts or sporadic wakeups.
     */
    MPV_EVENT_NONE              = 0,
    /**
     * Happens when the player quits. The player enters a state where it tries
     * to disconnect all clients. Most requests to the player will fail, and
     * mpv_wait_event() will always return instantly (returning new shutdown
     * events if no other events are queued). The client should react to this
     * and quit with mpv_destroy() as soon as possible.
     */
    MPV_EVENT_SHUTDOWN          = 1,
    /**
     * See mpv_request_log_messages().
     */
    MPV_EVENT_LOG_MESSAGE       = 2,
    /**
     * Reply to a mpv_get_property_async() request.
     * See also mpv_event and mpv_event_property.
     */
    MPV_EVENT_GET_PROPERTY_REPLY = 3,
    /**
     * Reply to a mpv_set_property_async() request.
     * (Unlike MPV_EVENT_GET_PROPERTY, mpv_event_property is not used.)
     */
    MPV_EVENT_SET_PROPERTY_REPLY = 4,
    /**
     * Reply to a mpv_command_async() request.
     */
    MPV_EVENT_COMMAND_REPLY     = 5,
    /**
     * Notification before playback start of a file.
     */
    MPV_EVENT_START_FILE        = 6,
    /**
     * Notification after playback end (after the file was unloaded).
     */
    MPV_EVENT_END_FILE          = 7,
    /**
     * Notification when the file has been loaded (headers were read etc.), and
     * decoding starts.
     */
    MPV_EVENT_FILE_LOADED       = 8,
    /**
     * The list of video/audio/subtitle tracks was changed.
     */
    MPV_EVENT_TRACKS_CHANGED    = 9,
    /**
     * A video/audio/subtitle track was switched on or off.
     */
    MPV_EVENT_TRACK_SWITCHED    = 10,
    /**
     * Idle mode was entered. In this mode, no file is played, and the playback
     * core waits for new commands. (The command line player normally quits
     * instead of entering idle mode, unless --idle was specified. If mpv
     * was started with mpv_create(), idle mode is enabled by default.)
     */
    MPV_EVENT_IDLE              = 11,
    /**
     * Playback was paused.
     */
    MPV_EVENT_PAUSE             = 12,
    /**
     * Playback was unpaused.
     */
    MPV_EVENT_UNPAUSE           = 13,
    /**
     * Sent every time after a video frame is displayed (or in lower frequency
     * if there is no video, or playback is paused).
     */
    MPV_EVENT_TICK              = 14,
    /**
     * Triggered by the script_dispatch input command. The command uses the
     * client name (see mpv_client_name()) to dispatch keyboard or mouse input
     * to a client.
     */
    MPV_EVENT_SCRIPT_INPUT_DISPATCH = 15,
    /**
     * Triggered by the script_message input command. The command uses the
     * first argument of the command as client name (see mpv_client_name()) to
     * dispatch the message, and passes along the all arguments starting from
     * the seconand argument as strings.
     */
    MPV_EVENT_CLIENT_MESSAGE    = 16,
    /**
     * Happens after video changed in some way. This can happen on resolution
     * changes, pixel format changes, or video filter changes. The event is
     * sent after the video filters and the VO are reconfigured. Applications
     * embedding a mpv window should listen to this event in order to resize
     * the window if needed.
     * Note that this event can happen sporadically, and you should check
     * yourself whether the video parameters really changed before doing
     * something expensive.
     */
    MPV_EVENT_VIDEO_RECONFIG    = 17,
    /**
     * Similar to MPV_EVENT_VIDEO_RECONFIG. This is relatively uninteresting,
     * because there is no such thing as audio output embedding.
     */
    MPV_EVENT_AUDIO_RECONFIG    = 18,
    /**
     * Happens when metadata (like file tags) is possibly updated. (It's left
     * unspecified whether this happens on file start or only when it changes
     * within a file.)
     */
    MPV_EVENT_METADATA_UPDATE   = 19,
    /**
     * Happens when a seek was initiated. Playback stops. Usually it will
     * resume with MPV_EVENT_PLAYBACK_RESTART as soon as the seek is finished.
     */
    MPV_EVENT_SEEK              = 20,
    /**
     * There was a discontinuity of some sort (like a seek), and playback
     * was reinitialized. Usually happens after seeking, or ordered chapter
     * segment switches. The main purpose is allowing the client to detect
     * when a seek request is finished.
     */
    MPV_EVENT_PLAYBACK_RESTART  = 21
} mpv_event_id;

/**
 * Return a string describing the event. For unknown events, NULL is returned.
 *
 * Note that all events actually returned by the API will also yield a non-NULL
 * string with this function.
 *
 * @param event event ID, see see enum mpv_event_id
 * @return A static string giving a short symbolic name of the event. It
 *         consists of lower-case alphanumeric characters and can include "-"
 *         characters. This string is suitable for use in e.g. scripting
 *         interfaces.
 *         The string is completely static, i.e. doesn't need to be deallocated,
 *         and is valid forever.
 */
const char *mpv_event_name(mpv_event_id event);

typedef struct mpv_event_property {
    /**
     * Name of the property.
     */
    const char *name;
    /**
     * Format of the given data. See enum mpv_format.
     * This is always the same format as the requested format.
     */
    mpv_format format;
    /**
     * Received property value. Depends on the format. This is like the
     * pointer argument passed to mpv_get_property().
     *
     * For example, for MPV_FORMAT_STRING you get the string with:
     *
     *    char *value = *(char **)(event_property->data);
     *
     * Note that this is set to NULL if retrieving the property failed.
     * See mpv_event.error for the status.
     */
    void *data;
} mpv_event_property;

typedef struct mpv_event_log_message {
    /**
     * The module prefix, identifies the sender of the message.
     */
    const char *prefix;
    /**
     * The log level as string. See mpv_request_log_messages() for possible
     * values.
     */
    const char *level;
    /**
     * The log message. Note that this is the direct output of a printf()
     * style output API. The text will contain embedded newlines, and it's
     * possible that a single message contains multiple lines, or that a
     * message contains a partial line.
     *
     * It's safe to display messages only if they end with a newline character,
     * and to buffer them otherwise.
     */
    const char *text;
} mpv_event_log_message;

typedef struct mpv_event_pause_reason {
    /**
     * Actual pause state (0 or 1)
     */
    int real_paused;
    /**
     * User requested pause state (0 or 1)
     */
    int user_paused;
    /**
     * 1 if the action was triggered by an input command (or via an user key
     * binding), 0 otherwise.
     */
    int by_command;
    /**
     * 1 if the action was triggered by a low (or recovering) cache state,
     * 0 otherwise.
     */
    int by_cache;
    /**
     * 1 if the pausing was triggered because the end of playback was reached,
     * and the "keep-open" option is enabled, 0 otherwise.
     */
    int by_keep_open;
} mpv_event_pause_reason;

typedef struct mpv_event_script_input_dispatch {
    /**
     * Arbitrary integer value that was provided as argument to the
     * script_dispatch input command.
     */
    int arg0;
    /**
     * Type of the input. Currently either "keyup_follows" (basically a key
     * down event), or "press" (either a single key event, or a key up event
     * following a "keyup_follows" event).
     */
    const char *type;
} mpv_event_script_input_dispatch;

typedef struct mpv_event_client_message {
    /**
     * Arbitrary arguments chosen by the sender of the message. If num_args > 0,
     * you can access args[0] through args[num_args - 1] (inclusive). What
     * these arguments mean is up to the sender and receiver.
     * None of the valid items is NULL.
     */
    int num_args;
    const char **args;
} mpv_event_client_message;

typedef struct mpv_event {
    /**
     * One of mpv_event. Keep in mind that later ABI compatible releases might
     * add new event types. These should be ignored by the API user.
     */
    mpv_event_id event_id;
    /**
     * This is mainly used for events that are replies to (asynchronous)
     * requests. It contains a status code, which is >= 0 on success, or < 0
     * on error (a mpv_error value). Usually, this will be set if an
     * asynchronous request fails.
     */
    int error;
    /**
     * If the event is in reply to a request (made with this API and this
     * API handle), this is set to the reply_userdata parameter of the request
     * call.
     * Otherwise, this field is 0.
     */
    uint64_t reply_userdata;
    /**
     * The meaning and contents of data member depend on the event_id:
     *  MPV_EVENT_GET_PROPERTY_REPLY:     mpv_event_property*
     *  MPV_EVENT_LOG_MESSAGE:            mpv_event_log_message*
     *  MPV_EVENT_PAUSE:                  mpv_event_pause_reason*
     *  MPV_EVENT_UNPAUSE:                mpv_event_pause_reason*
     *  MPV_EVENT_SCRIPT_INPUT_DISPATCH:  mpv_event_script_input_dispatch*
     *  MPV_EVENT_CLIENT_MESSAGE:         mpv_event_client_message*
     *  other: NULL
     *
     * Note: future enhancements might add new event structs for existing or new
     *       event types.
     */
    void *data;
} mpv_event;

/**
 * Enable or disable the given event.
 *
 * Some events are enabled by default. Some events can't be disabled.
 *
 * (Informational note: currently, all events are enabled by default, except
 *  MPV_EVENT_TICK.)
 *
 * @param event See enum mpv_event_id.
 * @param enable 1 to enable receiving this event, 0 to disable it.
 * @return error code
 */
int mpv_request_event(mpv_handle *ctx, mpv_event_id event, int enable);

/**
 * Enable or disable receiving of log messages. These are the messages the
 * command line player prints to the terminal. This call sets the minimum
 * required log level for a message to be received with MPV_EVENT_LOG_MESSAGE.
 *
 * @param min_level Minimal log level as string. Valid log levels:
 *                      no fatal error warn info status v debug trace
 *                  The value "no" disables all messages. This is the default.
 */
int mpv_request_log_messages(mpv_handle *ctx, const char *min_level);

/**
 * Wait for the next event, or until the timeout expires, or if another thread
 * makes a call to mpv_wakeup(). Passing 0 as timeout will never wait, and
 * is suitable for polling.
 *
 * The internal event queue has a limited size (per client handle). If you
 * don't empty the event queue quickly enough with mpv_wait_event(), it will
 * overflow and silently discard further events. If this happens, making
 * asynchronous requests will fail as well (with MPV_ERROR_EVENT_QUEUE_FULL).
 *
 * Only one thread is allowed to call this at a time. The API won't complain
 * if more than one thread calls this, but it will cause race conditions in
 * the client when accessing the shared mpv_event struct. Note that most other
 * API functions are not restricted by this, and no API function internally
 * calls mpv_wait_event().
 *
 * @param timeout Timeout in seconds, after which the function returns even if
 *                no event was received. A MPV_EVENT_NONE is returned on
 *                timeout. Values <= 0 will disable waiting.
 * @return A struct containing the event ID and other data. The pointer (and
 *         fields in the struct) stay valid until the next mpv_wait_event()
 *         call, or until mpv_destroy() is called. You must not write to
 *         the struct, and all memory referenced by it will be automatically
 *         released by the API. The return value is never NULL.
 */
mpv_event *mpv_wait_event(mpv_handle *ctx, double timeout);

/**
 * Interrupt the current mpv_wait_event() call. This will wake up the thread
 * currently waiting in mpv_wait_event(). If no thread is waiting, the next
 * mpv_wait_event() call will return immediately (this is to avoid lost
 * wakeups).
 *
 * mpv_wait_event() will receive a MPV_EVENT_NONE if it's woken up due to
 * this call. But note that this dummy event might be skipped if there are
 * already another events queued. All what counts is that the waiting thread
 * is woken up.
 */
void mpv_wakeup(mpv_handle *ctx);

/**
 * Set a custom function that should be called when there are new events. Use
 * this if blocking in mpv_wait_event() to wait for new events is not feasible.
 *
 * Keep in mind that the callback will be called from foreign threads. You
 * must not make any assumptions of the environment, and you must return as
 * soon as possible. You are not allowed to call any client API functions
 * inside of the callback. In particular, you should not do any processing in
 * the callback, but wake up another thread that does all the work.
 *
 * In general, the client API expects you to call mpv_wait_event() to receive
 * notifications, and the wakeup callback is merely a helper utility to make
 * this easier in certain situations.
 *
 * If you actually want to do processing in a callback, spawn a thread that
 * does nothing but call mpv_wait_event() in a loop and dispatches the result
 * to a callback.
 */
void mpv_set_wakeup_callback(mpv_handle *ctx, void (*cb)(void *d), void *d);

#ifdef __cplusplus
}
#endif

#endif
