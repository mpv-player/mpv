
#include "config.h"

#ifdef NEW_CONFIG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "mp_msg.h"
#include "m_option.h"
#include "m_config.h"
#include "playtree.h"

static int recursion_depth = 0;
static int mode = 0;

#define GLOBAL 0
#define LOCAL 1
#define DROP_LOCAL 2

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
      return 1;
  } else if(strcasecmp(opt,"vcd") == 0) {
    char* s;
    if(!param)
      return M_OPT_MISSING_PARAM;
    s = (char*)malloc((strlen(param) + 6 + 1)*sizeof(char));
    sprintf(s,"vcd://%s",param);
    entry = play_tree_new();
    play_tree_add_file(entry,s);
    free(s);
  } else if(strcasecmp(opt,"dvd") == 0) {
    char* s;
    if(!param)
      return M_OPT_MISSING_PARAM;
    s = (char*)malloc((strlen(param) + 6 + 1)*sizeof(char));
    sprintf(s,"dvd://%s",param);
    entry = play_tree_new();
    play_tree_add_file(entry,s);
    free(s);
  } else if(strcasecmp(opt,"tv") == 0) {
    char *s,*pr,*prs;
    char *ps,*pe,*channel=NULL;
    char *as;
    int on=0;

    if(!param)
      return M_OPT_MISSING_PARAM;
    ps = param;
    pe = strchr(param,':');
    pr = prs = (char*)malloc((strlen(param)+1)*sizeof(char));
    pr[0] = '\0';
    while(ps) {
      if(!pe)
	pe = ps + strlen(ps);

      as = strchr(ps,'=');
      if(as && as[1] != '\0' && pe-as > 0)
	as++;
      else
	as = NULL;
      if( !as && pe-ps == 2 &&  strncasecmp("on",ps,2) == 0 )
	on = 1;
      else if(as  && as-ps == 8  && strncasecmp("channel",ps,6) == 0 && pe-as > 0) {
	channel = (char*)realloc(channel,(pe-as+1)*sizeof(char));
	strncpy(channel,as,pe-as);
	channel[pe-as] = '\0';
      } else if(pe-ps > 0) {
	if(prs != pr) {
	  prs[0] = ':';
	  prs++;
	}
	strncpy(prs,ps,pe-ps);
	prs += pe-ps;
	prs[0] = '\0';
      }

      if(pe[0] != '\0') {
	ps = pe+1;
	pe = strchr(ps,':');
      } else
	ps = NULL;
    }

    if(on) {
      int l=5;
	
      if(channel)
	l += strlen(channel);
      s = (char*) malloc((l+1)*sizeof(char));
      if(channel)
	sprintf(s,"tv://%s",channel);
      else
	sprintf(s,"tv://");
      entry = play_tree_new();
      play_tree_add_file(entry,s);
      if(strlen(pr) > 0)
	play_tree_set_param(entry,"tv",pr);
      free(s);
    }
    free(pr);
    if(channel)
      free(channel);
	  
  }

  if(entry) {
    *ret = entry;
    return 1;
  } else
    return 0;
}

play_tree_t*
m_config_parse_mp_command_line(m_config_t *config, int argc, char **argv)
{
  int i;
  int tmp = 0;
  char *opt;
  int no_more_opts = 0;
  play_tree_t *last_parent, *last_entry = NULL, *root;
  void add_entry(play_tree_t *entry) {
    if(last_entry == NULL)
      play_tree_set_child(last_parent,entry);		      
    else 
      play_tree_append_entry(last_entry,entry);
    last_entry = entry;
  }

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(argv != NULL);
  assert(argc >= 1);
#endif

  config->mode = M_COMMAND_LINE;
  mode = GLOBAL;
  last_parent = root = play_tree_new();
  /* in order to work recursion detection properly in parse_config_file */
  ++recursion_depth;

  for (i = 1; i < argc; i++) {
    //next:
    opt = argv[i];
    /* check for -- (no more options id.) except --help! */
    if ((*opt == '-') && (*(opt+1) == '-') && (*(opt+2) != 'h'))
      {
	no_more_opts = 1;
	if (i+1 >= argc)
	  {
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "You added '--' but no filenames presented!\n");
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
	/* remove trailing '-' */
	opt++;

	mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "this_opt = option: %s\n", opt);
	// We handle here some specific option
	if(strcasecmp(opt,"list-options") == 0) {
	  m_config_print_option_list(config);
	  exit(1);
	  // Loop option when it apply to a group
	} else if(strcasecmp(opt,"loop") == 0 &&
		  (! last_entry || last_entry->child) ) {
	  int l;
	  char* end;
	  l = (i+1<argc) ? strtol(argv[i+1],&end,0) : 0;
	  if(*end != '\0') {
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The loop option must be an integer: %s\n",argv[i+1]);
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
	  m_option_t* mp_opt = NULL;
	  play_tree_t* entry = NULL;

	  tmp = is_entry_option(opt,(i+1<argc) ? argv[i + 1] : NULL,&entry);
	  if(tmp > 0)  { // It's an entry
	    if(entry) {
	      add_entry(entry);
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
	      tmp = M_OPT_UNKNOW;
	      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Unknow option on the command line: %s\n",opt);
	    }
	  }
	}

	if (tmp < 0)
	  goto err_out;
	i += tmp;
      }
    else /* filename */
      {
	play_tree_t* entry = play_tree_new();
	mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Adding file %s\n",argv[i]);
	play_tree_add_file(entry,argv[i]);
	// Lock stdin if it will be used as input
	if(strcasecmp(argv[i],"-") == 0)
	  m_config_set_option(config,"use-stdin",NULL);
	add_entry(entry);
	UNSET_GLOBAL; // We start entry specific options

      }
  }

  --recursion_depth;
  if(last_parent != root)
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Missing }- ?\n");
  return root;

 err_out:
  --recursion_depth;
  play_tree_free(root,1);
  return NULL;
}

#endif
