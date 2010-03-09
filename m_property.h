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

#include "m_option.h"

/// \defgroup Properties
///
/// Properties provide an interface to query and set the state of various
/// things in MPlayer. The API is based on the \ref Options API like the
/// \ref Config, but instead of using variables, properties use an ioctl like
/// function. The function is used to perform various actions like get and set
/// (see \ref PropertyActions).
///@{

/// \file

/// \defgroup PropertyActions Property actions
/// \ingroup Properties
///@{

/// Get the current value.
/** \param arg Pointer to a variable of the right type.
 */
#define M_PROPERTY_GET         0

/// Get a string representing the current value.
/** Set the variable to a newly allocated string or NULL.
 *  \param arg Pointer to a char* variable.
 */
#define M_PROPERTY_PRINT       1

/// Set a new value.
/** The variable is updated to the value actually set.
 *  \param arg Pointer to a variable of the right type.
 */
#define M_PROPERTY_SET         2

/// Set a new value from a string.
/** \param arg String containing the value.
 */
#define M_PROPERTY_PARSE       3

/// Increment the current value.
/** The sign of the argument is also taken into account if applicable.
 *  \param arg Pointer to a variable of the right type or NULL.
 */
#define M_PROPERTY_STEP_UP     4

/// Decrement the current value.
/** The sign of the argument is also taken into account if applicable.
 *  \param arg Pointer to a variable of the right type or NULL.
 */
#define M_PROPERTY_STEP_DOWN   5

/// Get a string containg a parsable representation.
/** Set the variable to a newly allocated string or NULL.
 *  \param arg Pointer to a char* variable.
 */
#define M_PROPERTY_TO_STRING   6

/// Pass down an action to a sub-property.
#define M_PROPERTY_KEY_ACTION  7

/// Get a m_option describing the property.
#define M_PROPERTY_GET_TYPE    8

///@}

/// \defgroup PropertyActionsArg Property actions argument type
/// \ingroup Properties
/// \brief  Types used as action argument.
///@{

/// Argument for \ref M_PROPERTY_KEY_ACTION
typedef struct {
    const char* key;
    int action;
    void* arg;
} m_property_action_t;

///@}

/// \defgroup PropertyActionsReturn Property actions return code
/// \ingroup Properties
/// \brief  Return values for the control function.
///@{

/// Returned on success.
#define M_PROPERTY_OK                1

/// Returned on error.
#define M_PROPERTY_ERROR             0

/// \brief Returned when the property can't be used, for example something about
/// the subs while playing audio only
#define M_PROPERTY_UNAVAILABLE      -1

/// Returned if the requested action is not implemented.
#define M_PROPERTY_NOT_IMPLEMENTED  -2

/// Returned when asking for a property that doesn't exist.
#define M_PROPERTY_UNKNOWN          -3

/// Returned when the action can't be done (like setting the volume when edl mute).
#define M_PROPERTY_DISABLED         -4

///@}

/// \ingroup Properties
/// \brief Property action callback.
typedef int(*m_property_ctrl_f)(const m_option_t* prop,int action,void* arg,void *ctx);

/// Do an action on a property.
/** \param prop_list The list of properties.
 *  \param prop The path of the property.
 *  \param action See \ref PropertyActions.
 *  \param arg Argument, usually a pointer to the data type used by the property.
 *  \return See \ref PropertyActionsReturn.
 */
int m_property_do(const m_option_t* prop_list, const char* prop,
                  int action, void* arg, void *ctx);

/// Print a list of properties.
void m_properties_print_help_list(const m_option_t* list);

/// Expand a property string.
/** This function allows to print strings containing property values.
 *  ${NAME} is expanded to the value of property NAME or an empty
 *  string in case of error. $(NAME:STR) expand STR only if the property
 *  NAME is available.
 *
 *  \param prop_list An array of \ref m_option describing the available
 *                   properties.
 *  \param str The string to expand.
 *  \return The newly allocated expanded string.
 */
char* m_properties_expand_string(const m_option_t* prop_list,char* str, void *ctx);

// Helpers to use MPlayer's properties

/// Do an action with an MPlayer property.
int mp_property_do(const char* name,int action, void* val, void *ctx);

/// Get the value of a property as a string suitable for display in an UI.
char* mp_property_print(const char *name, void* ctx);

/// \defgroup PropertyImplHelper Property implementation helpers
/// \ingroup Properties
/// \brief Helper functions for common property types.
///@{

/// Clamp a value according to \ref m_option::min and \ref m_option::max.
#define M_PROPERTY_CLAMP(prop,val) do {                                 \
        if(((prop)->flags & M_OPT_MIN) && (val) < (prop)->min)          \
            (val) = (prop)->min;                                        \
        else if(((prop)->flags & M_OPT_MAX) && (val) > (prop)->max)     \
            (val) = (prop)->max;                                        \
    } while(0)

/// Implement get.
int m_property_int_ro(const m_option_t* prop,int action,
                      void* arg,int var);

/// Implement set, get and step up/down.
int m_property_int_range(const m_option_t* prop,int action,
                         void* arg,int* var);

/// Same as m_property_int_range but cycle.
int m_property_choice(const m_option_t* prop,int action,
                      void* arg,int* var);

int m_property_flag_ro(const m_option_t* prop,int action,
                    void* arg,int var);

/// Switch betwen min and max.
int m_property_flag(const m_option_t* prop,int action,
                    void* arg,int* var);

/// Implement get, print.
int m_property_float_ro(const m_option_t* prop,int action,
                        void* arg,float var);

/// Implement set, get and step up/down
int m_property_float_range(const m_option_t* prop,int action,
                           void* arg,float* var);

/// float with a print function which print the time in ms
int m_property_delay(const m_option_t* prop,int action,
                     void* arg,float* var);

/// Implement get, print
int m_property_double_ro(const m_option_t* prop,int action,
                         void* arg,double var);

/// Implement print
int m_property_time_ro(const m_option_t* prop,int action,
                       void* arg,double var);

/// get/print the string
int m_property_string_ro(const m_option_t* prop,int action,void* arg, char* str);

/// get/print a bitrate
int m_property_bitrate(const m_option_t* prop,int action,void* arg,int rate);

///@}

///@}

#endif /* MPLAYER_M_PROPERTY_H */
