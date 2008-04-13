#ifndef MPLAYER_M_OPTION_H
#define MPLAYER_M_OPTION_H

#include <string.h>

/// \defgroup Options
/// m_option allows to parse, print and copy data of various types.
/// It is the base of the \ref OptionsStruct, \ref Config and
/// \ref Properties APIs.
///@{

/// \file m_option.h

/// \ingroup OptionTypes
typedef struct m_option_type m_option_type_t;
typedef struct m_option m_option_t;
struct m_struct_st;

/// \defgroup OptionTypes Options types
/// \ingroup Options
///@{

///////////////////////////// Options types declarations ////////////////////////////

// Simple types
extern const m_option_type_t m_option_type_flag;
extern const m_option_type_t m_option_type_int;
extern const m_option_type_t m_option_type_float;
extern const m_option_type_t m_option_type_double;
extern const m_option_type_t m_option_type_string;
extern const m_option_type_t m_option_type_string_list;
extern const m_option_type_t m_option_type_position;
extern const m_option_type_t m_option_type_time;
extern const m_option_type_t m_option_type_time_size;

extern const m_option_type_t m_option_type_print;
extern const m_option_type_t m_option_type_print_indirect;
extern const m_option_type_t m_option_type_print_func;
extern const m_option_type_t m_option_type_subconfig;
extern const m_option_type_t m_option_type_imgfmt;
extern const m_option_type_t m_option_type_afmt;

// Func-based types
extern const m_option_type_t m_option_type_func_full;
extern const m_option_type_t m_option_type_func_param;
extern const m_option_type_t m_option_type_func;

/// Callback used to reset func options.
typedef void (*m_opt_default_func_t)(const m_option_t *, const char*);

/// Callback used by m_option_type_func_full options.
typedef int (*m_opt_func_full_t)(const m_option_t *, const char *, char *);

/// Callback used by m_option_type_func_param options.
typedef int (*m_opt_func_param_t)(const m_option_t *, const char *);

/// Callback used by m_option_type_func options.
typedef int (*m_opt_func_t)(const m_option_t *);

// Backwards compatibility
typedef m_opt_default_func_t cfg_default_func_t;
typedef m_opt_func_full_t cfg_func_arg_param_t;
typedef m_opt_func_param_t cfg_func_param_t;
typedef m_opt_func_t cfg_func_t;

#define END_AT_NONE   0
#define END_AT_TIME   1
#define END_AT_SIZE   2
typedef struct {
  double pos;
  int type;
} m_time_size_t;

/// Extra definition needed for \ref m_option_type_obj_settings_list options.
typedef struct {
  /// Pointer to an array of pointer to some object type description struct.
  void** list;
  /// Offset of the object type name (char*) in the description struct.
  void* name_off;
  /// Offset of the object type info string (char*) in the description struct.
  void* info_off;
  /// \brief Offset of the object type parameter description (\ref m_struct_st)
  /// in the description struct.
  void* desc_off;
} m_obj_list_t;

/// The data type used by \ref m_option_type_obj_settings_list.
typedef struct m_obj_settings {
  /// Type of the object.
  char* name;
  /// NULL terminated array of parameter/value pairs.
  char** attribs;
} m_obj_settings_t;

/// A parser to set up a list of objects.
/** It creates a NULL terminated array \ref m_obj_settings. The option priv
 *  field (\ref m_option::priv) must point to a \ref m_obj_list_t describing
 *  the available object types.
 */
extern const m_option_type_t m_option_type_obj_settings_list;

/// Extra definition needed for \ref m_option_type_obj_presets options.
typedef struct {
  /// Description of the struct holding the presets.
  struct m_struct_st* in_desc;
  /// Description of the struct that should be set by the presets.
  struct m_struct_st* out_desc;
  /// Pointer to an array of structs defining the various presets.
  void* presets;
  /// Offset of the preset's name inside the in_struct.
  void* name_off;
} m_obj_presets_t;

/// Set several fields in a struct at once.
/** For this two struct descriptions are used. One for the struct holding the
 *  preset and one for the struct beeing set. Every field present in both
 *  structs will be copied from the preset struct to the destination one.
 *  The option priv field (\ref m_option::priv) must point to a correctly
 *  filled \ref m_obj_presets_t.
 */
extern const m_option_type_t m_option_type_obj_presets;

/// Parse an URL into a struct.
/** The option priv field (\ref m_option::priv) must point to a
 *  \ref m_struct_st describing which fields of the URL must be used.
 */
extern const m_option_type_t m_option_type_custom_url;

