/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_M_PROPERTY_H
#define MPLAYER_M_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "m_option.h"

struct mp_log;

extern const struct m_option_type m_option_type_dummy;

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

    // Pass down an action to a sub-property.
    //  arg: struct m_property_action_arg*
    M_PROPERTY_KEY_ACTION,
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
};

// Access a property.
// action: one of m_property_action
// ctx: opaque value passed through to property implementation
// returns: one of mp_property_return
int m_property_do(struct mp_log *log, const struct m_option* prop_list,
                  const char* property_name, int action, void* arg, void *ctx);

// Print a list of properties.
void m_properties_print_help_list(struct mp_log *log,
                                  const struct m_option* list);

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
char* m_properties_expand_string(const struct m_option* prop_list,
                                 const char *str, void *ctx);

// Trivial helpers for implementing properties.
int m_property_int_ro(const struct m_option* prop, int action, void* arg,
                      int var);
int m_property_int64_ro(const struct m_option* prop, int action, void* arg,
                        int64_t var);
int m_property_float_ro(const struct m_option* prop, int action, void* arg,
                        float var);
int m_property_double_ro(const struct m_option* prop, int action, void* arg,
                         double var);
int m_property_strdup_ro(const struct m_option* prop, int action, void* arg,
                         const char *var);

struct m_sub_property {
    // Name of the sub-property - this will be prefixed with the parent
    // property's name.
    const char *name;
    // Type of the data stored in the value member. See m_option.
    const struct m_option_type *type;
    // Data returned by the sub-property. m_property_read_sub() will make a
    // copy of this if needed. It will never write or free the data.
    union m_option_value value;
    // This can be set to true if the property should be hidden.
    bool unavailable;
};

// Convenience macros which can be used as part of a sub_property entry.
#define SUB_PROP_INT(i) \
    .type = CONF_TYPE_INT, .value = {.int_ = (i)}
#define SUB_PROP_STR(s) \
    .type = CONF_TYPE_STRING, .value = {.string = (char *)(s)}
#define SUB_PROP_FLOAT(f) \
    .type = CONF_TYPE_FLOAT, .value = {.float_ = (f)}
#define SUB_PROP_FLAG(f) \
    .type = CONF_TYPE_FLAG, .value = {.flag = (f)}

int m_property_read_sub(const struct m_sub_property *props, int action, void *arg);

#endif /* MPLAYER_M_PROPERTY_H */
