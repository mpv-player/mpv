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

#ifndef MPLAYER_M_CONFIG_H
#define MPLAYER_M_CONFIG_H

#include <stdbool.h>

#include "bstr.h"

// m_config provides an API to manipulate the config variables in MPlayer.
// It makes use of the Options API to provide a context stack that
// allows saving and later restoring the state of all variables.

typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;

// Config option save slot
struct m_config_save_slot {
    // Previous level slot.
    struct m_config_save_slot *prev;
    // Level at which the save was made.
    int lvl;
    // We have to store other datatypes in this as well,
    // so make sure we get properly aligned addresses.
    unsigned char data[0] __attribute__ ((aligned(8)));
};

// Config option
struct m_config_option {
    struct m_config_option *next;
    // Full name (ie option:subopt).
    char *name;
    // Compiled without support for this option? If so set to name of feature
    char *disabled_feature;
    // Option description.
    const struct m_option *opt;
    // Save slot stack.
    struct m_config_save_slot *slots;
    // See \ref ConfigOptionFlags.
    unsigned int flags;
};

// Profiles allow to predefine some sets of options that can then
// be applied later on with the internal -profile option.

// Config profile
struct m_profile {
    struct m_profile *next;
    char *name;
    char *desc;
    int num_opts;
    // Option/value pair array.
    char **opts;
};

enum option_source {
    // Set when parsing from a config file.
    M_CONFIG_FILE,
    // Set when parsing command line arguments.
    M_COMMAND_LINE,
    // Set when pre-parsing the command line
    M_COMMAND_LINE_PRE_PARSE,
};

// Config object
/** \ingroup Config */
typedef struct m_config {
    // Registered options.
    /** This contains all options and suboptions.
     */
    struct m_config_option *opts;
    // Current stack level.
    int lvl;
    enum option_source mode;
    // List of defined profiles.
    struct m_profile *profiles;
    // Depth when recursively including profiles.
    int profile_depth;

    void *optstruct; // struct mpopts or other
} m_config_t;


// Set if an option has been set at the current level.
#define M_CFG_OPT_SET    (1 << 0)

// Set if another option already uses the same variable.
#define M_CFG_OPT_ALIAS  (1 << 1)

// Create a new config object.
struct m_config *
m_config_new(void *optstruct,
             int includefunc(struct m_option *conf, char *filename));

// Free a config object.
void m_config_free(struct m_config *config);

/* Push a new context.
 * \param config The config object.
 */
void m_config_push(struct m_config *config);

/* Pop the current context restoring the previous context state.
 * \param config The config object.
 */
void m_config_pop(struct m_config *config);

/*  Register some options to be used.
 *  \param config The config object.
 *  \param args An array of \ref m_option struct.
 *  \return 1 on success, 0 on failure.
 */
int m_config_register_options(struct m_config *config,
                              const struct m_option *args);

/*  Set an option.
 *  \param config The config object.
 *  \param name The option's name.
 *  \param param The value of the option, can be NULL.
 *  \param ambiguous_param: old style cmdline option, "param" may be a
           parameter to this option or something entirely unrelated
 *  \return See \ref OptionParserReturn.
 */
int m_config_set_option(struct m_config *config, struct bstr name,
                        struct bstr param, bool ambiguous_param);

static inline int m_config_set_option0(struct m_config *config,
                                       const char *name, const char *param,
                                       bool ambiguous)
{
    return m_config_set_option(config, bstr(name), bstr(param), ambiguous);
}

/*  Check if an option setting is valid.
 *  Same as above m_config_set_option() but doesn't actually set anything.
 */
int m_config_check_option(const struct m_config *config, struct bstr name,
                          struct bstr param, bool ambiguous_param);

static inline int m_config_check_option0(struct m_config *config,
                                         const char *name, const char *param,
                                         bool ambiguous)
{
    return m_config_check_option(config, bstr(name), bstr(param), ambiguous);
}


/*  Get the option matching the given name.
 *  \param config The config object.
 *  \param name The option's name.
 */
const struct m_option *m_config_get_option(const struct m_config *config,
                                           struct bstr name);

/*  Print a list of all registered options.
 *  \param config The config object.
 */
void m_config_print_option_list(const struct m_config *config);


/*  Find the profile with the given name.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object or NULL.
 */
struct m_profile *m_config_get_profile(const struct m_config *config,
                                       char *name);

/*  Get the profile with the given name, creating it if necessary.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object.
 */
struct m_profile *m_config_add_profile(struct m_config *config, char *name);

/*  Set the description of a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param p The profile object.
 *  \param arg The profile's name.
 */
void m_profile_set_desc(struct m_profile *p, char *desc);

/*  Add an option to a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 *  \param name The option's name.
 *  \param val The option's value.
 */
int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                char *name, char *val);

/*  Enables profile usage
 *  Used by the config file parser when loading a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 */
void m_config_set_profile(struct m_config *config, struct m_profile *p);

#endif /* MPLAYER_M_CONFIG_H */
