/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_M_OPTION_H
#define MPLAYER_M_OPTION_H

#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#include "misc/bstr.h"
#include "audio/chmap.h"

// m_option allows to parse, print and copy data of various types.

typedef struct m_option_type m_option_type_t;
typedef struct m_option m_option_t;
struct m_config;
struct mp_log;
struct mpv_node;

///////////////////////////// Options types declarations ////////////////////

// Simple types
extern const m_option_type_t m_option_type_flag;
extern const m_option_type_t m_option_type_store;
extern const m_option_type_t m_option_type_float_store;
extern const m_option_type_t m_option_type_int;
extern const m_option_type_t m_option_type_int64;
extern const m_option_type_t m_option_type_intpair;
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
extern const m_option_type_t m_option_type_subconfig;
extern const m_option_type_t m_option_type_imgfmt;
extern const m_option_type_t m_option_type_fourcc;
extern const m_option_type_t m_option_type_afmt;
extern const m_option_type_t m_option_type_color;
extern const m_option_type_t m_option_type_geometry;
extern const m_option_type_t m_option_type_size_box;
extern const m_option_type_t m_option_type_chmap;
extern const m_option_type_t m_option_type_node;

// Used internally by m_config.c
extern const m_option_type_t m_option_type_alias;
extern const m_option_type_t m_option_type_removed;

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
};

void m_geometry_apply(int *xpos, int *ypos, int *widw, int *widh,
                      int scrw, int scrh, struct m_geometry *gm);

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
    // For free use by the implementer of m_obj_list.get_desc
    const void *p;
    // If not NULL, options which should be set before applying other options.
    // This member is usually set by m_obj_list_find() only, and read by the
    // option parser. It's not used anywhere else.
    const char *init_options;
    // Don't list entry with "help"
    bool hidden;
    // Callback to print custom help if "help" is passed
    void (*print_help)(struct mp_log *log);
    // Set by m_obj_list_find(). If the requested name is an old alias, this
    // is set to the old name (while the name field uses the new name).
    const char *replaced_name;
};

// Extra definition needed for \ref m_option_type_obj_settings_list options.
struct m_obj_list {
    bool (*get_desc)(struct m_obj_desc *dst, int index);
    const char *description;
    // Can be set to a NULL terminated array of aliases
    const char *aliases[4][5];
    // Allow a trailing ",", which adds an entry with name=""
    bool allow_trailer;
    // Allow unknown entries, for which a dummy entry is inserted, and whose
    // options are skipped and ignored.
    bool allow_unknown_entries;
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
    // NULL terminated array of parameter/value pairs.
    char **attribs;
} m_obj_settings_t;

// A parser to set up a list of objects.
/** It creates a NULL terminated array \ref m_obj_settings. The option priv
 *  field (\ref m_option::priv) must point to a \ref m_obj_list_t describing
 *  the available object types.
 */
extern const m_option_type_t m_option_type_obj_settings_list;

struct m_opt_choice_alternatives {
    char *name;
    int value;
};

const char *m_opt_choice_str(const struct m_opt_choice_alternatives *choices,
                             int value);

// For OPT_STRING_VALIDATE(). Behaves like m_option_type.parse().
typedef int (*m_opt_string_validate_fn)(struct mp_log *log, const m_option_t *opt,
                                        struct bstr name, struct bstr param);

// m_option.priv points to this if OPT_SUBSTRUCT is used
struct m_sub_options {
    const struct m_option *opts;
    size_t size;
    const void *defaults;
};

#define CONF_TYPE_FLAG          (&m_option_type_flag)
#define CONF_TYPE_STORE         (&m_option_type_store)
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
#define CONF_TYPE_INT_PAIR      (&m_option_type_intpair)
#define CONF_TYPE_NODE          (&m_option_type_node)

// Possible option values. Code is allowed to access option data without going
// through this union. It serves for self-documentation and to get minimal
// size/alignment requirements for option values in general.
union m_option_value {
    int flag; // not the C type "bool"!
    int store;
    float float_store;
    int int_;
    int64_t int64;
    int intpair[2];
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
    struct mp_chmap chmap;
};

////////////////////////////////////////////////////////////////////////////

// Option type description
struct m_option_type {
    const char *name;
    // Size needed for the data.
    unsigned int size;
    // One of M_OPT_TYPE*.
    unsigned int flags;

