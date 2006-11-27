
/// \file
/// \ingroup ConfigParsers MEntry

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
#include "parser-mecmd.h"

void
m_entry_list_free(m_entry_t* lst) {
  int i,j;

  for(i = 0 ; lst[i].name != NULL ; i++){
    free(lst[i].name);
    for(j = 0 ; lst[i].opts[2*j] != NULL ; j++) {
      free(lst[i].opts[2*j]);
      free(lst[i].opts[2*j+1]);
    }
    free(lst[i].opts);
  }
  free(lst);
}

int
m_entry_set_options(m_config_t *config, m_entry_t* entry) {
  int i,r;

  for(i = 0 ; entry->opts[2*i] != NULL ; i++){
    r = m_config_set_option(config,entry->opts[2*i],entry->opts[2*i+1]);
    if(r < 0)
      return 0;
  }
  return 1;
}


  

m_entry_t*
m_config_parse_me_command_line(m_config_t *config, int argc, char **argv)
{
  int i,nf = 0,no = 0;
  int tmp;
  char *opt;
  int no_more_opts = 0;
  int opt_exit = 0;
  m_entry_t *lst = NULL, *entry = NULL;
	
#ifdef MP_DEBUG
  assert(config != NULL);
  assert(argv != NULL);
  assert(argc >= 1);
#endif

  config->mode = M_COMMAND_LINE;

  lst = calloc(1,sizeof(m_entry_t));

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
			
    if ((no_more_opts == 0) && (*opt == '-') && (*(opt+1) != 0)) /* option */
      {
	m_option_t* mp_opt = NULL;
	/* remove trailing '-' */
	opt++;
	mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "this_opt = option: %s\n", opt);
	mp_opt = m_config_get_option(config,opt);
	if(!mp_opt) {
	  tmp = M_OPT_UNKNOWN;
	  mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_NotAnMEncoderOption, opt);
	  goto err_out;
	}
	if(!entry || (mp_opt->flags & M_OPT_GLOBAL)){
	  tmp = m_config_set_option(config, opt, argv[i + 1]);
	  if (tmp <= M_OPT_EXIT) {
	    opt_exit = 1;
	    tmp = M_OPT_EXIT - tmp;
	  }
	  else
	  if(tmp < 0){
//	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "m_config_set_option() failed (%d)\n",tmp);
	    mp_msg(MSGT_CFGPARSER, MSGL_FATAL, MSGTR_ErrorParsingOptionOnCommandLine, opt);
	    goto err_out;
	  }
	} else {
	  tmp = m_config_check_option(config, opt, argv[i + 1]);
	  if (tmp <= M_OPT_EXIT) {
	    opt_exit = 1;
	    tmp = M_OPT_EXIT - tmp;
	  }
	  if(tmp >= 0) {
	    entry->opts = realloc(entry->opts,(no+2)*2*sizeof(char*));
	    entry->opts[2*no] = strdup(opt);
	    entry->opts[2*no+1] = argv[i + 1] ? strdup(argv[i + 1]) : NULL;
	    entry->opts[2*no+2] =  entry->opts[2*no+3] = NULL;
	    no++;
	  } else {
//	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "m_config_set_option() failed (%d)\n",tmp);
	    goto err_out;
	  }
	}
	i += tmp;
      } else  {/* filename */
	mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Adding file %s\n",argv[i]);
	lst = realloc(lst,(nf+2)*sizeof(m_entry_t));
	lst[nf].name = strdup(argv[i]);
	lst[nf].opts = calloc(2,sizeof(char*));
	entry = &lst[nf];
	no = 0;
	memset(&lst[nf+1],0,sizeof(m_entry_t));
	nf++;
      }
  }

  if (opt_exit)
    exit(0);
  if(nf == 0) {
    m_entry_list_free(lst);
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_NoFileGiven);
    return NULL;
  }
  return lst;

 err_out:
   m_entry_list_free(lst);
  return NULL;
}
