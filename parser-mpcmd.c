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

/// \file
/// \ingroup ConfigParsers Playtree

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_config.h"
#include "playtree.h"
#include "parser-mpcmd.h"
#include "osdep/macosx_finder_args.h"

static int recursion_depth = 0;
static int mode = 0;

#define GLOBAL 0
#define LOCAL 1
#define DROP_LOCAL 2

#define dvd_range(a)  (a>0 && a<256)
#define UNSET_GLOBAL (mode = LOCAL)
// Use this 1 if you want to have only global option (no per file option)
// #define UNSET_GLOBAL (mode = GLOBAL)


static int is_entry_option(char *opt, char *param, play_tree_t** ret) {
  play_tree_t* entry = NULL;

  *ret = NULL;

  if(strcasecmp(opt,"playlist") == 0) { // We handle playlist here
    if(!param)
      return M_OPT_MISSING_PARAM;

    entry = parse_playlist_file(param);
    if(!entry)
      return -1;
    else {
       *ret=entry;
       return 1;
    }
  }
    return 0;
}

static inline void add_entry(play_tree_t **last_parentp,
	play_tree_t **last_entryp, play_tree_t *entry) {
    if(*last_entryp == NULL)
      play_tree_set_child(*last_parentp,entry);
    else
      play_tree_append_entry(*last_entryp,entry);
    *last_entryp = entry;
}

/// Setup the \ref Config from command line arguments and build a playtree.
/** \ingroup ConfigParsers
 */
