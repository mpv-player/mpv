/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_M_OPTION_H
#define MPLAYER_M_OPTION_H

#include <float.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#include "misc/bstr.h"
#include "audio/chmap.h"
#include "common/common.h"

// m_option allows to parse, print and copy data of various types.

typedef struct m_option_type m_option_type_t;
typedef struct m_option m_option_t;
struct m_config;
struct mp_log;
struct mpv_node;
struct mpv_global;

///////////////////////////// Options types declarations ////////////////////

// Simple types
extern const m_option_type_t m_option_type_bool;
extern const m_option_type_t m_option_type_flag;
extern const m_option_type_t m_option_type_dummy_flag;
extern const m_option_type_t m_option_type_int;
extern const m_option_type_t m_option_type_int64;
extern const m_option_type_t m_option_type_byte_size;
extern const m_option_type_t m_option_type_float;
extern const m_option_type_t m_option_type_double;
extern const m_option_type_t m_option_type_string;
extern const m_option_type_t m_option_type_string_list;
extern const m_option_type_t m_option_type_string_append_list;
extern const m_option_type_t m_option_type_keyvalue_list;
extern const m_option_type_t m_option_type_time;
extern const m_option_type_t m_option_type_rel_time;
extern const m_option_type_t m_option_type_choice;
extern const m_option_type_t m_option_type_flags;
extern const m_option_type_t m_option_type_msglevels;
extern const m_option_type_t m_option_type_print_fn;
extern const m_option_type_t m_option_type_imgfmt;
extern const m_option_type_t m_option_type_fourcc;
extern const m_option_type_t m_option_type_afmt;
extern const m_option_type_t m_option_type_color;
extern const m_option_type_t m_option_type_geometry;
extern const m_option_type_t m_option_type_size_box;
extern const m_option_type_t m_option_type_channels;
extern const m_option_type_t m_option_type_aspect;
extern const m_option_type_t m_option_type_obj_settings_list;
extern const m_option_type_t m_option_type_node;
extern const m_option_type_t m_option_type_rect;
extern const m_option_type_t m_option_type_cycle_dir;

// Used internally by m_config.c
extern const m_option_type_t m_option_type_alias;
extern const m_option_type_t m_option_type_cli_alias;
extern const m_option_type_t m_option_type_removed;
extern const m_option_type_t m_option_type_subconfig;

// Callback used by m_option_type_print_fn options.
typedef void (*m_opt_print_fn)(struct mp_log *log);

enum m_rel_time_type {
    REL_TIME_NONE,
    REL_TIME_ABSOLUTE,
    REL_TIME_RELATIVE,
    REL_TIME_PERCENT,
    REL_TIME_CHAPTER,
};

struct m_rel_time {
    double pos;
    enum m_rel_time_type type;
};

struct m_color {
    uint8_t r, g, b, a;
};

struct m_geometry {
    int x, y, w, h;
    bool xy_valid : 1, wh_valid : 1;
    bool w_per : 1, h_per : 1;
    bool x_sign : 1, y_sign : 1, x_per : 1, y_per : 1;
    int ws; // workspace; valid if !=0
};

void m_geometry_apply(int *xpos, int *ypos, int *widw, int *widh,
                      int scrw, int scrh, bool center, struct m_geometry *gm);
void m_rect_apply(struct mp_rect *rc, int w, int h, struct m_geometry *gm);

struct m_channels {
    bool set : 1;
    bool auto_safe : 1;
    struct mp_chmap *chmaps;
    int num_chmaps;
};

struct m_obj_desc {
    // Name which will be used in the option string
    const char *name;
    // Will be printed when "help" is passed
    const char *description;
    // Size of the private struct
    int priv_size;
    // If not NULL, default values for private struct
    const void *priv_defaults;
    // Options which refer to members in the private struct
    const struct m_option *options;
    // Prefix for each of the above options (none if NULL).
    const char *options_prefix;
    // For free use by the implementer of m_obj_list.get_desc
    const void *p;
    // Don't list entry with "help"
    bool hidden;
    // Callback to print custom help if "vf=entry=help" is passed
    void (*print_help)(struct mp_log *log);
    // Set by m_obj_list_find(). If the requested name is an old alias, this
    // is set to the old name (while the name field uses the new name).
    const char *replaced_name;
    // For convenience: these are added as global command-line options.
    const struct m_sub_options *global_opts;
};

