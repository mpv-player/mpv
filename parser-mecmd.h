#ifndef MPLAYER_PARSER_MECMD_H
#define MPLAYER_PARSER_MECMD_H

#include "m_config.h"

/// \file
/// \ingroup ConfigParsers MEntry
/// \brief A simple parser with per-entry settings.

/// \defgroup MEntry MEncoder's playlist
///@{

/// Playlist entry
typedef struct m_entry_st {
  /// Filename, url or whatever.
  char* name;
  /// NULL terminated list of name,val pairs.
  char** opts;
} m_entry_t;

/// Free a list returned by \ref m_config_parse_me_command_line.
void
m_entry_list_free(m_entry_t* lst);

/// Helper to set all config options from an entry.
int
m_entry_set_options(m_config_t *config, m_entry_t* entry);

/// Setup the \ref Config from command line arguments and build a playlist.
/** \ingroup ConfigParsers
 */
m_entry_t*
m_config_parse_me_command_line(m_config_t *config, int argc, char **argv);

///@}

#endif /* MPLAYER_PARSER_MECMD_H */
