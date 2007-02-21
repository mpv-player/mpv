
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
typedef int(*m_property_ctrl_f)(m_option_t* prop,int action,void* arg,void *ctx);

/// Do an action on a property.
/** \param prop The property.
 *  \param action See \ref PropertyActions.
 *  \param arg Argument, usually a pointer to the data type used by the property.
 *  \return See \ref PropertyActionsReturn.
 */
int m_property_do(m_option_t* prop, int action, void* arg, void *ctx);

/// Print the current value of a property.
/** \param prop The property.
 *  \return A newly allocated string with the current value or NULL on error.
 */
char* m_property_print(m_option_t* prop, void *ctx);

/// Set a property.
/** \param prop The property.
 *  \param txt The value to set.
 *  \return 1 on success, 0 on error.
 */
int m_property_parse(m_option_t* prop, char* txt, void *ctx);

/// Print a list of properties.
void m_properties_print_help_list(m_option_t* list);

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
char* m_properties_expand_string(m_option_t* prop_list,char* str, void *ctx);

// Helpers to use MPlayer's properties

/// Get an MPlayer property.
m_option_t*  mp_property_find(const char* name);

/// Do an action with an MPlayer property.
int mp_property_do(const char* name,int action, void* val, void *ctx);

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
int m_property_int_ro(m_option_t* prop,int action,
                      void* arg,int var);

/// Implement set, get and step up/down.
int m_property_int_range(m_option_t* prop,int action,
                         void* arg,int* var);

/// Same as m_property_int_range but cycle.
int m_property_choice(m_option_t* prop,int action,
                      void* arg,int* var);

/// Switch betwen min and max.
int m_property_flag(m_option_t* prop,int action,
                    void* arg,int* var);

/// Implement get, print.
int m_property_float_ro(m_option_t* prop,int action,
                        void* arg,float var);

/// Implement set, get and step up/down
int m_property_float_range(m_option_t* prop,int action,
                           void* arg,float* var);

/// float with a print function which print the time in ms
int m_property_delay(m_option_t* prop,int action,
                     void* arg,float* var);

/// Implement get, print
int m_property_double_ro(m_option_t* prop,int action,
                         void* arg,double var);

/// get/print the string
int m_property_string_ro(m_option_t* prop,int action,void* arg, char* str);

///@}

///@}