// Extra definition needed for \ref m_option_type_obj_settings_list options.
struct m_obj_list {
    bool (*get_desc)(struct m_obj_desc *dst, int index);
    const char *description;
    // Can be set to a NULL terminated array of aliases
    const char *aliases[5][2];
    // Allow a trailing ",", which adds an entry with name=""
    bool allow_trailer;
    // Callback to test whether an unknown entry should be allowed. (This can
    // be useful if adding them as explicit entries is too much work.)
    bool (*check_unknown_entry)(const char *name);
    // Allow syntax for disabling entries.
    bool allow_disable_entries;
    // This helps with confusing error messages if unknown flag options are used.
    bool disallow_positional_parameters;
    // Each sub-item is backed by global options (for AOs and VOs).
    bool use_global_options;
    // Callback to print additional custom help if "vf=help" is passed
    void (*print_help_list)(struct mp_log *log);
    // Callback to print help for _unknown_ entries with "vf=entry=help"
    void (*print_unknown_entry_help)(struct mp_log *log, const char *name);
    // Get lavfi filters for option-info/[av]f/choices.
    const char **(*get_lavfi_filters)(void *talloc_ctx);
};

// Find entry by name
bool m_obj_list_find(struct m_obj_desc *dst, const struct m_obj_list *list,
                     bstr name);

// The data type used by \ref m_option_type_obj_settings_list.
typedef struct m_obj_settings {
    // Type of the object.
    char *name;
    // Optional user-defined name.
    char *label;
    // User enable flag.
    bool enabled;
    // NULL terminated array of parameter/value pairs.
    char **attribs;
} m_obj_settings_t;

bool m_obj_settings_equal(struct m_obj_settings *a, struct m_obj_settings *b);

struct m_opt_choice_alternatives {
    char *name;
    int value;
};

const char *m_opt_choice_str(const struct m_opt_choice_alternatives *choices,
                             int value);

typedef int (*m_opt_generic_validate_fn)(struct mp_log *log, const m_option_t *opt,
                                         struct bstr name, void *value);

#define OPT_FUNC(name) name
#define OPT_FUNC_IN(name, suffix) name ## _ ## suffix
#define OPT_VALIDATE_FUNC(func, value_type, suffix) \
int OPT_FUNC(func)(struct mp_log *log, const m_option_t *opt, \
                   struct bstr name, value_type value); \
static inline int OPT_FUNC_IN(func, suffix)(struct mp_log *log, const m_option_t *opt, \
                                            struct bstr name, void *value) { \
    return OPT_FUNC(func)(log, opt, name, value); \
} \
int OPT_FUNC(func)(struct mp_log *log, const m_option_t *opt, \
                   struct bstr name, value_type value)

// m_option.priv points to this if OPT_SUBSTRUCT is used
struct m_sub_options {
    const char *prefix;
    const struct m_option *opts;
    size_t size;
    const void *defaults;
    // Change flags passed to mp_option_change_callback() if any option that is
    // directly or indirectly part of this group is changed.
    uint64_t change_flags;
    // Return further sub-options, for example for optional components. If set,
    // this is called with increasing index (starting from 0), as long as true
    // is returned. If true is returned and *sub is set in any of these calls,
    // they are added as options.
    bool (*get_sub_options)(int index, const struct m_sub_options **sub);
};