/// Extra definition needed for \ref m_option_type_obj_params options.
typedef struct {
  /// Field descriptions.
  const struct m_struct_st* desc;
  /// Field separator to use.
  char separator;
} m_obj_params_t;

/// Parse a set of parameters.
/** Parameters are separated by the given separator and each one
 *  successively sets a field from the struct. The option priv field
 *  (\ref m_option::priv) must point to a \ref m_obj_params_t.
 */
extern const m_option_type_t m_option_type_obj_params;

typedef struct {
  int start;
  int end;
} m_span_t;
/// Ready made settings to parse a \ref m_span_t with a start-end syntax.
extern const m_obj_params_t m_span_params_def;


// FIXME: backward compatibility
#define CONF_TYPE_FLAG		(&m_option_type_flag)
#define CONF_TYPE_INT		(&m_option_type_int)
#define CONF_TYPE_FLOAT		(&m_option_type_float)
#define CONF_TYPE_DOUBLE	(&m_option_type_double)
#define CONF_TYPE_STRING	(&m_option_type_string)
#define CONF_TYPE_FUNC		(&m_option_type_func)
#define CONF_TYPE_FUNC_PARAM	(&m_option_type_func_param)
#define CONF_TYPE_PRINT		(&m_option_type_print)
#define CONF_TYPE_PRINT_INDIRECT (&m_option_type_print_indirect)
#define CONF_TYPE_PRINT_FUNC	(&m_option_type_print_func)
#define CONF_TYPE_FUNC_FULL	(&m_option_type_func_full)
#define CONF_TYPE_SUBCONFIG	(&m_option_type_subconfig)
#define CONF_TYPE_STRING_LIST	(&m_option_type_string_list)
#define CONF_TYPE_POSITION	(&m_option_type_position)
#define CONF_TYPE_IMGFMT	(&m_option_type_imgfmt)
#define CONF_TYPE_AFMT		(&m_option_type_afmt)
#define CONF_TYPE_SPAN		(&m_option_type_span)
#define CONF_TYPE_OBJ_SETTINGS_LIST (&m_option_type_obj_settings_list)
#define CONF_TYPE_OBJ_PRESETS	(&m_option_type_obj_presets)
#define CONF_TYPE_CUSTOM_URL	(&m_option_type_custom_url)
#define CONF_TYPE_OBJ_PARAMS	(&m_option_type_obj_params)
#define CONF_TYPE_TIME		(&m_option_type_time)
#define CONF_TYPE_TIME_SIZE	(&m_option_type_time_size)

/////////////////////////////////////////////////////////////////////////////////////////////

/// Option type description
struct m_option_type {
  char* name;
  /// Syntax description, etc
  char* comments;
  /// Size needed for the data.
  unsigned int size;
  /// See \ref OptionTypeFlags.
  unsigned int flags;
  
  /// Parse the data from a string.
  /** It is the only required function, all others can be NULL.
   * 
   *  \param opt The option that is parsed.
   *  \param name The full option name.
   *  \param param The parameter to parse.
   *  \param dst Pointer to the memory where the data should be written.
   *             If NULL the parameter validity should still be checked.
   *  \param src Source of the option, see \ref OptionParserModes.
   *  \return On error a negative value is returned, on success the number of arguments
   *          consumed. For details see \ref OptionParserReturn.
   */
  int (*parse)(const m_option_t* opt,const char *name, char *param, void* dst, int src);
  
  /// Print back a value in string form.
  /** \param opt The option to print.
   *  \param val Pointer to the memory holding the data to be printed.
   *  \return An allocated string containing the text value or (void*)-1
   *          on error.
   */
  char* (*print)(const m_option_t* opt, const void* val);

  /** \name
   *  These functions are called to save/set/restore the status of the
   *  variables. The difference between the 3 only matters for types like
   *  \ref m_option_type_func where 'setting' needs to do more than just
   *  copying some data.
   */
  //@{

  /// Update a save slot (dst) from the current value in the program (src).
  /** \param opt The option to copy.
   *  \param dst Pointer to the destination memory.
   *  \param src Pointer to the source memory.
   */
  void (*save)(const m_option_t* opt,void* dst, void* src);
  
  /// Set the value in the program (dst) from a save slot.
  /** \param opt The option to copy.
   *  \param dst Pointer to the destination memory.
   *  \param src Pointer to the source memory.
   */
  void (*set)(const m_option_t* opt,void* dst, void* src);

  /// Copy the data between two save slots. If NULL and size is > 0 a memcpy will be used.
  /** \param opt The option to copy.
   *  \param dst Pointer to the destination memory.
   *  \param src Pointer to the source memory.
   */
  void (*copy)(const m_option_t* opt,void* dst, void* src);
  //@}

