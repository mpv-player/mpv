
#ifndef __PLAYTREEPARSER_H
#define __PLAYTREEPARSER_H

#include "playtree.h"
#include "libmpdemux/stream.h"

typedef struct play_tree_parser {
  stream_t* stream;
  char* buffer;
  int buffer_size , buffer_end;
  int deep;
} play_tree_parser_t;


play_tree_parser_t*
play_tree_parser_new(stream_t* stream,int deep);

void
play_tree_parser_free(play_tree_parser_t* p);

play_tree_t*
play_tree_parser_get_play_tree(play_tree_parser_t* p);

#endif
