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

#include "core/bstr.h"

// m_config provides an API to manipulate the config variables in MPlayer.
// It makes use of the Options API to provide a context stack that
// allows saving and later restoring the state of all variables.

typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;
struct m_sub_options;
struct m_obj_desc;

// Config option
struct m_config_option {
    struct m_config_option *next;
    // For positional parameters
    int pos;
    // Full name (ie option-subopt).
    char *name;
    // Option description.
    const struct m_option *opt;
    // Raw value of the option.
    void *data;
    // Raw value of the backup of the global value (or NULL).
    void *global_backup;
    // If this is a suboption, the option that contains this option.
    struct m_config_option *parent;
    // If this option aliases another, more important option. The alias_owner
    // option is the one that has the most correct option type for the data
    // variable, and which is considered the original.
    struct m_config_option *alias_owner;
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

// Config object
/** \ingroup Config */
typedef struct m_config {
    // Registered options.
    /** This contains all options and suboptions.
     */
    struct m_config_option *opts;
    int num_pos_opts;
    // When options are set (via m_config_set_option or m_config_set_profile),
    // back up the old value (unless it's already backed up). Used for restoring
    // global options when per-file options are set.
    bool file_local_mode;

    // List of defined profiles.
    struct m_profile *profiles;
    // Depth when recursively including profiles.
    int profile_depth;

    void *optstruct; // struct mpopts or other
    int (*includefunc)(struct m_config *conf, char *filename);
    bool use_profiles;
} m_config_t;

// Create a new config object.
struct m_config *
m_config_new(void *optstruct,
             int includefunc(struct m_config *conf, char *filename));

struct m_config *m_config_simple(void *optstruct);

struct m_config *m_config_from_obj_desc(void *talloc_parent,
                                        struct m_obj_desc *desc);

int m_config_set_obj_params(struct m_config *conf, char **args);

// Initialize an object (VO/VF/...) in one go, including legacy handling.
// This is pretty specialized, and is just for convenience.
int m_config_initialize_obj(struct m_config *config, struct m_obj_desc *desc,
                            void **ppriv, char ***pargs);

// Free a config object.
void m_config_free(struct m_config *config);

void m_config_enter_file_local(struct m_config *config);
void m_config_leave_file_local(struct m_config *config);
void m_config_mark_file_local(struct m_config *config, const char *opt);
void m_config_mark_all_file_local(struct m_config *config);

/*  Register some options to be used.
 *  \param config The config object.
 *  \param args An array of \ref m_option struct.
 *  \return 1 on success, 0 on failure.
 */
int m_config_register_options(struct m_config *config,
                              const struct m_option *args);

enum {
    M_SETOPT_PRE_PARSE_ONLY = 1,    // Silently ignore non-M_OPT_PRE_PARSE opt.
    M_SETOPT_CHECK_ONLY = 2,        // Don't set, just check name/value
    M_SETOPT_FROM_CONFIG_FILE = 4,  // Reject M_OPT_NOCFG opt. (print error)
};

// Set the named option to the given string.
// flags: combination of M_SETOPT_* flags (0 for normal operation)
// Returns >= 0 on success, otherwise see OptionParserReturn.
int m_config_set_option_ext(struct m_config *config, struct bstr name,
                            struct bstr param, int flags);

/*  Set an option. (Like: m_config_set_option_ext(config, name, param, 0))
 *  \param config The config object.
 *  \param name The option's name.
 *  \param param The value of the option, can be NULL.
 *  \return See \ref OptionParserReturn.
 */
int m_config_set_option(struct m_config *config, struct bstr name,
                        struct bstr param);

static inline int m_config_set_option0(struct m_config *config,
                                       const char *name, const char *param)
{
    return m_config_set_option(config, bstr0(name), bstr0(param));
}

int m_config_parse_suboptions(struct m_config *config, char *name,
                              char *subopts);


/*  Get the option matching the given name.
 *  \param config The config object.
 *  \param name The option's name.
 */
const struct m_option *m_config_get_option(const struct m_config *config,
                                           struct bstr name);

struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name);

// Return the n-th option by position. n==0 is the first option. If there are
// less than (n + 1) options, return NULL.
const char *m_config_get_positional_option(const struct m_config *config, int n);

// Return a hint to the option parser whether a parameter is/may be required.
// The option may still accept empty/non-empty parameters independent from
// this, and this function is useful only for handling ambiguous options like
// flags (e.g. "--a" is ok, "--a=yes" is also ok).
// Returns: error code (<0), or number of expected params (0, 1)
int m_config_option_requires_param(struct m_config *config, bstr name);

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
                                bstr name, bstr val);

/*  Enables profile usage
 *  Used by the config file parser when loading a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 */
void m_config_set_profile(struct m_config *config, struct m_profile *p);

void *m_config_alloc_struct(void *talloc_parent,
                            const struct m_sub_options *subopts);

#endif /* MPLAYER_M_CONFIG_H */