#define CONF_TYPE_BOOL          (&m_option_type_bool)
#define CONF_TYPE_FLAG          (&m_option_type_flag)
#define CONF_TYPE_INT           (&m_option_type_int)
#define CONF_TYPE_INT64         (&m_option_type_int64)
#define CONF_TYPE_FLOAT         (&m_option_type_float)
#define CONF_TYPE_DOUBLE        (&m_option_type_double)
#define CONF_TYPE_STRING        (&m_option_type_string)
#define CONF_TYPE_STRING_LIST   (&m_option_type_string_list)
#define CONF_TYPE_IMGFMT        (&m_option_type_imgfmt)
#define CONF_TYPE_FOURCC        (&m_option_type_fourcc)
#define CONF_TYPE_AFMT          (&m_option_type_afmt)
#define CONF_TYPE_OBJ_SETTINGS_LIST (&m_option_type_obj_settings_list)
#define CONF_TYPE_TIME          (&m_option_type_time)
#define CONF_TYPE_CHOICE        (&m_option_type_choice)
#define CONF_TYPE_NODE          (&m_option_type_node)

// Possible option values. Code is allowed to access option data without going
// through this union. It serves for self-documentation and to get minimal
// size/alignment requirements for option values in general.
union m_option_value {
    bool bool_;
    int flag; // not the C type "bool"!
    int int_;
    int64_t int64;
    float float_;
    double double_;
    char *string;
    char **string_list;
    char **keyvalue_list;
    int imgfmt;
    unsigned int fourcc;
    int afmt;
    m_obj_settings_t *obj_settings_list;
    double time;
    struct m_rel_time rel_time;
    struct m_color color;
    struct m_geometry geometry;
    struct m_geometry size_box;
    struct m_channels channels;
};

// Keep fully zeroed instance of m_option_value to use as a default value, before
// any specific union member is used. C standard says that `= {0}` activates and
// initializes only the first member of the union, leaving padding bits undefined.
static const union m_option_value m_option_value_default;

////////////////////////////////////////////////////////////////////////////

struct m_option_action {
    // The name of the suffix, e.g. "add" for a list. If the option is named
    // "foo", this will be available as "--foo-add". Note that no suffix (i.e.
    // "--foo" is implicitly always available.
    const char *name;
    // One of M_OPT_TYPE*.
    unsigned int flags;
};

// Option type description
struct m_option_type {
    const char *name;
    // Size needed for the data.
    unsigned int size;
    // One of M_OPT_TYPE*.
    unsigned int flags;

    // Parse the data from a string.
    /** It is the only required function, all others can be NULL.
     *  Generally should not be called directly outside of the options module,
     *  but instead through \ref m_option_parse which calls additional option
     *  specific callbacks during the process.
     *
     *  \param log for outputting parser error or help messages
     *  \param opt The option that is parsed.
     *  \param name The full option name.
     *  \param param The parameter to parse.
     *         may not be an argument meant for this option
     *  \param dst Pointer to the memory where the data should be written.
     *             If NULL the parameter validity should still be checked.
     *  \return On error a negative value is returned, on success the number
     *          of arguments consumed. For details see \ref OptionParserReturn.
     */
    int (*parse)(struct mp_log *log, const m_option_t *opt,
                 struct bstr name, struct bstr param, void *dst);

    // Print back a value in string form.
    /** \param opt The option to print.
     *  \param val Pointer to the memory holding the data to be printed.
     *  \return An allocated string containing the text value or (void*)-1
     *          on error.
     */
    char *(*print)(const m_option_t *opt, const void *val);

    // Print the value in a human readable form. Unlike print(), it doesn't
    // necessarily return the exact value, and is generally not parseable with
    // parse().
    char *(*pretty_print)(const m_option_t *opt, const void *val);

    // Copy data between two locations. Deep copy if the data has pointers.
    // The implementation must free *dst if memory allocation is involved.
    /** \param opt The option to copy.
     *  \param dst Pointer to the destination memory.
     *  \param src Pointer to the source memory.
     */
    void (*copy)(const m_option_t *opt, void *dst, const void *src);

