/** 
 * \file subopt-helper.c
 *
 * \brief Compensates the suboption parsing code duplication a bit.
 *
 * The routines defined below are there to help you with the
 * suboption parsing. Meaning extracting the options and their
 * values for you and also outputting generic help message if
 * a parse error is encountered.
 *
 * Most stuff happens in the subopt_parse function: if you call it
 * it parses for the passed opts in the passed string. It calls some
 * extra functions for explicit argument parsing ( where the option
 * itself isn't the argument but a value given after the argument
 * delimiter ('='). It also calls your test function if you supplied
 * one.
 *
 */

#include "subopt-helper.h"
#include "mp_msg.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifndef MPDEBUG
  #define NDEBUG
#endif

/* prototypes for argument parsing */
static char const * parse_int( char const * const str, int * const valp );
static char const * parse_str( char const * const str, strarg_t * const valp );
static char const * parse_float( char const * const str, float * const valp );

/**
 * \brief Try to parse all options in str and fail if it was not possible.
 *
 * \param str Pointer to the zero terminated string to be parsed.
 * \param opts Pointer to a options array. The array must be terminated
 *             with an element having set name to NULL in its opt_t structure.
 *
 * \return The return value is zero if the string could be parsed
 *         else a non-zero value is returned.
 *
 */
int subopt_parse( char const * const str, opt_t * opts )
{
  int parse_err = 0, idx;
  unsigned int parse_pos = 0;

  /* Initialize set member to false.          *
   * It is set to true if it was found in str */
  for ( idx=0; opts[idx].name; ++idx )
  {
    opts[idx].set = 0;
  }

  if ( str )
  {
    while ( str[parse_pos] && !parse_err )
    {
      int next = 0;

      idx = 0; // reset index for the below loop
      while ( opts[idx].name )
      {
        int opt_len;
        int substr_len;

        // get length of the option we test against */
        opt_len = strlen( opts[idx].name );

        // get length of the current substring of str */
        {
          char * delim, * arg_delim;

          /* search nearest delimiter ( option or argument delimiter ) */ 
          delim = strchr( &str[parse_pos], ':' );
          arg_delim = strchr( &str[parse_pos], '=' );

          if ( ( delim && arg_delim && delim > arg_delim ) ||
               delim == NULL )
          {
            delim = strchr( &str[parse_pos], '=' );
          }
          
          substr_len = delim ? // is a delim present
                         delim - &str[parse_pos] : // yes
                         strlen( &str[parse_pos] ); // no, end of string
        }

        //printf( "substr_len=%d, opt_len=%d\n", substr_len, opt_len );

        /* Check if the length of the current option matches the *
         * length of the option we want to test again.           */
        if ( substr_len == opt_len )
{
        /* check if option was activated/deactivated */
        if( strncmp( &str[parse_pos], opts[idx].name, opt_len ) == 0 )
        {
          /* option was found */
          opts[idx].set = 1; next = 1;

          assert( opts[idx].valp && "Need a pointer to store the arg!" );

          /* type specific code */
          if ( opts[idx].type == OPT_ARG_BOOL )
          {
            /* Handle OPT_ARG_BOOL separately so *
             * the others can share code.        */

            /* set option to true */
            *((int *)(opts[idx].valp)) = 1;

            /* increment position */
            parse_pos += opt_len;
          }
          else
          {
            /* Type is not OPT_ARG_BOOL, means we have to parse *
             * for the arg delimiter character and eventually   *
             * call a test function.                            */
            char const * last;

            /* increment position to check for arg */
            parse_pos += opt_len;

            if ( str[parse_pos] != '=' )
            {
              parse_err = 1; break;
            }

            /* '=' char was there, so let's move after it */
            ++parse_pos;

            switch ( opts[idx].type )
            {
              case OPT_ARG_INT:
                last = parse_int( &str[parse_pos],
                                  (int *)opts[idx].valp );

                break;
              case OPT_ARG_STR:
                last = parse_str( &str[parse_pos],
                                  (strarg_t *)opts[idx].valp );
                break;
              case OPT_ARG_MSTRZ:
                {
                  char **valp = opts[idx].valp;
                  strarg_t tmp;
                  tmp.str = NULL;
                  tmp.len = 0;
                  last = parse_str( &str[parse_pos], &tmp );
                  if (*valp)
                    free(*valp);
                  *valp = NULL;
                  if (tmp.str && tmp.len > 0) {
                    *valp = malloc(tmp.len + 1);
                    memcpy(*valp, tmp.str, tmp.len);
                    (*valp)[tmp.len] = 0;
                  }
                  break;
                }
              case OPT_ARG_FLOAT:
                last = parse_float( &str[parse_pos],
                                  (float *)opts[idx].valp );
                break;
              default:
                assert( 0 && "Arg type of suboption doesn't exist!" );
                last = NULL; // break parsing!
            }

            /* was the conversion succesful? */
            if ( !last )
            {
              parse_err = 1; break;
            }

            /* make test if supplied */
            if ( opts[idx].test && !opts[idx].test( opts[idx].valp ) )
            {
              parse_err = 1; break;
            }

            /* we succeded, set position */
            parse_pos = last - str;
          }
        }
}
else if ( substr_len == opt_len+2 )
{
             if ( opts[idx].type == OPT_ARG_BOOL && // check for no<opt>
                  strncmp( &str[parse_pos], "no", 2 ) == 0 &&
                  strncmp( &str[parse_pos+2], opts[idx].name, opt_len ) == 0 )
        {
          /* option was found but negated */
          opts[idx].set = 1; next = 1;

          /* set arg to false */
          *((int *)(opts[idx].valp)) = 0;

          /* increment position */
          parse_pos += opt_len+2;
        }
}

        ++idx; // test against next option

        /* break out of the loop, if this subopt is processed */
        if ( next ) { break; }
      }
      
      /* if we had a valid suboption the current pos should *
       * equal the delimiter char, which should be ':' for  *
       * suboptions.                                        */
      if ( !parse_err && str[parse_pos] == ':' ) { ++parse_pos; }
      else if ( str[parse_pos] ) { parse_err = 1; }
    }
  }

  /* if an error was encountered */
  if (parse_err)
  {
    unsigned int i;
    mp_msg( MSGT_VO, MSGL_FATAL, "Could not parse arguments at the position indicated below:\n%s\n", str );
    for ( i = 0; i < parse_pos; ++i )
    {
      mp_msg(MSGT_VO, MSGL_FATAL, " ");
    }
    mp_msg(MSGT_VO, MSGL_FATAL, "^\n");

    return -1;
  }

  /* we could parse everything */
  return 0;
}