play_tree_t*
m_config_parse_mp_command_line(m_config_t *config, int argc, char **argv)
{
  int i,j,start_title=-1,end_title=-1;
  char *opt,*splitpos=NULL;
  char entbuf[15];
  int no_more_opts = 0;
  int opt_exit = 0; // flag indicating whether mplayer should exit without playing anything
  play_tree_t *last_parent, *last_entry = NULL, *root;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(argv != NULL);
  assert(argc >= 1);
#endif

  config->mode = M_COMMAND_LINE;
  mode = GLOBAL;
#ifdef CONFIG_MACOSX_FINDER
  root=macosx_finder_args(config, argc, argv);
  if(root)
  	return root;
#endif

  last_parent = root = play_tree_new();
  /* in order to work recursion detection properly in parse_config_file */
  ++recursion_depth;

  for (i = 1; i < argc; i++) {
    //next:
    opt = argv[i];
    /* check for -- (no more options id.) except --help! */
    if ((*opt == '-') && (*(opt+1) == '-') && (*(opt+2) == 0))
      {
	no_more_opts = 1;
	if (i+1 >= argc)
	  {
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_NoFileGivenOnCommandLine);
	    goto err_out;
	  }
	continue;
      }
    if((opt[0] == '{') && (opt[1] == '\0'))
      {
	play_tree_t* entry = play_tree_new();
	UNSET_GLOBAL;
	if(last_parent->flags & PLAY_TREE_RND)
	  entry->flags |= PLAY_TREE_RND;
	if(last_entry == NULL) {
	  play_tree_set_child(last_parent,entry);
	} else {
	  play_tree_append_entry(last_entry,entry);
	  last_entry = NULL;
	}
	last_parent = entry;
	continue;
      }

    if((opt[0] == '}') && (opt[1] == '\0'))
      {
	if( ! last_parent || ! last_parent->parent) {
	  mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too much }-\n");
	  goto err_out;
	}
	last_entry = last_parent;
	last_parent = last_entry->parent;
	continue;
      }

    if ((no_more_opts == 0) && (*opt == '-') && (*(opt+1) != 0)) /* option */
      {
	int tmp = 0;
	/* remove trailing '-' */
	opt++;

	mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "this_opt = option: %s\n", opt);
	// We handle here some specific option
	// Loop option when it apply to a group
	if(strcasecmp(opt,"loop") == 0 &&
		  (! last_entry || last_entry->child) ) {
	  int l;
	  char* end = NULL;
	  l = (i+1<argc) ? strtol(argv[i+1],&end,0) : 0;
	  if(!end || *end != '\0') {
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_TheLoopOptionMustBeAnInteger, argv[i+1]);
	    tmp = ERR_OUT_OF_RANGE;
	  } else {
	    play_tree_t* pt = last_entry ? last_entry : last_parent;
	    l = l <= 0 ? -1 : l;
	    pt->loop = l;
	    tmp = 1;
	  }
	} else if(strcasecmp(opt,"shuffle") == 0) {
	  if(last_entry && last_entry->child)
	    last_entry->flags |= PLAY_TREE_RND;
	  else
	    last_parent->flags |= PLAY_TREE_RND;
	} else if(strcasecmp(opt,"noshuffle") == 0) {
	  if(last_entry && last_entry->child)
	    last_entry->flags &= ~PLAY_TREE_RND;
	  else
	    last_parent->flags &= ~PLAY_TREE_RND;
	} else {
	  const m_option_t* mp_opt = NULL;
	  play_tree_t* entry = NULL;

	  tmp = is_entry_option(opt,(i+1<argc) ? argv[i + 1] : NULL,&entry);
	  if(tmp > 0)  { // It's an entry
	    if(entry) {
	      add_entry(&last_parent,&last_entry,entry);
	      if((last_parent->flags & PLAY_TREE_RND) && entry->child)
		entry->flags |= PLAY_TREE_RND;
	      UNSET_GLOBAL;
	    } else if(mode == LOCAL) // Entry is empty we have to drop his params
	      mode = DROP_LOCAL;
	  } else if(tmp == 0) { // 'normal' options
	    mp_opt = m_config_get_option(config,opt);
	    if (mp_opt != NULL) { // Option exist
	      if(mode == GLOBAL || (mp_opt->flags & M_OPT_GLOBAL))
                tmp = (i+1<argc) ? m_config_set_option(config, opt, argv[i + 1])
				 : m_config_set_option(config, opt, NULL);
	      else {
		tmp = m_config_check_option(config, opt, (i+1<argc) ? argv[i + 1] : NULL);
		if(tmp >= 0 && mode != DROP_LOCAL) {
		  play_tree_t* pt = last_entry ? last_entry : last_parent;
		  play_tree_set_param(pt,opt, argv[i + 1]);
		}
	      }
	    } else {
	      tmp = M_OPT_UNKNOWN;
	      mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_UnknownOptionOnCommandLine, opt);
	    }
	  }
	}

	if (tmp <= M_OPT_EXIT) {
	  opt_exit = 1;
	  tmp = M_OPT_EXIT - tmp;
	} else
	if (tmp < 0) {
	  mp_msg(MSGT_CFGPARSER, MSGL_FATAL, MSGTR_ErrorParsingOptionOnCommandLine, opt);
	  goto err_out;
	}
	i += tmp;
      }
    else /* filename */
      {
        int is_dvdnav = strstr(argv[i],"dvdnav://") != NULL;
	play_tree_t* entry = play_tree_new();
	mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Adding file %s\n",argv[i]);
        // if required expand DVD filename entries like dvd://1-3 into component titles
        if ( strstr(argv[i],"dvd://") != NULL || is_dvdnav)
	{
             int offset = is_dvdnav ? 9 : 6;
             splitpos=strstr(argv[i]+offset,"-");
             if(splitpos != NULL)
             {
               start_title=strtol(argv[i]+offset,NULL,10);
	       if (start_title<0) { //entries like dvd://-2 start title implied 1
		   end_title=abs(start_title);
                   start_title=1;
               } else {
                   end_title=strtol(splitpos+1,NULL,10);
               }

               if (dvd_range(start_title) && dvd_range(end_title) && (start_title<end_title))
               {
                 for (j=start_title;j<=end_title;j++)
                 {
                  if (j!=start_title)
                      entry=play_tree_new();
                  snprintf(entbuf,sizeof(entbuf),is_dvdnav ? "dvdnav://%d" : "dvd://%d",j);
                  play_tree_add_file(entry,entbuf);
                  add_entry(&last_parent,&last_entry,entry);
		  last_entry = entry;
                 }
               } else {
                 mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_InvalidPlayEntry, argv[i]);
               }

	     } else { // dvd:// or dvd://x entry
                play_tree_add_file(entry,argv[i]);
             }
        } else {
	play_tree_add_file(entry,argv[i]);
	}

	// Lock stdin if it will be used as input
	if(strcasecmp(argv[i],"-") == 0)
	  m_config_set_option(config,"noconsolecontrols",NULL);
	add_entry(&last_parent,&last_entry,entry);
	UNSET_GLOBAL; // We start entry specific options

      }
  }

  if (opt_exit)
    goto err_out;
  --recursion_depth;
  if(last_parent != root)
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Missing }- ?\n");
  return root;

 err_out:
  --recursion_depth;
  play_tree_free(root,1);
  return NULL;
}