    // Free the data allocated for a save slot.
    /** This is only needed for dynamic types like strings.
     *  \param dst Pointer to the data, usually a pointer that should be freed and
     *             set to NULL.
     */
    void (*free)(void *dst);

    // Add the value add to the value in val. For types that are not numeric,
    // add gives merely the direction. The wrap parameter determines whether
    // the value is clipped, or wraps around to the opposite max/min.
    void (*add)(const m_option_t *opt, void *val, double add, bool wrap);

    // Multiply the value with the factor f. The callback must clip the result
    // to the valid value range of the option.
    void (*multiply)(const m_option_t *opt, void *val, double f);

    // Set the option value in dst to the contents of src.
    // (If the option is dynamic, the old value in *dst has to be freed.)
    // Return values:
    //  M_OPT_UNKNOWN:      src is in an unknown format
    //  M_OPT_INVALID:      src is incorrectly formatted
    //  >= 0:               success
    //  other error code:   some other error, essentially M_OPT_INVALID refined
    int (*set)(const m_option_t *opt, void *dst, struct mpv_node *src);

    // Copy the option value in src to dst. Use ta_parent for any dynamic
    // memory allocations. It's explicitly allowed to have mpv_node reference
    // static strings (and even mpv_node_list.keys), though.
    int (*get)(const m_option_t *opt, void *ta_parent, struct mpv_node *dst,
               void *src);

    // Return whether the values are the same. (There are no "unordered"
    // results; for example, two floats with the value NaN compare equal. Other
    // ambiguous floats, such as +0 and -0 compare equal. Some option types may
    // incorrectly report unequal for values that are equal, such as sets (if
    // the element order is different, which incorrectly matters), but values
    // duplicated with m_option_copy() always return as equal. Empty strings
    // and NULL strings are equal. Ambiguous unicode representations compare
    // unequal.)
    // If not set, values are always considered equal (=> not really optional).
    bool (*equal)(const m_option_t *opt, void *a, void *b);

    // Optional: list of suffixes, terminated with a {0} entry. An empty list
    // behaves like the list being NULL.
    const struct m_option_action *actions;
};

// Option description
struct m_option {
    // Option name.
    // Option declarations can use this as positional field.
    const char *name;

    // Option type.
    const m_option_type_t *type;

    // See \ref OptionFlags.
    uint64_t flags;

    // Always force an option update even if the written value does not change.
    bool force_update;

    // If the option is an alias, use the prefix of sub option.
    bool alias_use_prefix;

    int offset;

    // Most numeric types restrict the range to [min, max] if min<max (this
    // implies that if min/max are not set, the full range is used). In all
    // cases, the actual range is clamped to the type's native range.
    // Float type uses [FLT_MIN, FLT_MAX], and double type uses
    // [DBL_MIN, DBL_MAX], though by setting min or max to -/+INFINITY,
    // the range can be extended to INFINITY.
    // Preferably use M_RANGE() to set these fields.
    double min, max;

    // Type dependent data (for all kinds of extended settings).
    void *priv;

    // Initialize variable to given default before parsing options
    const void *defval;

    // Print a warning when this option is used (for options with no direct
    // replacement.)
    const char *deprecation_message;

    // Optional function that validates a param value for this option.
    m_opt_generic_validate_fn validate;

    // Optional function that displays help. Will replace type-specific help.
    int (*help)(struct mp_log *log, const m_option_t *opt, struct bstr name);
};

char *format_file_size(int64_t size);