  /// Free the data allocated for a save slot.
  /** This is only needed for dynamic types like strings.
   *  \param dst Pointer to the data, usually a pointer that should be freed and
   *             set to NULL.
   */
  void (*free)(void* dst);
};

///@}

/// Option description
/** \ingroup Options
 */
struct m_option {
  /// Option name.
  const char *name;
  
  /// Reserved for higher level APIs, it shouldn't be used by parsers.
  /** The suboption parser and func types do use it. They should instead
   *  use the priv field but this was inherited from older versions of the
   *  config code.
   */
  void *p;
  
  /// Option type.
  const m_option_type_t* type;
  
  /// See \ref OptionFlags.
  unsigned int flags;
  
  /// \brief Mostly useful for numeric types, the \ref M_OPT_MIN flags must
  /// also be set.
  double min;

  /// \brief Mostly useful for numeric types, the \ref M_OPT_MAX flags must
  /// also be set.
  double max;
  
  /// Type dependent data (for all kinds of extended settings).
  /** This used to be a function pointer to hold a 'reverse to defaults' func.
   *  Now it can be used to pass any type of extra args needed by the parser.
   *  Passing a 'default func' is still valid for all func based option types.
   */
  void* priv;
};


/// \defgroup OptionFlags Option flags
///@{

/// The option has a minimum set in \ref m_option::min.
#define M_OPT_MIN		(1<<0)

/// The option has a maximum set in \ref m_option::max.
#define M_OPT_MAX		(1<<1)

/// The option has a minimum and maximum in \ref m_option::min and \ref m_option::max.
#define M_OPT_RANGE		(M_OPT_MIN|M_OPT_MAX)

/// The option is forbidden in config files.
#define M_OPT_NOCFG		(1<<2)

/// The option is forbidden on the command line.
#define M_OPT_NOCMD		(1<<3)

/// The option is global in the \ref Config.
/** It won't be saved on push and the command line parser will set it when
 *  it's parsed (i.e. it won't be set later)
 *  e.g options : -v, -quiet
 */
#define M_OPT_GLOBAL		(1<<4)

/// The \ref Config won't save this option on push.
/** It won't be saved on push but the command line parser will add it with
 *  its entry (i.e. it may be set later)
 *  e.g options : -include
 */
#define M_OPT_NOSAVE		(1<<5)

/// \brief The \ref Config will emulate the old behavior by pushing the
/// option only if it was set by the user.
#define M_OPT_OLD		(1<<6)

/// The option should be set during command line pre-parsing
#define M_OPT_PRE_PARSE		(1<<7)

/// \defgroup OldOptionFlags Backward compatibility
///
/// These are kept for compatibility with older code.
/// @{
#define CONF_MIN		M_OPT_MIN
#define CONF_MAX		M_OPT_MAX
#define CONF_RANGE		M_OPT_RANGE
#define CONF_NOCFG		M_OPT_NOCFG
#define CONF_NOCMD		M_OPT_NOCMD
#define CONF_GLOBAL		M_OPT_GLOBAL
#define CONF_NOSAVE		M_OPT_NOSAVE
#define CONF_OLD		M_OPT_OLD
#define CONF_PRE_PARSE		M_OPT_PRE_PARSE
///@}

///@}

/// \defgroup OptionTypeFlags Option type flags
/// \ingroup OptionTypes
///
/// These flags are used to describe special parser capabilities or behavior.
///
///@{

/// Suboption parser flag.
/** When this flag is set, m_option::p should point to another m_option
 *  array. Only the parse function will be called. If dst is set, it should
 *  create/update an array of char* containg opt/val pairs. The options in
 *  the child array will then be set automatically by the \ref Config.
 *  Also note that suboptions may be directly accessed by using
 *  -option:subopt blah.
 */
#define M_OPT_TYPE_HAS_CHILD		(1<<0)

/// Wildcard matching flag.
/** If set the option type has a use for option names ending with a *
 *  (used for -aa*), this only affects the option name matching.
 */
#define M_OPT_TYPE_ALLOW_WILDCARD	(1<<1)

/// Dynamic data type.
/** This flag indicates that the data is dynamically allocated (m_option::p
 *  points to a pointer). It enables a little hack in the \ref Config wich
 *  replaces the initial value of such variables with a dynamic copy in case
 *  the initial value is statically allocated (pretty common with strings).
 */
#define M_OPT_TYPE_DYNAMIC		(1<<2)

