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

#ifndef MPLAYER_PLAYTREEPARSER_H
#define MPLAYER_PLAYTREEPARSER_H

#include "playtree.h"

/// \defgroup PlaytreeParser Playtree parser
/// \ingroup Playtree
///
/// The playtree parser allows to read various playlist formats. It reads from
/// a stream allowing to handle playlists from local files and the network.
///@{

/// \file

struct stream;

typedef struct play_tree_parser {
  struct stream *stream;
  char *buffer,*iter,*line;
  int buffer_size , buffer_end;
  int deep,keep;
} play_tree_parser_t;

/// Create a new parser.
/** \param stream The stream to read from.
 *  \param deep Parser depth. Some formats allow including other files,
 *              this is used to track the inclusion depth.
 *  \return The new parser.
 */
play_tree_parser_t*
play_tree_parser_new(struct stream *stream, int deep);

/// Destroy a parser.
void
play_tree_parser_free(play_tree_parser_t* p);

/// Build a playtree from the playlist opened with the parser.
/** \param p The parser.
 *  \param forced If non-zero the playlist file was explicitly
 *                given by the user, allow falling back on
 *                one filename per line playlist.
 *  \return A new playtree or NULL on error.
 */
play_tree_t*
play_tree_parser_get_play_tree(play_tree_parser_t* p, int forced);

/// Wrapper for play_tree_add_basepath (add base path from file).
void
play_tree_add_bpf(play_tree_t* pt, char* filename);

///@}

#endif /* MPLAYER_PLAYTREEPARSER_H */