// The following are also part of the M_OPT_* flags, and are used to update
// certain groups of options.
#define UPDATE_TERM             (UINT64_C(1) << 0)   // terminal options
#define UPDATE_SUB_FILT         (UINT64_C(1) << 1)   // subtitle filter options
#define UPDATE_OSD              (UINT64_C(1) << 2)   // related to OSD rendering
#define UPDATE_BUILTIN_SCRIPTS  (UINT64_C(1) << 3)   // osc/ytdl/stats
#define UPDATE_IMGPAR           (UINT64_C(1) << 4)   // video image params overrides
#define UPDATE_INPUT            (UINT64_C(1) << 5)   // mostly --input-* options
#define UPDATE_AUDIO            (UINT64_C(1) << 6)   // --audio-channels etc.
#define UPDATE_PRIORITY         (UINT64_C(1) << 7)   // --priority (Windows-only)
#define UPDATE_SCREENSAVER      (UINT64_C(1) << 8)   // --stop-screensaver
#define UPDATE_VOL              (UINT64_C(1) << 9)   // softvol related options
#define UPDATE_LAVFI_COMPLEX    (UINT64_C(1) << 10)  // --lavfi-complex
#define UPDATE_HWDEC            (UINT64_C(1) << 11)  // --hwdec
#define UPDATE_DVB_PROG         (UINT64_C(1) << 12)  // some --dvbin-...
#define UPDATE_SUB_HARD         (UINT64_C(1) << 13)  // subtitle opts. that need full reinit
#define UPDATE_SUB_EXTS         (UINT64_C(1) << 14)  // update internal list of sub exts
#define UPDATE_VIDEO            (UINT64_C(1) << 15)  // force redraw if needed
#define UPDATE_VO               (UINT64_C(1) << 16)  // reinit the VO
#define UPDATE_CLIPBOARD        (UINT64_C(1) << 17)  // reinit the clipboard
#define UPDATE_DEMUXER          (UINT64_C(1) << 18)  // invalidate --prefetch-playlist's data
#define UPDATE_AD               (UINT64_C(1) << 19)  // reinit audio chain and decoder
#define UPDATE_VD               (UINT64_C(1) << 20)  // reinit video chain and decoder
#define UPDATE_OPT_LAST         (UINT64_C(1) << 20)

// All bits between of UPDATE_ flags
#define UPDATE_OPTS_MASK        ((UPDATE_OPT_LAST << 1) - 1)

// The option is forbidden in config files.
#define M_OPT_NOCFG             (UINT64_C(1) << 63)

// The option should be set during command line pre-parsing
#define M_OPT_PRE_PARSE         (UINT64_C(1) << 62)

// The option expects a file name (or a list of file names)
#define M_OPT_FILE              (UINT64_C(1) << 61)

// Do not add as property.
#define M_OPT_NOPROP            (UINT64_C(1) << 60)

// Enable special semantics for some options when parsing the string "help".
#define M_OPT_HAVE_HELP         (UINT64_C(1) << 59)

// type_float/type_double: string "default" is parsed as NaN (and reverse)
#define M_OPT_DEFAULT_NAN       (UINT64_C(1) << 58)

// type time: string "no" maps to MP_NOPTS_VALUE (if unset, NOPTS is rejected)
// and
// parsing: "--no-opt" is parsed as "--opt=no"
#define M_OPT_ALLOW_NO          (UINT64_C(1) << 57)

// type channels: disallow "auto" (still accept ""), limit list to at most 1 item.
#define M_OPT_CHANNELS_LIMITED  (UINT64_C(1) << 56)

// type_float/type_double: controls if pretty print should trim trailing zeros
#define M_OPT_FIXED_LEN_PRINT   (UINT64_C(1) << 55)

// Like M_OPT_TYPE_OPTIONAL_PARAM.
#define M_OPT_OPTIONAL_PARAM    (UINT64_C(1) << 54)

static_assert(!(UPDATE_OPTS_MASK & M_OPT_OPTIONAL_PARAM), "");

// These flags are used to describe special parser capabilities or behavior.

// The parameter is optional and by default no parameter is preferred. If
// ambiguous syntax is used ("--opt value"), the command line parser will
// assume that the argument takes no parameter. In config files, these
// options can be used without "=" and value.
#define M_OPT_TYPE_OPTIONAL_PARAM       (1 << 0)

// Behaves fundamentally like a choice or a superset of it (all allowed string
// values are from a fixed set, although other types of values like numbers
// might be allowed too). E.g. m_option_type_choice and m_option_type_flag.
#define M_OPT_TYPE_CHOICE               (1 << 1)