/// Indirect option type.
/** If this is set the parse function doesn't directly return
 *  the wanted thing. Options use this if for some reasons they have to wait
 *  until the set call to be able to correctly set the target var.
 *  So for those types new values must first be parsed, then set to the target
 *  var. If this flag isn't set then new values can be parsed directly to the
 *  target var. It's used by the callback-based options as the callback call
 *  may append later on.
 */
#define M_OPT_TYPE_INDIRECT		(1<<3)

///@}

///////////////////////////// Parser flags ////////////////////////////////////////

/// \defgroup OptionParserModes Option parser modes
/// \ingroup Options
///
/// Some parsers behave differently depending on the mode passed in the src
/// parameter of m_option_type::parse. For example the flag type doesn't take
/// an argument when parsing from the command line.
///@{

/// Set when parsing from a config file.
#define M_CONFIG_FILE 0
/// Set when parsing command line arguments.
#define M_COMMAND_LINE 1
/// Set when pre-parsing the command line
#define M_COMMAND_LINE_PRE_PARSE 2

///@}

/// \defgroup OptionParserReturn Option parser return code
/// \ingroup Options
///
/// On success parsers return the number of arguments consumed: 0 or 1.
///
/// To indicate that MPlayer should exit without playing anything,
/// parsers return M_OPT_EXIT minus the number of parameters they
/// consumed: \ref M_OPT_EXIT or \ref M_OPT_EXIT-1.
///
/// On error one of the following (negative) error codes is returned:
///@{

/// For use by higher level APIs when the option name is invalid.
#define M_OPT_UNKNOWN		-1

/// Returned when a parameter is needed but wasn't provided.
#define M_OPT_MISSING_PARAM	-2

/// Returned when the given parameter couldn't be parsed.
#define M_OPT_INVALID		-3

/// \brief Returned if the value is "out of range". The exact meaning may 
/// vary from type to type.
#define M_OPT_OUT_OF_RANGE	-4

/// Returned if the parser failed for any other reason than a bad parameter.
#define M_OPT_PARSER_ERR	-5

/// Returned when MPlayer should exit. Used by various help stuff.
/** M_OPT_EXIT must be the lowest number on this list.
 */
#define M_OPT_EXIT              -6

/// \defgroup OldOptionParserReturn Backward compatibility
///
/// These are kept for compatibility with older code.
///
///@{
#define ERR_NOT_AN_OPTION	M_OPT_UNKNOWN
#define ERR_MISSING_PARAM	M_OPT_MISSING_PARAM
#define ERR_OUT_OF_RANGE	M_OPT_OUT_OF_RANGE
#define ERR_FUNC_ERR		M_OPT_PARSER_ERR
///@}

///@}

/// Find the option matching the given name in the list.
/** \ingroup Options
 *  This function takes the possible wildcards into account (see
 *  \ref M_OPT_TYPE_ALLOW_WILDCARD).
 * 
 *  \param list Pointer to an array of \ref m_option.
 *  \param name Name of the option.
 *  \return The matching option or NULL.
 */
const m_option_t* m_option_list_find(const m_option_t* list,const char* name);

/// Helper to parse options, see \ref m_option_type::parse.
inline static int
m_option_parse(const m_option_t* opt,const char *name, char *param, void* dst, int src) {
  return opt->type->parse(opt,name,param,dst,src);
}

/// Helper to print options, see \ref m_option_type::print.
inline static  char*
m_option_print(const m_option_t* opt, const void* val_ptr) {
  if(opt->type->print)
    return opt->type->print(opt,val_ptr);
  else
    return (char*)-1;
}

/// Helper around \ref m_option_type::save.
inline static  void
m_option_save(const m_option_t* opt,void* dst, void* src) {
  if(opt->type->save)
    opt->type->save(opt,dst,src);
}

/// Helper around \ref m_option_type::set.
inline static  void
m_option_set(const m_option_t* opt,void* dst, void* src) {
  if(opt->type->set)
    opt->type->set(opt,dst,src);
}

/// Helper around \ref m_option_type::copy.
inline  static void
m_option_copy(const m_option_t* opt,void* dst, void* src) {
  if(opt->type->copy)
    opt->type->copy(opt,dst,src);
  else if(opt->type->size > 0)
    memcpy(dst,src,opt->type->size);
}

/// Helper around \ref m_option_type::free.
inline static void
m_option_free(const m_option_t* opt,void* dst) {
  if(opt->type->free)
    opt->type->free(dst);
}

/*@}*/

#endif /* MPLAYER_M_OPTION_H */
