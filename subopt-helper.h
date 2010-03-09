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

#ifndef MPLAYER_SUBOPT_HELPER_H
#define MPLAYER_SUBOPT_HELPER_H

/**
 * \file subopt-helper.h
 *
 * \brief Datatype and functions declarations for usage
 *        of the suboption parser.
 *
 */

#define OPT_ARG_BOOL 0
#define OPT_ARG_INT  1
#define OPT_ARG_STR  2
#define OPT_ARG_MSTRZ 3 ///< A malloced, zero terminated string, use free()!
#define OPT_ARG_FLOAT 4

typedef int (*opt_test_f)(void *);

/** simple structure for defining the option name, type and storage location */
typedef struct opt_s
{
  const char * name; ///< string that identifies the option
  int type;    ///< option type as defined in subopt-helper.h
  void * valp; ///< pointer to the mem where the value should be stored
  opt_test_f test; ///< argument test func ( optional )
} opt_t;

/** parses the string for the options specified in opt */
int subopt_parse( char const * const str, const opt_t * opts );


/*------------------ arg specific types and declaration -------------------*/
typedef struct strarg_s
{
  int len; ///< length of the string determined by the parser
  char const * str;  ///< pointer to position inside the parse string
} strarg_t;


int int_non_neg(void *iptr);
int int_pos(void *iptr);

int strargcmp(strarg_t *arg, const char *str);
int strargcasecmp(strarg_t *arg, char *str);

#endif /* MPLAYER_SUBOPT_HELPER_H */