// When m_option.min/max are set, they denote a value range.
#define M_OPT_TYPE_USES_RANGE           (1 << 2)

///////////////////////////// Parser flags /////////////////////////////////

// OptionParserReturn
//
// On success parsers return a number >= 0.
//
// To indicate that MPlayer should exit without playing anything,
// parsers return M_OPT_EXIT.
//
// On error one of the following (negative) error codes is returned:

// For use by higher level APIs when the option name is invalid.
#define M_OPT_UNKNOWN           -1

// Returned when a parameter is needed but wasn't provided.
#define M_OPT_MISSING_PARAM     -2

// Returned when the given parameter couldn't be parsed.
#define M_OPT_INVALID           -3

// Returned if the value is "out of range". The exact meaning may
// vary from type to type.
#define M_OPT_OUT_OF_RANGE      -4

// The option doesn't take a parameter.
#define M_OPT_DISALLOW_PARAM    -5

// Returned when MPlayer should exit. Used by various help stuff.
#define M_OPT_EXIT              -6

char *m_option_strerror(int code);

// Base function to parse options. Includes calling help and validation
// callbacks. Only when this functionality is for some reason required to not
// happen should the parse function pointer be utilized by itself.
//
// See \ref m_option_type::parse.
int m_option_parse(struct mp_log *log, const m_option_t *opt,
                   struct bstr name, struct bstr param, void *dst);

// Helper to print options, see \ref m_option_type::print.
static inline char *m_option_print(const m_option_t *opt, const void *val_ptr)
{
    if (opt->type->print)
        return opt->type->print(opt, val_ptr);
    else
        return NULL;
}

static inline char *m_option_pretty_print(const m_option_t *opt,
                                          const void *val_ptr,
                                          bool fixed_len)
{
    m_option_t o = *opt;
    if (fixed_len)
        o.flags |= M_OPT_FIXED_LEN_PRINT;
    if (opt->type->pretty_print)
        return opt->type->pretty_print(&o, val_ptr);
    else
        return m_option_print(&o, val_ptr);
}

// Helper around \ref m_option_type::copy.
static inline void m_option_copy(const m_option_t *opt, void *dst,
                                 const void *src)
{
    if (opt->type->copy)
        opt->type->copy(opt, dst, src);
}

// Helper around \ref m_option_type::free.
static inline void m_option_free(const m_option_t *opt, void *dst)
{
    if (opt->type->free)
        opt->type->free(dst);
}

// see m_option_type.set
static inline int m_option_set_node(const m_option_t *opt, void *dst,
                                    struct mpv_node *src)
{
    if (opt->type->set)
        return opt->type->set(opt, dst, src);
    return M_OPT_UNKNOWN;
}

// Call m_option_parse for strings, m_option_set_node otherwise.
int m_option_set_node_or_string(struct mp_log *log, const m_option_t *opt,
                                const char *name, void *dst, struct mpv_node *src);

// see m_option_type.get
static inline int m_option_get_node(const m_option_t *opt, void *ta_parent,
                                    struct mpv_node *dst, void *src)
{
    if (opt->type->get)
        return opt->type->get(opt, ta_parent, dst, src);
    return M_OPT_UNKNOWN;
}

static inline bool m_option_equal(const m_option_t *opt, void *a, void *b)
{
    // Handle trivial equivalence.
    // If not implemented, assume this type has no actual values => always equal.
    if (a == b || !opt->type->equal)
        return true;
    return opt->type->equal(opt, a, b);
}

int m_option_required_params(const m_option_t *opt);

extern const char m_option_path_separator;

// Cause a compilation warning if typeof(expr) != type.
// Should be used with pointer types only.
#define MP_EXPECT_TYPE(type, expr) (0 ? (type)0 : (expr))