static char const * parse_int( char const * const str, int * const valp )
{
  char * endp;

  assert( str && "parse_int(): str == NULL" );

  *valp = (int)strtol( str, &endp, 0 );

  /* nothing was converted */
  if ( str == endp ) { return NULL; }

  return endp;
}

static char const * parse_float( char const * const str, float * const valp )
{
  char * endp;

  assert( str && "parse_float(): str == NULL" );

  *valp = strtod( str, &endp );

  /* nothing was converted */
  if ( str == endp ) { return NULL; }

  return endp;
}

#define QUOTE_CHAR '%'
static char const * parse_str( char const * str, strarg_t * const valp )
{
  char const * match = strchr( str, ':' );

  if (str[0] == QUOTE_CHAR) {
    int len = 0;
    str = &str[1];
    len = (int)strtol(str, (char **)&str, 0);
    if (!str || str[0] != QUOTE_CHAR || (len > strlen(str) - 1))
      return NULL;
    str = &str[1];
    match = &str[len];
  }
  else
  if (str[0] == '"') {
    str = &str[1];
    match = strchr(str, '"');
    if (!match)
      return NULL;
    valp->len = match - str;
    valp->str = str;
    return &match[1];
  }
  if ( !match )
    match = &str[strlen(str)];

  // empty string or too long
  if ((match == str) || (match - str > INT_MAX))
    return NULL;

  valp->len = match - str;
  valp->str = str;

  return match;
}


/*** common test functions ***/

/** \brief Test if i is not negative */
int int_non_neg( int * i )
{
  if ( *i < 0 ) { return 0; }

  return 1;
}
/** \brief Test if i is positive. */
int int_pos( int * i )
{
  if ( *i > 0 ) { return 1; }

  return 0;
}

/*** little helpers */

/** \brief compare the stings just as strcmp does */
int strargcmp(strarg_t *arg, const char *str) {
  int res = strncmp(arg->str, str, arg->len);
  if (!res && arg->len != strlen(str))
    res = arg->len - strlen(str);
  return res;
}

/** \brief compare the stings just as strcasecmp does */
int strargcasecmp(strarg_t *arg, char *str) {
  int res = strncasecmp(arg->str, str, arg->len);
  if (!res && arg->len != strlen(str))
    res = arg->len - strlen(str);
  return res;
}

