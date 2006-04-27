
/// \defgroup PlaytreeParser Playtree parser
/// \ingroup Playtree
///
/// The playtree parser allows to read various playlist formats. It reads from
/// a stream allowing to handle playlists from local files and the network.
///@{

/// \file

#ifndef __PLAYTREEPARSER_H
#define __PLAYTREEPARSER_H

struct stream_st;

typedef struct play_tree_parser {
  struct stream_st* stream;
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
play_tree_parser_new(struct stream_st* stream,int deep);

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

#endif

///@}