// This behaves like offsetof(type, member), but will cause a compilation
// warning if typeof(member) != expected_member_type.
// It uses some trickery to make it compile as expression.
#define MP_CHECKED_OFFSETOF(type, member, expected_member_type)             \
    (offsetof(type, member) + (0 && MP_EXPECT_TYPE(expected_member_type*,   \
                                                   &((type*)0)->member)))

#define OPT_TYPED_FIELD(type_, c_type, field) \
    .type = &type_, \
    .offset = MP_CHECKED_OFFSETOF(OPT_BASE_STRUCT, field, c_type)

#define OPTION_LIST_SEPARATOR ','

#define OPTDEF_STR(s)     .defval = (void *)&(char * const){s}
#define OPTDEF_INT(i)     .defval = (void *)&(const int){i}
#define OPTDEF_INT64(i)   .defval = (void *)&(const int64_t){i}
#define OPTDEF_FLOAT(f)   .defval = (void *)&(const float){f}
#define OPTDEF_DOUBLE(d)  .defval = (void *)&(const double){d}

#define M_RANGE(a, b) .min = (double) (a), .max = (double) (b)

#define OPT_BOOL(field) \
    OPT_TYPED_FIELD(m_option_type_bool, bool, field)

#define OPT_INT(field) \
    OPT_TYPED_FIELD(m_option_type_int, int, field)

#define OPT_INT64(field) \
    OPT_TYPED_FIELD(m_option_type_int64, int64_t, field)

#define OPT_FLOAT(field) \
    OPT_TYPED_FIELD(m_option_type_float, float, field)

#define OPT_DOUBLE(field) \
    OPT_TYPED_FIELD(m_option_type_double, double, field)

#define OPT_STRING(field) \
    OPT_TYPED_FIELD(m_option_type_string, char*, field)

#define OPT_STRINGLIST(field) \
    OPT_TYPED_FIELD(m_option_type_string_list, char**, field)

#define OPT_KEYVALUELIST(field) \
    OPT_TYPED_FIELD(m_option_type_keyvalue_list, char**, field)

#define OPT_PATHLIST(field) \
    OPT_TYPED_FIELD(m_option_type_string_list, char**, field), \
    .priv = (void *)&m_option_path_separator

#define OPT_TIME(field) \
    OPT_TYPED_FIELD(m_option_type_time, double, field)

#define OPT_REL_TIME(field) \
    OPT_TYPED_FIELD(m_option_type_rel_time, struct m_rel_time, field)

#define OPT_COLOR(field) \
    OPT_TYPED_FIELD(m_option_type_color, struct m_color, field)

#define OPT_BYTE_SIZE(field) \
    OPT_TYPED_FIELD(m_option_type_byte_size, int64_t, field)

// (Approximation of x<=SIZE_MAX/2 for m_option.max, which is double.)
#define M_MAX_MEM_BYTES MPMIN((1ULL << 62), (size_t)-1 / 2)

#define OPT_GEOMETRY(field) \
    OPT_TYPED_FIELD(m_option_type_geometry, struct m_geometry, field)

#define OPT_SIZE_BOX(field) \
    OPT_TYPED_FIELD(m_option_type_size_box, struct m_geometry, field)

#define OPT_RECT(field) \
    OPT_TYPED_FIELD(m_option_type_rect, struct m_geometry, field)

#define OPT_TRACKCHOICE(field) \
    OPT_CHOICE(field, {"no", -2}, {"auto", -1}), \
    M_RANGE(0, 8190)

#define OPT_MSGLEVELS(field) \
    OPT_TYPED_FIELD(m_option_type_msglevels, char **, field)

#define OPT_ASPECT(field) \
    OPT_TYPED_FIELD(m_option_type_aspect, double, field)

#define OPT_IMAGEFORMAT(field) \
    OPT_TYPED_FIELD(m_option_type_imgfmt, int, field)

#define OPT_AUDIOFORMAT(field) \
    OPT_TYPED_FIELD(m_option_type_afmt, int, field)

#define OPT_CHANNELS(field) \
    OPT_TYPED_FIELD(m_option_type_channels, struct m_channels, field)

