#ifndef MPLAYER_M_CONFIG_H
#define MPLAYER_M_CONFIG_H

/// \defgroup Config Config manager
///
/// m_config provides an API to manipulate the config variables in MPlayer.
/// It makes use of the \ref Options API to provide a context stack that
/// allows saving and later restoring the state of all variables.
///@{

/// \file

typedef struct m_config_option m_config_option_t;
typedef struct m_config_save_slot m_config_save_slot_t;
/// \ingroup ConfigProfiles
typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;

/// Config option save slot
struct m_config_save_slot {
  /// Previous level slot.
  m_config_save_slot_t* prev;
  /// Level at which the save was made.
  int lvl;
  // We have to store other datatypes in this as well,
  // so make sure we get properly aligned addresses.
  unsigned char data[0] __attribute__ ((aligned (8)));
};

/// Config option
struct m_config_option {
  m_config_option_t* next;
  /// Full name (ie option:subopt).
  char* name;
  /// Option description.
  const struct m_option* opt;
  /// Save slot stack.
  m_config_save_slot_t* slots;
  /// See \ref ConfigOptionFlags.
  unsigned int flags;
};

/// \defgroup ConfigProfiles Config profiles
/// \ingroup Config
///
/// Profiles allow to predefine some sets of options that can then
/// be applied later on with the internal -profile option.
///
///@{

/// Config profile
struct m_profile {
  m_profile_t* next;
  char* name;
  char* desc;
  int num_opts;
  /// Option/value pair array.
  char** opts;
};

///@}

/// Config object
/** \ingroup Config */
typedef struct m_config {
  /// Registered options.
  /** This contains all options and suboptions.
   */ 
  m_config_option_t* opts;
  /// Current stack level.
  int lvl;
  /// \ref OptionParserModes
  int mode;
  /// List of defined profiles.
  m_profile_t* profiles;
  /// Depth when recursively including profiles.
  int profile_depth;
  /// Options defined by the config itself.
  struct m_option* self_opts;
} m_config_t;

/// \defgroup ConfigOptionFlags Config option flags
/// \ingroup Config
///@{

/// Set if an option has been set at the current level.
#define M_CFG_OPT_SET    (1<<0)

/// Set if another option already uses the same variable.
#define M_CFG_OPT_ALIAS  (1<<1)

///@}

/// Create a new config object.
/** \ingroup Config
 */
m_config_t*
m_config_new(void);

/// Free a config object.
void
m_config_free(m_config_t* config);

/// Push a new context.
/** \param config The config object.
 */
void
m_config_push(m_config_t* config);

/// Pop the current context restoring the previous context state.
/** \param config The config object.
 */
void
m_config_pop(m_config_t* config);

/// Register some options to be used.
/** \param config The config object.
 *  \param args An array of \ref m_option struct.
 *  \return 1 on success, 0 on failure.
 */
int
m_config_register_options(m_config_t *config, const struct m_option *args);

/// Set an option.
/** \param config The config object.
 *  \param arg The option's name.
 *  \param param The value of the option, can be NULL.
 *  \return See \ref OptionParserReturn.
 */
int
m_config_set_option(m_config_t *config, char* arg, char* param);

/// Check if an option setting is valid.
/** \param config The config object.
 *  \param arg The option's name.
 *  \param param The value of the option, can be NULL.
 *  \return See \ref OptionParserReturn.
 */
int
m_config_check_option(m_config_t *config, char* arg, char* param);

/// Get the option matching the given name.
/** \param config The config object.
 *  \param arg The option's name.
 */
const struct m_option*
m_config_get_option(m_config_t *config, char* arg);

/// Print a list of all registered options.
/** \param config The config object.
 */
void
m_config_print_option_list(m_config_t *config);

/// \addtogroup ConfigProfiles
///@{

/// Find the profile with the given name.
/** \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object or NULL.
 */
m_profile_t*
m_config_get_profile(m_config_t* config, char* name);

/// Get the profile with the given name, creating it if necessary.
/** \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object.
 */
m_profile_t*
m_config_add_profile(m_config_t* config, char* name);

/// Set the description of a profile.
/** Used by the config file parser when defining a profile.
 * 
 *  \param p The profile object.
 *  \param arg The profile's name.
 */
void
m_profile_set_desc(m_profile_t* p, char* desc);

/// Add an option to a profile.
/** Used by the config file parser when defining a profile.
 * 
 *  \param config The config object.
 *  \param p The profile object.
 *  \param name The option's name.
 *  \param val The option's value.
 */
int
m_config_set_profile_option(m_config_t* config, m_profile_t* p,
			    char* name, char* val);

/// Enables profile usage
/** Used by the config file parser when loading a profile.
 * 
 *  \param config The config object.
 *  \param p The profile object.
 */
void
m_config_set_profile(m_config_t* config, m_profile_t* p);

///@}

///@}

#endif /* MPLAYER_M_CONFIG_H */
