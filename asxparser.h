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

#ifndef MPLAYER_ASXPARSER_H
#define MPLAYER_ASXPARSER_H

#include "playtree.h"

typedef struct ASX_Parser_t ASX_Parser_t;

typedef struct {
  char* buffer;
  int line;
} ASX_LineSave_t;

struct ASX_Parser_t {
  int line; // Curent line
  ASX_LineSave_t *ret_stack;
  int ret_stack_size;
  char* last_body;
  int deep;
};

ASX_Parser_t*
asx_parser_new(void);

void
asx_parser_free(ASX_Parser_t* parser);

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
int
asx_get_element(ASX_Parser_t* parser,char** _buffer,
                char** _element,char** _body,char*** _attribs);

int
asx_parse_attribs(ASX_Parser_t* parser,char* buffer,char*** _attribs);

/////// Attribs utils

char*
asx_get_attrib(const char* attrib,char** attribs);

int
asx_attrib_to_enum(const char* val,char** valid_vals);

#define asx_free_attribs(a) asx_list_free(&a,free)

////// List utils

typedef void (*ASX_FreeFunc)(void* arg);

void
asx_list_free(void* list_ptr,ASX_FreeFunc free_func);

play_tree_t*
asx_parser_build_tree(char* buffer,int deep);

#endif /* MPLAYER_ASXPARSER_H */