#define OPT_INT_VALIDATE_FUNC(func) OPT_VALIDATE_FUNC(func, const int *, int)

#define OPT_INT_VALIDATE(field, validate_fn) \
    OPT_TYPED_FIELD(m_option_type_int, int, field), \
    .validate = OPT_FUNC_IN(validate_fn, int)

#define OPT_STRING_VALIDATE_FUNC(func) OPT_VALIDATE_FUNC(func, const char **, str)

#define OPT_STRING_VALIDATE(field, validate_fn) \
    OPT_TYPED_FIELD(m_option_type_string, char*, field), \
    .validate = OPT_FUNC_IN(validate_fn, str)

#define M_CHOICES(...) \
    .priv = (void *)&(const struct m_opt_choice_alternatives[]){ __VA_ARGS__, {0}}

// Variant which takes a pointer to struct m_opt_choice_alternatives directly
#define OPT_CHOICE_C(field, choices) \
    OPT_TYPED_FIELD(m_option_type_choice, int, field), \
    .priv = (void *)MP_EXPECT_TYPE(const struct m_opt_choice_alternatives*, choices)

// Variant where you pass a struct m_opt_choice_alternatives initializer
#define OPT_CHOICE(field, ...) \
    OPT_TYPED_FIELD(m_option_type_choice, int, field), \
    M_CHOICES(__VA_ARGS__)

#define OPT_FLAGS(field, ...) \
    OPT_TYPED_FIELD(m_option_type_flags, int, field), \
    M_CHOICES(__VA_ARGS__)

#define OPT_SETTINGSLIST(field, objlist) \
    OPT_TYPED_FIELD(m_option_type_obj_settings_list, m_obj_settings_t*, field), \
    .priv = (void*)MP_EXPECT_TYPE(const struct m_obj_list*, objlist)

#define OPT_FOURCC(field) \
    OPT_TYPED_FIELD(m_option_type_fourcc, int, field)

#define OPT_CYCLEDIR(field) \
    OPT_TYPED_FIELD(m_option_type_cycle_dir, double, field)

// subconf must have the type struct m_sub_options.
// All sub-options are prefixed with "name-" and are added to the current
// (containing) option list.
// If name is "", add the sub-options directly instead.
// "field" refers to the field, that must be a pointer to a field described by
// the subconf struct.
#define OPT_SUBSTRUCT(field, subconf) \
    .offset = offsetof(OPT_BASE_STRUCT, field), \
    .type = &m_option_type_subconfig, .priv = (void*)&subconf

// Non-fields

#define OPT_ALIAS(newname) \
    .type = &m_option_type_alias, .priv = newname, .offset = -1

// If "--optname" was removed, but "--newname" has the same semantics.
// It will be redirected, and a warning will be printed on first use.
#define OPT_REPLACED_MSG(newname, msg) \
    .type = &m_option_type_alias, .priv = newname, \
    .deprecation_message = (msg), .offset = -1

// Same, with a generic deprecation message.
#define OPT_REPLACED(newname) OPT_REPLACED_MSG(newname, "")

// Alias, resolved on the CLI/config file/profile parser level only.
#define OPT_CLI_ALIAS(newname) \
    .type = &m_option_type_cli_alias, .priv = newname, \
    .flags = M_OPT_NOPROP, .offset = -1

// "--optname" doesn't exist, but inform the user about a replacement with msg.
#define OPT_REMOVED(msg) \
    .type = &m_option_type_removed, .priv = msg, \
    .deprecation_message = "", .flags = M_OPT_NOPROP, .offset = -1

#define OPT_PRINT(fn) \
    .flags = M_OPT_NOCFG | M_OPT_PRE_PARSE | M_OPT_NOPROP, \
    .type = &m_option_type_print_fn, \
    .priv = MP_EXPECT_TYPE(m_opt_print_fn, fn), \
    .offset = -1

#endif /* MPLAYER_M_OPTION_H */
