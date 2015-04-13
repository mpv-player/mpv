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

#ifndef MPLAYER_M_PROPERTY_H
#define MPLAYER_M_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "m_option.h"

struct mp_log;

enum mp_property_action {
    // Get the property type. This defines the fundamental data type read from
    // or written to the property.
    // If unimplemented, the m_option entry that defines the property is used.
    //  arg: m_option*
    M_PROPERTY_GET_TYPE,

    // Get the current value.
    //  arg: pointer to a variable of the type according to the property type
    M_PROPERTY_GET,

    // Set a new value. The property wrapper will make sure that only valid
    // values are set (e.g. according to the property type's min/max range).
    // If unimplemented, the property is read-only.
    //  arg: pointer to a variable of the type according to the property type
    M_PROPERTY_SET,

    // Get human readable string representing the current value.
    // If unimplemented, the property wrapper uses the property type as
    // fallback.
    //  arg: char**
    M_PROPERTY_PRINT,

    // Switch the property up/down by a given value.
    // If unimplemented, the property wrapper uses the property type as
    // fallback.
    //  arg: struct m_property_switch_arg*
    M_PROPERTY_SWITCH,

    // Get a string containing a parsable representation.
    // Can't be overridden by property implementations.
    //  arg: char**
    M_PROPERTY_GET_STRING,

    // Set a new value from a string. The property wrapper parses this using the
    // parse function provided by the property type.
    // Can't be overridden by property implementations.
    //  arg: char*
    M_PROPERTY_SET_STRING,

    // Set a mpv_node value.
    //  arg: mpv_node*
    M_PROPERTY_GET_NODE,

    // Get a mpv_node value.
    //  arg: mpv_node*
    M_PROPERTY_SET_NODE,

    // Pass down an action to a sub-property.
    //  arg: struct m_property_action_arg*
    M_PROPERTY_KEY_ACTION,

    // Get the (usually constant) value that indicates no change. Obscure
    // special functionality for things like the volume property.
    // Otherwise works like M_PROPERTY_GET.
    M_PROPERTY_GET_NEUTRAL,
};

// Argument for M_PROPERTY_SWITCH
struct m_property_switch_arg {
    double inc;         // value to add to property, or cycle direction
    bool wrap;          // whether value should wrap around on over/underflow
};

// Argument for M_PROPERTY_KEY_ACTION
struct m_property_action_arg {
    const char* key;
    int action;
    void* arg;
};

enum mp_property_return {
    // Returned on success.
    M_PROPERTY_OK = 1,

    // Returned on error.
    M_PROPERTY_ERROR = 0,

    // Returned when the property can't be used, for example video related
    // properties while playing audio only.
    M_PROPERTY_UNAVAILABLE = -1,

    // Returned if the requested action is not implemented.
    M_PROPERTY_NOT_IMPLEMENTED = -2,

    // Returned when asking for a property that doesn't exist.
    M_PROPERTY_UNKNOWN = -3,

    // When trying to set invalid or incorrectly formatted data.
    M_PROPERTY_INVALID_FORMAT = -4,
};

struct m_property {
    const char *name;
    // ctx: opaque caller context, which the property might use
    // prop: pointer to this struct
    // action: one of enum mp_property_action
    // arg: specific to the action
    // returns: one of enum mp_property_return
    int (*call)(void *ctx, struct m_property *prop, int action, void *arg);
    void *priv;
};

// Access a property.
// action: one of m_property_action
// ctx: opaque value passed through to property implementation
// returns: one of mp_property_return
int m_property_do(struct mp_log *log, const struct m_property* prop_list,
                  const char* property_name, int action, void* arg, void *ctx);

// Given a path of the form "a/b/c", this function will set *prefix to "a",
// and rem to "b/c", and return true.
// If there is no '/' in the path, set prefix to path, and rem to "", and
// return false.
bool m_property_split_path(const char *path, bstr *prefix, char **rem);

// Print a list of properties.
void m_properties_print_help_list(struct mp_log *log,
                                  const struct m_property *list);

// Expand a property string.
// This function allows to print strings containing property values.
//  ${NAME} is expanded to the value of property NAME.
//  If NAME starts with '=', use the raw value of the property.
//  ${NAME:STR} expands to the property, or STR if the property is not
//  available.
//  ${?NAME:STR} expands to STR if the property is available.
//  ${!NAME:STR} expands to STR if the property is not available.
// General syntax: "${" ["?" | "!"] ["="] NAME ":" STR "}"
// STR is recursively expanded using the same rules.
// "$$" can be used to escape "$", and "$}" to escape "}".
// "$>" disables parsing of "$" for the rest of the string.
char* m_properties_expand_string(const struct m_property *prop_list,
                                 const char *str, void *ctx);

// Trivial helpers for implementing properties.
int m_property_flag_ro(int action, void* arg, int var);
int m_property_int_ro(int action, void* arg, int var);
int m_property_int64_ro(int action, void* arg, int64_t var);
int m_property_float_ro(int action, void* arg, float var);
int m_property_double_ro(int action, void* arg, double var);
int m_property_strdup_ro(int action, void* arg, const char *var);

struct m_sub_property {
    // Name of the sub-property - this will be prefixed with the parent
    // property's name.
    const char *name;
    // Type of the data stored in the value member. See m_option.
    struct m_option type;
    // Data returned by the sub-property. m_property_read_sub() will make a
    // copy of this if needed. It will never write or free the data.
    union m_option_value value;
    // This can be set to true if the property should be hidden.
    bool unavailable;
};

// Convenience macros which can be used as part of a sub_property entry.
#define SUB_PROP_INT(i) \
    .type = {.type = CONF_TYPE_INT}, .value = {.int_ = (i)}
#define SUB_PROP_STR(s) \
    .type = {.type = CONF_TYPE_STRING}, .value = {.string = (char *)(s)}
#define SUB_PROP_FLOAT(f) \
    .type = {.type = CONF_TYPE_FLOAT}, .value = {.float_ = (f)}
#define SUB_PROP_DOUBLE(f) \
    .type = {.type = CONF_TYPE_DOUBLE}, .value = {.double_ = (f)}
#define SUB_PROP_FLAG(f) \
    .type = {.type = CONF_TYPE_FLAG}, .value = {.flag = (f)}

int m_property_read_sub(const struct m_sub_property *props, int action, void *arg);


// Used with m_property_read_list().
// Get an entry. item is the 0-based index of the item. This behaves like a
// top-level property request (but you must implement M_PROPERTY_GET_TYPE).
// item will be in range [0, count), for count see m_property_read_list()
// action, arg are for property access.
// ctx is userdata passed to m_property_read_list.
typedef int (*m_get_item_cb)(int item, int action, void *arg, void *ctx);

int m_property_read_list(int action, void *arg, int count,
                         m_get_item_cb get_item, void *ctx);

#endif /* MPLAYER_M_PROPERTY_H */