    // Parse the data from a string.
    /** It is the only required function, all others can be NULL.
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

    // Clamp the value in val to the option's valid value range.
    // Return values:
    //  M_OPT_OUT_OF_RANGE: val was invalid, and modified (clamped) to be valid
    //  M_OPT_INVALID:      val was invalid, and can't be made valid
    //  0:                  val was already valid and is unchanged
    int (*clamp)(const m_option_t *opt, void *val);

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
};

// Option description
struct m_option {
    // Option name.
    const char *name;

    // Option type.
    const m_option_type_t *type;

    // See \ref OptionFlags.
    unsigned int flags;

    int offset;

    // \brief Mostly useful for numeric types, the \ref M_OPT_MIN flags must
    // also be set.
    double min;

    // \brief Mostly useful for numeric types, the \ref M_OPT_MAX flags must
    // also be set.
    double max;

    // Type dependent data (for all kinds of extended settings).
    void *priv;

    // Initialize variable to given default before parsing options
    const void *defval;
};


// The option has a minimum set in \ref m_option::min.
#define M_OPT_MIN               (1 << 0)

// The option has a maximum set in \ref m_option::max.
#define M_OPT_MAX               (1 << 1)

// The option has a minimum and maximum in m_option::min and m_option::max.
#define M_OPT_RANGE             (M_OPT_MIN | M_OPT_MAX)

// The option is forbidden in config files.
#define M_OPT_NOCFG             (1 << 2)

// This option can't be set per-file when used with struct m_config.
#define M_OPT_GLOBAL            (1 << 4)

// Can not be freely changed at runtime (normally, all options can be changed,
// even if the settings don't get effective immediately). Note that an option
// might still change even if this is set, e.g. via properties or per-file
// options.
#define M_OPT_FIXED             (1 << 5)

// The option should be set during command line pre-parsing
#define M_OPT_PRE_PARSE         (1 << 6)

// The option expects a file name (or a list of file names)
#define M_OPT_FILE              (1 << 11)

// These are kept for compatibility with older code.
#define CONF_MIN                M_OPT_MIN
#define CONF_MAX                M_OPT_MAX
#define CONF_RANGE              M_OPT_RANGE
#define CONF_NOCFG              M_OPT_NOCFG
#define CONF_GLOBAL             (M_OPT_GLOBAL | M_OPT_FIXED)
#define CONF_PRE_PARSE          M_OPT_PRE_PARSE

// These flags are used to describe special parser capabilities or behavior.

// Suboption parser flag.
/** When this flag is set, m_option::p should point to another m_option
 *  array. Only the parse function will be called. If dst is set, it should
 *  create/update an array of char* containg opt/val pairs. The options in
 *  the child array will then be set automatically by the \ref Config.
 *  Also note that suboptions may be directly accessed by using
 *  -option:subopt blah.
 */
#define M_OPT_TYPE_HAS_CHILD            (1 << 0)

// Wildcard matching flag.
/** If set the option type has a use for option names ending with a *
 *  (used for -aa*), this only affects the option name matching.
 */
#define M_OPT_TYPE_ALLOW_WILDCARD       (1 << 1)

// Dynamic data type.
/** This flag indicates that the data is dynamically allocated (m_option::p
 *  points to a pointer). It enables a little hack in the \ref Config wich
 *  replaces the initial value of such variables with a dynamic copy in case
 *  the initial value is statically allocated (pretty common with strings).
 */
#define M_OPT_TYPE_DYNAMIC              (1 << 2)

// The parameter is optional and by default no parameter is preferred. If
// ambiguous syntax is used ("--opt value"), the command line parser will
// assume that the argument takes no parameter. In config files, these
// options can be used without "=" and value.
#define M_OPT_TYPE_OPTIONAL_PARAM       (1 << 3)

///////////////////////////// Parser flags /////////////////////////////////

// OptionParserReturn
//
// On success parsers return a number >= 0.
//
// To indicate that MPlayer should exit without playing anything,
// parsers return M_OPT_EXIT minus the number of parameters they
// consumed: \ref M_OPT_EXIT or \ref M_OPT_EXIT-1.
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

// Returned if the parser failed for any other reason than a bad parameter.
#define M_OPT_PARSER_ERR        -6

// Returned when MPlayer should exit. Used by various help stuff.
/** M_OPT_EXIT must be the lowest number on this list.
 */
#define M_OPT_EXIT              -7

char *m_option_strerror(int code);

// Find the option matching the given name in the list.
/** \ingroup Options
 *  This function takes the possible wildcards into account (see
 *  \ref M_OPT_TYPE_ALLOW_WILDCARD).
 *
 *  \param list Pointer to an array of \ref m_option.
 *  \param name Name of the option.
 *  \return The matching option or NULL.
 */
const m_option_t *m_option_list_find(const m_option_t *list, const char *name);

// Helper to parse options, see \ref m_option_type::parse.
static inline int m_option_parse(struct mp_log *log, const m_option_t *opt,
                                 struct bstr name, struct bstr param, void *dst)
{
    return opt->type->parse(log, opt, name, param, dst);
}

// Helper to print options, see \ref m_option_type::print.
static inline char *m_option_print(const m_option_t *opt, const void *val_ptr)
{
    if (opt->type->print)
        return opt->type->print(opt, val_ptr);
    else
        return NULL;
}

static inline char *m_option_pretty_print(const m_option_t *opt,
                                          const void *val_ptr)
{
    if (opt->type->pretty_print)
        return opt->type->pretty_print(opt, val_ptr);
    else
        return m_option_print(opt, val_ptr);
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

// see m_option_type.get
static inline int m_option_get_node(const m_option_t *opt, void *ta_parent,
                                    struct mpv_node *dst, void *src)
{
    if (opt->type->get)
        return opt->type->get(opt, ta_parent, dst, src);
    return M_OPT_UNKNOWN;
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


#define OPTION_LIST_SEPARATOR ','

#define OPTDEF_STR(s)     .defval = (void *)&(char * const){s}
#define OPTDEF_INT(i)     .defval = (void *)&(const int){i}
#define OPTDEF_FLOAT(f)   .defval = (void *)&(const float){f}
#define OPTDEF_DOUBLE(d)  .defval = (void *)&(const double){d}

#define OPT_GENERAL(ctype, optname, varname, flagv, ...)                \
    {.name = optname, .flags = flagv,                                   \
    .offset = MP_CHECKED_OFFSETOF(OPT_BASE_STRUCT, varname, ctype),     \
    __VA_ARGS__}

#define OPT_GENERAL_NOTYPE(optname, varname, flagv, ...)                \
    {.name = optname, .flags = flagv,                                   \
    .offset = offsetof(OPT_BASE_STRUCT, varname),                       \
    __VA_ARGS__}

#define OPT_HELPER_REMOVEPAREN(...) __VA_ARGS__

/* The OPT_SOMETHING->OPT_SOMETHING_ kind of redirection exists to
 * make the code fully standard-conforming: the C standard requires that
 * __VA_ARGS__ has at least one argument (though GCC for example would accept
 * 0). Thus the first OPT_SOMETHING is a wrapper which just adds one
 * argument to ensure __VA_ARGS__ is not empty when calling the next macro.
 */

#define OPT_FLAG(...) \
    OPT_GENERAL(int, __VA_ARGS__, .type = &m_option_type_flag)

#define OPT_FLAG_STORE(optname, varname, flags, value)          \
    OPT_GENERAL(int, optname, varname, flags, .max = value,     \
                .type = &m_option_type_store)

#define OPT_FLOAT_STORE(optname, varname, flags, value)         \
    OPT_GENERAL(float, optname, varname, flags, .max = value,   \
                .type = &m_option_type_float_store)

#define OPT_STRINGLIST(...) \
    OPT_GENERAL(char**, __VA_ARGS__, .type = &m_option_type_string_list)

#define OPT_STRING_APPEND_LIST(...) \
    OPT_GENERAL(char**, __VA_ARGS__, .type = &m_option_type_string_append_list)

#define OPT_KEYVALUELIST(...) \
    OPT_GENERAL(char**, __VA_ARGS__, .type = &m_option_type_keyvalue_list)

#define OPT_PATHLIST(...)                                                \
    OPT_GENERAL(char**, __VA_ARGS__, .type = &m_option_type_string_list, \
                .priv = (void *)&m_option_path_separator)

#define OPT_INT(...) \
    OPT_GENERAL(int, __VA_ARGS__, .type = &m_option_type_int)

#define OPT_INT64(...) \
    OPT_GENERAL(int64_t, __VA_ARGS__, .type = &m_option_type_int64)

#define OPT_RANGE_(ctype, optname, varname, flags, minval, maxval, ...) \
    OPT_GENERAL(ctype, optname, varname, (flags) | CONF_RANGE,          \
                .min = minval, .max = maxval, __VA_ARGS__)

#define OPT_INTRANGE(...) \
    OPT_RANGE_(int, __VA_ARGS__, .type = &m_option_type_int)

#define OPT_FLOATRANGE(...) \
    OPT_RANGE_(float, __VA_ARGS__, .type = &m_option_type_float)

#define OPT_INTPAIR(...) \
    OPT_GENERAL_NOTYPE(__VA_ARGS__, .type = &m_option_type_intpair)

#define OPT_FLOAT(...) \
    OPT_GENERAL(float, __VA_ARGS__, .type = &m_option_type_float)

#define OPT_DOUBLE(...) \
    OPT_GENERAL(double, __VA_ARGS__, .type = &m_option_type_double)

#define OPT_STRING(...) \
    OPT_GENERAL(char*, __VA_ARGS__, .type = &m_option_type_string)

#define OPT_SETTINGSLIST(optname, varname, flags, objlist)      \
    OPT_GENERAL(m_obj_settings_t*, optname, varname, flags,     \
                .type = &m_option_type_obj_settings_list,       \
                .priv = (void*)MP_EXPECT_TYPE(const struct m_obj_list*, objlist))

#define OPT_IMAGEFORMAT(...) \
    OPT_GENERAL(int, __VA_ARGS__, .type = &m_option_type_imgfmt)

#define OPT_AUDIOFORMAT(...) \
    OPT_GENERAL(int, __VA_ARGS__, .type = &m_option_type_afmt)

#define OPT_CHMAP(...) \
    OPT_GENERAL(struct mp_chmap, __VA_ARGS__, .type = &m_option_type_chmap)


#define M_CHOICES(choices)                                              \
    .priv = (void *)&(const struct m_opt_choice_alternatives[]){        \
                      OPT_HELPER_REMOVEPAREN choices, {NULL}}

#define OPT_CHOICE(...) \
    OPT_CHOICE_(__VA_ARGS__, .type = &m_option_type_choice)
#define OPT_CHOICE_(optname, varname, flags, choices, ...) \
    OPT_GENERAL(int, optname, varname, flags, M_CHOICES(choices), __VA_ARGS__)
// Variant which takes a pointer to struct m_opt_choice_alternatives directly
#define OPT_CHOICE_C(optname, varname, flags, choices) \
    OPT_GENERAL(int, optname, varname, flags, .priv = (void *) \
                MP_EXPECT_TYPE(const struct m_opt_choice_alternatives*, choices), \
                .type = &m_option_type_choice)

#define OPT_FLAGS(...) \
    OPT_CHOICE_(__VA_ARGS__, .type = &m_option_type_flags)

// Union of choices and an int range. The choice values can be included in the
// int range, or be completely separate - both works.
#define OPT_CHOICE_OR_INT_(optname, varname, flags, minval, maxval, choices, ...) \
    OPT_GENERAL(int, optname, varname, (flags) | CONF_RANGE,                      \
                .min = minval, .max = maxval,                                     \
                M_CHOICES(choices), __VA_ARGS__)
#define OPT_CHOICE_OR_INT(...) \
    OPT_CHOICE_OR_INT_(__VA_ARGS__, .type = &m_option_type_choice)

#define OPT_TIME(...) \
    OPT_GENERAL(double, __VA_ARGS__, .type = &m_option_type_time)

#define OPT_REL_TIME(...) \
    OPT_GENERAL(struct m_rel_time, __VA_ARGS__, .type = &m_option_type_rel_time)

#define OPT_COLOR(...) \
    OPT_GENERAL(struct m_color, __VA_ARGS__, .type = &m_option_type_color)

#define OPT_GEOMETRY(...) \
    OPT_GENERAL(struct m_geometry, __VA_ARGS__, .type = &m_option_type_geometry)

#define OPT_SIZE_BOX(...) \
    OPT_GENERAL(struct m_geometry, __VA_ARGS__, .type = &m_option_type_size_box)

#define OPT_TRACKCHOICE(name, var) \
    OPT_CHOICE_OR_INT(name, var, 0, 0, 8190, ({"no", -2}, {"auto", -1}))

#define OPT_STRING_VALIDATE_(optname, varname, flags, validate_fn, ...)        \
    OPT_GENERAL(char*, optname, varname, flags, __VA_ARGS__,                   \
                .priv = MP_EXPECT_TYPE(m_opt_string_validate_fn, validate_fn))
#define OPT_STRING_VALIDATE(...) \
    OPT_STRING_VALIDATE_(__VA_ARGS__, .type = &m_option_type_string)

#define OPT_PRINT(optname, fn)                                              \
    {.name = optname,                                                       \
     .flags = M_OPT_FIXED | M_OPT_GLOBAL | M_OPT_NOCFG | M_OPT_PRE_PARSE,   \
     .type = &m_option_type_print_fn,                                       \
     .priv = MP_EXPECT_TYPE(m_opt_print_fn, fn),                            \
     .offset = -1}

// subconf must have the type struct m_sub_options.
// All sub-options are prefixed with "name-" and are added to the current
// (containing) option list.
// If name is "", add the sub-options directly instead.
// varname refers to the field, that must be a pointer to a field described by
// the subconf struct.
#define OPT_SUBSTRUCT(name, varname, subconf, flagv)            \
    OPT_GENERAL_NOTYPE(name, varname, flagv,                    \
                       .type = &m_option_type_subconfig,        \
                       .priv = (void*)&subconf)

// If "--name" was removed, but "--newname" has the same semantics.
// It will be redirected, and a warning will be printed on first use.
#define OPT_REPLACED(optname, newname) \
    {.name = optname, .type = &m_option_type_alias, .priv = newname, .offset = -1}

// "--name" doesn't exist, but inform the user about a replacement with msg.
#define OPT_REMOVED(optname, msg) \
    {.name = optname, .type = &m_option_type_removed, .priv = msg, .offset = -1}

#endif /* MPLAYER_M_OPTION_H */
