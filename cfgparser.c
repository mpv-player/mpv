/*
 * command line and config file parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 *
 * subconfig support by alex
 */

//#define DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "config.h"

#include "mp_msg.h"

#define COMMAND_LINE		0
#define CONFIG_FILE		1

#define LIST_SEPARATOR ','

#define CONFIG_GLOBAL (1<<0)
#define CONFIG_RUNNING (1<<1)

#define SET_GLOBAL(c)  (c->flags |= CONFIG_GLOBAL)
#ifdef GLOBAL_OPTIONS_ONLY
#define UNSET_GLOBAL(c)
#else
#define UNSET_GLOBAL(c) (c->flags &= (!CONFIG_GLOBAL))
#endif
#define IS_GLOBAL(c) (c->flags & CONFIG_GLOBAL)
#define SET_RUNNING(c) (c->flags |= CONFIG_RUNNING)
#define IS_RUNNING(c) (c->flags & CONFIG_RUNNING)

#define MAX_RECURSION_DEPTH	8

#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "cfgparser.h"

static void m_config_list_options(m_config_t *config);
static void m_config_error(int err,char* opt,char* val);

static void
m_config_save_option(m_config_t* config, config_t* conf,char* opt, char *param) {
  config_save_t* save;
  int sl=0;
 
#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->cs_level >= 0);
  assert(conf != NULL);
  assert(opt != NULL);
  assert( ! (conf->flags & CONF_NOSAVE));
#endif

  switch(conf->type) {
  case CONF_TYPE_PRINT :
  case CONF_TYPE_SUBCONFIG :
    return;
  default :
    ;
  }

  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Saving option %s\n",opt);

  save = config->config_stack[config->cs_level];

  if(save) {
    for(sl = 0; save[sl].opt != NULL; sl++){
      // Check to not save the same arg two times
      if(save[sl].opt == conf && (save[sl].opt_name == NULL || strcasecmp(save[sl].opt_name,opt) == 0))
	break;
    }
    if(save[sl].opt)
      return;
  }

  save = (config_save_t*)realloc(save,(sl+2)*sizeof(config_save_t));
  if(save == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",(sl+2)*sizeof(config_save_t),strerror(errno));
    return;
  }
  memset(&save[sl],0,2*sizeof(config_save_t));
  save[sl].opt = conf;
  
  switch(conf->type) {
  case CONF_TYPE_FLAG :
  case CONF_TYPE_INT :
    save[sl].param.as_int = *((int*)conf->p);
    break;
  case CONF_TYPE_FLOAT :
    save[sl].param.as_float = *((float*)conf->p);
    break;
  case CONF_TYPE_STRING :
    save[sl].param.as_pointer = *((char**)conf->p);
    break;   
  case CONF_TYPE_FUNC_FULL :
    if(strcasecmp(conf->name,opt) != 0) save->opt_name = strdup(opt);
  case CONF_TYPE_FUNC_PARAM :
    if(param)
      save->param.as_pointer = strdup(param);
  case CONF_TYPE_FUNC :    
    break;
  case CONF_TYPE_STRING_LIST :
    save[sl].param.as_pointer = *((char***)conf->p);
    break;
  case CONF_TYPE_POSITION :
    save[sl].param.as_off_t = *((off_t*)conf->p);
    break;
  default :
    mp_msg(MSGT_CFGPARSER,MSGL_ERR,"Should never append in m_config_save_option : conf->type=%d\n",conf->type);
  }

  config->config_stack[config->cs_level] = save;
}

static int
m_config_revert_option(m_config_t* config, config_save_t* save) {
  char* arg = NULL;
  config_save_t* iter=NULL;
  int i=-1;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->cs_level >= 0);
  assert(save != NULL);
#endif


  arg = save->opt_name ? save->opt_name : save->opt->name;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Reverting option %s\n",arg);

  if(save->opt->default_func)
    save->opt->default_func(save->opt,arg);

  switch(save->opt->type) {
  case CONF_TYPE_FLAG :
  case CONF_TYPE_INT :
    *((int*)save->opt->p) = save->param.as_int;
    break;
  case CONF_TYPE_FLOAT :
    *((float*)save->opt->p) = save->param.as_float;
    break;
  case CONF_TYPE_STRING :
    *((char**)save->opt->p) = save->param.as_pointer;
    break;
  case CONF_TYPE_STRING_LIST :
    *((char***)save->opt->p) = save->param.as_pointer;
    break;
  case CONF_TYPE_FUNC_PARAM :
  case CONF_TYPE_FUNC_FULL :
  case CONF_TYPE_FUNC :
    if(config->cs_level > 0) {
      for(i = config->cs_level - 1 ; i >= 0 ; i--){
	if(config->config_stack[i] == NULL) continue;
	for(iter = config->config_stack[i]; iter != NULL && iter->opt != NULL ; iter++) {
	  if(iter->opt == save->opt && 
	     ((save->param.as_pointer == NULL || iter->param.as_pointer == NULL) || strcasecmp(save->param.as_pointer,iter->param.as_pointer) == 0) && 
	     (save->opt_name == NULL || 
	      (iter->opt_name && strcasecmp(save->opt_name,iter->opt_name)))) break;
	}
      }
    }
    free(save->param.as_pointer);
    if(save->opt_name) free(save->opt_name);
    save->opt_name = save->param.as_pointer = NULL;
    if(i < 0) break;
    arg = iter->opt_name ? iter->opt_name : iter->opt->name;
    switch(iter->opt->type) {
    case CONF_TYPE_FUNC :
      if ((((cfg_func_t) iter->opt->p)(iter->opt)) < 0)
	return -1;
      break;
    case CONF_TYPE_FUNC_PARAM :
      if (iter->param.as_pointer == NULL) {
	mp_msg(MSGT_CFGPARSER,MSGL_ERR,"We lost param for option %s?\n",iter->opt->name);
	return -1;
      } 
      if ((((cfg_func_param_t) iter->opt->p)(iter->opt, (char*)iter->param.as_pointer)) < 0)
	return -1;
      break;
    case CONF_TYPE_FUNC_FULL :
      if (iter->param.as_pointer != NULL && ((char*)iter->param.as_pointer)[0]=='-'){
	if( ((cfg_func_arg_param_t) iter->opt->p)(iter->opt, arg, NULL) < 0)
	  return -1;
      }else {
	if (((cfg_func_arg_param_t) save->opt->p)(iter->opt, arg, (char*)iter->param.as_pointer) < 0) 
	  return -1;
	  
      }
      break;
    }
    break;
  case CONF_TYPE_POSITION :
    *((off_t*)save->opt->p) = save->param.as_off_t;
    break;
  default :
    mp_msg(MSGT_CFGPARSER,MSGL_WARN,"Why do we reverse this : name=%s type=%d ?\n",save->opt->name,save->opt->type);
  }
			
  return 1;
}

void
m_config_push(m_config_t* config) {

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->cs_level >= 0);
#endif

  config->cs_level++;
  config->config_stack = (config_save_t**)realloc(config->config_stack ,sizeof(config_save_t*)*(config->cs_level+1));
  if(config->config_stack == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",sizeof(config_save_t*)*(config->cs_level+1),strerror(errno));
    config->cs_level = -1;
    return;
  }
  config->config_stack[config->cs_level] = NULL;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Config pushed level=%d\n",config->cs_level);
}

int
m_config_pop(m_config_t* config) {
  int i,ret= 1;
  config_save_t* cs;
  
#ifdef MP_DEBUG
  assert(config != NULL);
  //assert(config->cs_level > 0);
#endif

  if(config->config_stack[config->cs_level] != NULL) {
    cs = config->config_stack[config->cs_level];
    for(i=0; cs[i].opt != NULL ; i++ ) {
      if (m_config_revert_option(config,&cs[i]) < 0)
	ret = -1;
    }
    free(config->config_stack[config->cs_level]);
  }
  config->config_stack = (config_save_t**)realloc(config->config_stack ,sizeof(config_save_t*)*config->cs_level);
  config->cs_level--;
  if(config->cs_level > 0 && config->config_stack == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",sizeof(config_save_t*)*config->cs_level,strerror(errno));
    config->cs_level = -1;
    return -1;
  }
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Config poped level=%d\n",config->cs_level);
  return ret;
}

m_config_t*
m_config_new(play_tree_t* pt) {
  m_config_t* config;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  config = (m_config_t*)calloc(1,sizeof(m_config_t));
  if(config == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",sizeof(m_config_t),strerror(errno));
    return NULL;
  }
  config->config_stack = (config_save_t**)calloc(1,sizeof(config_save_t*));
  if(config->config_stack == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",sizeof(config_save_t*),strerror(errno));
    free(config);
    return NULL;
  }
  SET_GLOBAL(config); // We always start with global options
  config->pt = pt;
  return config;
}

void
m_config_free(m_config_t* config) {

#ifdef MP_DEBUG
  assert(config != NULL);
#endif

  free(config->opt_list);
  free(config->config_stack);
  free(config);
}


static int init_conf(m_config_t *config, int mode)
{
#ifdef MP_DEBUG
	assert(config != NULL);
	assert(config->pt != NULL);
	assert(config->last_entry == NULL || config->last_entry->parent == config->pt);

	if (mode != COMMAND_LINE && mode != CONFIG_FILE) {
		mp_msg(MSGT_CFGPARSER, MSGL_ERR, "init_conf: wrong mode!\n");
		return -1;
	}
#endif
	config->parser_mode = mode;
	return 1;
}

static int config_is_entry_option(m_config_t *config, char *opt, char *param) {
  play_tree_t* entry = NULL;

#ifdef MP_DEBUG
  assert(config->pt != NULL);
#endif

  if(strcasecmp(opt,"playlist") == 0) { // We handle playlist here
    if(!param)
      return ERR_MISSING_PARAM;
    entry = parse_playlist_file(param);
    if(!entry) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Playlist parsing failed: %s\n",param);
      return 1;
    }
  }

  if(! IS_RUNNING(config)) {
    if(strcasecmp(opt,"vcd") == 0) {
      char* s;
      if(!param)
	return ERR_MISSING_PARAM;
      s = (char*)malloc((strlen(param) + 6 + 1)*sizeof(char));
      sprintf(s,"vcd://%s",param);
      entry = play_tree_new();
      play_tree_add_file(entry,s);
      free(s);
    } else if(strcasecmp(opt,"dvd") == 0) {
      char* s;
      if(!param)
	return ERR_MISSING_PARAM;
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
	return ERR_MISSING_PARAM;
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
  }

  if(entry) {
    if(config->last_entry)
      play_tree_append_entry(config->last_entry,entry);
    else
      play_tree_set_child(config->pt,entry);
    config->last_entry = entry;
    if(config->parser_mode == COMMAND_LINE)
      UNSET_GLOBAL(config);
    return 1;
  } else
    return 0;
}



static int config_read_option(m_config_t *config,config_t** conf_list, char *opt, char *param)
{
	int i=0,nconf = 0;
	long tmp_int;
	off_t tmp_off;
	double tmp_float;
	int dummy;
	int ret = -1;
	char *endptr;
	config_t* conf=NULL;

#ifdef MP_DEBUG
	assert(config != NULL);
	assert(conf_list != NULL);
	assert(opt != NULL);
#endif

	mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "read_option: conf=%p opt='%s' param='%s'\n",
	    conf, opt, param);
	for(nconf = 0 ;  conf_list[nconf] != NULL; nconf++) {
	  conf = conf_list[nconf];
		for (i = 0; conf[i].name != NULL; i++) {
			int namelength;
			/* allow 'aa*' in config.name */
			namelength=strlen(conf[i].name);
			if ( (conf[i].name[namelength-1]=='*') && 
				    !memcmp(opt, conf[i].name, namelength-1))
			  goto option_found;
			if (!strcasecmp(opt, conf[i].name))
			  goto option_found;
		}
	}
	if (config->parser_mode == CONFIG_FILE)
	  mp_msg(MSGT_CFGPARSER, MSGL_ERR, "invalid option: %s\n",opt);
	ret = ERR_NOT_AN_OPTION;
	goto out;	
	option_found :
	mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "read_option: name='%s' p=%p type=%d\n",
	    conf[i].name, conf[i].p, conf[i].type);

	if (conf[i].flags & CONF_NOCFG && config->parser_mode == CONFIG_FILE) {
		mp_msg(MSGT_CFGPARSER, MSGL_ERR, "this option can only be used on command line:\n", opt);
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	if (conf[i].flags & CONF_NOCMD && config->parser_mode == COMMAND_LINE) {
		mp_msg(MSGT_CFGPARSER, MSGL_ERR, "this option can only be used in config file:\n", opt);
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	ret = config_is_entry_option(config,opt,param);
	if(ret != 0)
	  return ret;
	else
	  ret = -1;
	if(! IS_RUNNING(config) && ! IS_GLOBAL(config) && 
	   ! (conf[i].flags & CONF_GLOBAL)  && conf[i].type != CONF_TYPE_SUBCONFIG  )
	  m_config_push(config);
	if( !(conf[i].flags & CONF_NOSAVE) && ! (conf[i].flags & CONF_GLOBAL) )
	  m_config_save_option(config,&conf[i],opt,param);
	switch (conf[i].type) {
		case CONF_TYPE_FLAG:
			/* flags need a parameter in config file */
			if (config->parser_mode == CONFIG_FILE) {
				if (!strcasecmp(param, "yes") ||	/* any other language? */
				    !strcasecmp(param, "ja") ||
				    !strcasecmp(param, "si") ||
				    !strcasecmp(param, "igen") ||
				    !strcasecmp(param, "y") ||
				    !strcasecmp(param, "j") ||
				    !strcasecmp(param, "i") ||
				    !strcmp(param, "1"))
					*((int *) conf[i].p) = conf[i].max;
				else if (!strcasecmp(param, "no") ||
				    !strcasecmp(param, "nein") ||
				    !strcasecmp(param, "nicht") ||
				    !strcasecmp(param, "nem") ||
				    !strcasecmp(param, "n") ||
				    !strcmp(param, "0"))
					*((int *) conf[i].p) = conf[i].min;
				else {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "invalid parameter for flag: %s\n", param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
				ret = 1;
			} else {	/* parser_mode == COMMAND_LINE */
				*((int *) conf[i].p) = conf[i].max;
				ret = 0;
			}
			break;
		case CONF_TYPE_INT:
			if (param == NULL)
				goto err_missing_param;

			tmp_int = strtol(param, &endptr, 0);
			if (*endptr) {
				mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be an integer: %s\n", param);
				ret = ERR_OUT_OF_RANGE;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_int < conf[i].min) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be >= %d: %s\n", (int) conf[i].min, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_int > conf[i].max) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be <= %d: %s\n", (int) conf[i].max, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((int *) conf[i].p) = tmp_int;
			ret = 1;
			break;
		case CONF_TYPE_FLOAT:
			if (param == NULL)
				goto err_missing_param;

			tmp_float = strtod(param, &endptr);

			if ((*endptr == ':') || (*endptr == '/'))
				tmp_float /= strtod(endptr+1, &endptr);

			if (*endptr) {
				mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be a floating point number"
				       " or a ratio (numerator[:/]denominator): %s\n", param);
				ret = ERR_MISSING_PARAM;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_float < conf[i].min) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be >= %f: %s\n", conf[i].min, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_float > conf[i].max) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be <= %f: %s\n", conf[i].max, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((float *) conf[i].p) = tmp_float;
			ret = 1;
			break;
		case CONF_TYPE_STRING:
			if (param == NULL)
				goto err_missing_param;

			if (conf[i].flags & CONF_MIN)
				if (strlen(param) < conf[i].min) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be >= %d chars: %s\n",
							(int) conf[i].min, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (strlen(param) > conf[i].max) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be <= %d chars: %s\n",
							(int) conf[i].max, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
			*((char **) conf[i].p) = strdup(param);
			ret = 1;
			break;
		case CONF_TYPE_STRING_LIST:
			if (param == NULL)
				goto err_missing_param;
			else {
			  int n = 0,len;
			  char *ptr = param, *last_ptr, **res;

			  while(ptr[0] != '\0') {
			    last_ptr = ptr;
			    ptr = strchr(ptr,LIST_SEPARATOR);
			    if(!ptr) {
			      if(strlen(last_ptr) > 0)
				n++;
			      break;
			    }
			    ptr++;
			    n++;
			  }
			  if(n == 0)
			    goto err_missing_param;
			  else if( (conf[i].flags & CONF_MIN && n < conf[i].min) || 
				   (conf[i].flags & CONF_MAX && n > conf[i].max) ) {
			    ret = ERR_OUT_OF_RANGE;
			    goto out;
			  }
			  ret = 1;
			  res = malloc((n+1)*sizeof(char*));
			  ptr = param;
			  n = 0;
			  while(ptr[0] != '\0') {
			    last_ptr = ptr;
			    ptr = strchr(ptr,LIST_SEPARATOR);
			     if(!ptr) {
			       if(strlen(last_ptr) > 0) {
				 res[n] = strdup(last_ptr);
				 n++;
			       }
			       break;
			     }
			     len = ptr - last_ptr;
			     res[n] = (char*)malloc(len + 1);
			     strncpy(res[n],last_ptr,len);
			     res[n][len] = '\0';
			     ptr++;
			     n++;
			  }
			  res[n] = NULL;
			  *((char ***) conf[i].p) = res;
			}
			break;
		case CONF_TYPE_FUNC_PARAM:
			if (param == NULL)
				goto err_missing_param;
			if ((((cfg_func_param_t) conf[i].p)(conf + i, param)) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 1;
			break;
		case CONF_TYPE_FUNC_FULL:
			if (param!=NULL && param[0]=='-'){
			    ret=((cfg_func_arg_param_t) conf[i].p)(conf + i, opt, NULL);
			    if (ret>=0) ret=0;
			    /* if we return >=0: param is processed again (if there is any) */
			}else{
			    ret=((cfg_func_arg_param_t) conf[i].p)(conf + i, opt, param);
			    /* if we return 0: need no param, precess it again */
			    /* if we return 1: accepted param */
			}
			break;
		case CONF_TYPE_FUNC:
			if ((((cfg_func_t) conf[i].p)(conf + i)) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 0;
			break;
		case CONF_TYPE_SUBCONFIG:
		    {
			char *subparam;
			char *subopt;
			int subconf_optnr;
			config_t *subconf;
			config_t *sublist[] = { NULL , NULL };
			char *token;
			char *p;

			if (param == NULL)
				goto err_missing_param;

			subparam = malloc(strlen(param)+1);
			subopt = malloc(strlen(param)+1);
			p = strdup(param); // In case that param is a static string (cf man strtok)

			subconf = conf[i].p;
			sublist[0] = subconf;
			for (subconf_optnr = 0; subconf[subconf_optnr].name != NULL; subconf_optnr++)
			    /* NOTHING */;
			config->sub_conf = opt;
			token = strtok(p, (char *)&(":"));
			while(token)
			{
			    int sscanf_ret;
			    /* clear out */
			    subopt[0] = subparam[0] = 0;
			    
			    sscanf_ret = sscanf(token, "%[^=]=%[^:]", subopt, subparam);

			    mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "token: '%s', i=%d, subopt='%s', subparam='%s' (ret: %d)\n", token, i, subopt, subparam, sscanf_ret);
			    switch(sscanf_ret)
			    {
				case 1:
				    subparam[0] = 0;
				case 2:
				    if ((ret = config_read_option(config,sublist, subopt, subparam)) < 0)
				    {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Subconfig parsing returned error: %d in token: %s\n",
					    ret, token);
					goto out;
				    }
				    break;
				default:
				    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Invalid subconfig argument! ('%s')\n", token);
				    ret = ERR_NOT_AN_OPTION;
				    goto out;
			    }
			    token = strtok(NULL, (char *)&(":"));
			}
			config->sub_conf = NULL;
			free(subparam);
			free(subopt);
			free(p);
			ret = 1;
			break;
		    }
		case CONF_TYPE_PRINT:
			mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", (char *) conf[i].p);
			exit(1);
		case CONF_TYPE_POSITION:
			if (param == NULL)
				goto err_missing_param;

			if (sscanf(param, sizeof(off_t) == sizeof(int) ?
			"%d%c" : "%lld%c", &tmp_off, dummy) != 1) {
				mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be an integer: %s\n", param);
				ret = ERR_OUT_OF_RANGE;
				goto out;
			}

			if (conf[i].flags & CONF_MIN)
				if (tmp_off < conf[i].min) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR,
					(sizeof(off_t) == sizeof(int) ?
					"parameter must be >= %d: %s\n" :
					"parameter must be >= %lld: %s\n"),
					(off_t) conf[i].min, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (conf[i].flags & CONF_MAX)
				if (tmp_off > conf[i].max) {
					mp_msg(MSGT_CFGPARSER, MSGL_ERR,
					(sizeof(off_t) == sizeof(int) ?
					"parameter must be <= %d: %s\n" :
					"parameter must be <= %lld: %s\n"),
					(off_t) conf[i].max, param);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((off_t *) conf[i].p) = tmp_off;
			ret = 1;
			break;
		default:
			mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Unknown config type specified in conf-mplayer.h!\n");
			break;
	}
out:
	if(ret >= 0 && ! IS_RUNNING(config) && ! IS_GLOBAL(config) && ! (conf[i].flags & CONF_GLOBAL) && conf[i].type != CONF_TYPE_SUBCONFIG ) {
	  play_tree_t* dest = config->last_entry ? config->last_entry : config->last_parent;
	  char* o;
#ifdef MP_DEBUG
	  assert(dest != NULL);
#endif
	  if(config->sub_conf) {
	    o = (char*)malloc((strlen(config->sub_conf) + 1 + strlen(opt) + 1)*sizeof(char));
	    sprintf(o,"%s:%s",config->sub_conf,opt);
	  } else
	    o =strdup(opt);

	  if(ret == 0)
	    play_tree_set_param(dest,o,NULL);
	  else if(ret > 0)
	    play_tree_set_param(dest,o,param);
	  free(o);
	  m_config_pop(config); 
	}
	return ret;
err_missing_param:
	mp_msg(MSGT_CFGPARSER, MSGL_ERR, "missing parameter for option: %s\n", opt);
	ret = ERR_MISSING_PARAM;
	goto out;
}

int m_config_set_option(m_config_t *config,char *opt, char *param) {
  char *e;
#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->opt_list != NULL);
  assert(opt != NULL);
#endif
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2, "Setting option %s=%s\n",opt,param);
  e = strchr(opt,':');
  if(e && e[1] != '\0') {
    int ret;
    config_t* opt_list[] = { NULL, NULL };
    char* s = (char*)malloc((e-opt+1)*sizeof(char));
    strncpy(s,opt,e-opt);
    s[e-opt] = '\0';
    opt_list[0] = m_config_get_option_ptr(config,s);
    if(!opt_list[0]) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR,"m_config_set_option %s=%s : no %s subconfig\n",opt,param,s);
      free(s);
      return ERR_NOT_AN_OPTION;
    }
    e++;
    s = (char*)realloc(s,strlen(e) + 1);
    strcpy(s,e);
    ret = config_read_option(config,opt_list,s,param);
    free(s);
    return ret;
  }

  return config_read_option(config,config->opt_list,opt,param);
}

int m_config_parse_config_file(m_config_t *config, char *conffile)
{
#define PRINT_LINENUM	mp_msg(MSGT_CFGPARSER,MSGL_INFO,"%s(%d): ", conffile, line_num)
#define MAX_LINE_LEN	1000
#define MAX_OPT_LEN	100
#define MAX_PARAM_LEN	100
	FILE *fp;
	char *line;
	char opt[MAX_OPT_LEN + 1];
	char param[MAX_PARAM_LEN + 1];
	char c;		/* for the "" and '' check */
	int tmp;
	int line_num = 0;
	int line_pos;	/* line pos */
	int opt_pos;	/* opt pos */
	int param_pos;	/* param pos */
	int ret = 1;
	int errors = 0;

#ifdef MP_DEBUG
	assert(config != NULL);
	//	assert(conf_list != NULL);
#endif
	if (++config->recursion_depth > 1)
		mp_msg(MSGT_CFGPARSER,MSGL_INFO,"Reading config file: %s", conffile);

	if (config->recursion_depth > MAX_RECURSION_DEPTH) {
		mp_msg(MSGT_CFGPARSER,MSGL_ERR,": too deep 'include'. check your configfiles\n");
		ret = -1;
		goto out;
	}

	if (init_conf(config, CONFIG_FILE) == -1) {
		ret = -1;
		goto out;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		mp_msg(MSGT_CFGPARSER,MSGL_FATAL,"\ncan't get memory for 'line': %s", strerror(errno));
		ret = -1;
		goto out;
	}

	if ((fp = fopen(conffile, "r")) == NULL) {
		if (config->recursion_depth > 1)
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,": %s\n", strerror(errno));
		free(line);
		ret = 0;
		goto out;
	}
	if (config->recursion_depth > 1)
		mp_msg(MSGT_CFGPARSER,MSGL_INFO,"\n");

	while (fgets(line, MAX_LINE_LEN, fp)) {
		if (errors >= 16) {
			mp_msg(MSGT_CFGPARSER,MSGL_FATAL,"too many errors\n");
			goto out;
		}

		line_num++;
		line_pos = 0;

		/* skip whitespaces */
		while (isspace(line[line_pos]))
			++line_pos;

		/* EOL / comment */
		if (line[line_pos] == '\0' || line[line_pos] == '#')
			continue;

		/* read option. */
		for (opt_pos = 0; isprint(line[line_pos]) &&
				line[line_pos] != ' ' &&
				line[line_pos] != '#' &&
				line[line_pos] != '='; /* NOTHING */) {
			opt[opt_pos++] = line[line_pos++];
			if (opt_pos >= MAX_OPT_LEN) {
				PRINT_LINENUM;
				mp_msg(MSGT_CFGPARSER,MSGL_ERR,"too long option\n");
				errors++;
				ret = -1;
				goto nextline;
			}
		}
		if (opt_pos == 0) {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"parse error\n");
			ret = -1;
			errors++;
			continue;
		}
		opt[opt_pos] = '\0';

#ifdef MP_DEBUG
		PRINT_LINENUM;
		mp_msg(MSGT_CFGPARSER,MSGL_INFO,"option: %s\n", opt);
#endif

		/* skip whitespaces */
		while (isspace(line[line_pos]))
			++line_pos;

		/* check '=' */
		if (line[line_pos++] != '=') {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"option without parameter\n");
			ret = -1;
			errors++;
			continue;
		}

		/* whitespaces... */
		while (isspace(line[line_pos]))
			++line_pos;

		/* read the parameter */
		if (line[line_pos] == '"' || line[line_pos] == '\'') {
			c = line[line_pos];
			++line_pos;
			for (param_pos = 0; line[line_pos] != c; /* NOTHING */) {
				param[param_pos++] = line[line_pos++];
				if (param_pos >= MAX_PARAM_LEN) {
					PRINT_LINENUM;
					mp_msg(MSGT_CFGPARSER,MSGL_ERR,"too long parameter\n");
					ret = -1;
					errors++;
					goto nextline;
				}
			}
			line_pos++;	/* skip the closing " or ' */
		} else {
			for (param_pos = 0; isprint(line[line_pos]) && !isspace(line[line_pos])
					&& line[line_pos] != '#'; /* NOTHING */) {
				param[param_pos++] = line[line_pos++];
				if (param_pos >= MAX_PARAM_LEN) {
					PRINT_LINENUM;
					mp_msg(MSGT_CFGPARSER,MSGL_ERR,"too long parameter\n");
					ret = -1;
					errors++;
					goto nextline;
				}
			}
		}
		param[param_pos] = '\0';

		/* did we read a parameter? */
		if (param_pos == 0) {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"option without parameter\n");
			ret = -1;
			errors++;
			continue;
		}

#ifdef MP_DEBUG
		PRINT_LINENUM;
		mp_msg(MSGT_CFGPARSER,MSGL_INFO,"parameter: %s\n", param);
#endif

		/* now, check if we have some more chars on the line */
		/* whitespace... */
		while (isspace(line[line_pos]))
			++line_pos;

		/* EOL / comment */
		if (line[line_pos] != '\0' && line[line_pos] != '#') {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_WARN,"extra characters on line: %s\n", line+line_pos);
			ret = -1;
		}

		tmp = m_config_set_option(config, opt, param);
		switch (tmp) {
		case ERR_NOT_AN_OPTION:
		case ERR_MISSING_PARAM:
		case ERR_OUT_OF_RANGE:
		case ERR_FUNC_ERR:
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_INFO,"%s\n", opt);
			ret = -1;
			errors++;
			continue;
			/* break */
		}	
nextline:
		;
	}

	free(line);
	fclose(fp);
out:
	--config->recursion_depth;
	return ret;
}

int m_config_parse_command_line(m_config_t *config, int argc, char **argv)
{
	int i;
	int tmp;
	char *opt;
	int no_more_opts = 0;

#ifdef MP_DEBUG
	assert(config != NULL);
	assert(config->pt != NULL);
	assert(argv != NULL);
	assert(argc >= 1);
#endif
	
	if (init_conf(config, COMMAND_LINE) == -1)
		return -1;	
	if(config->last_parent == NULL)
	  config->last_parent = config->pt;
	/* in order to work recursion detection properly in parse_config_file */
	++config->recursion_depth;

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
		    UNSET_GLOBAL(config);		    
		    if(config->last_entry == NULL) {
		      play_tree_set_child(config->last_parent,entry);
		    } else {
		      play_tree_append_entry(config->last_entry,entry);
		      config->last_entry = NULL;
		    }
		    config->last_parent = entry;
		    continue;
		  }

		if((opt[0] == '}') && (opt[1] == '\0'))
		  {
		    if( ! config->last_parent || ! config->last_parent->parent) {
		      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "too much }-\n");
		      goto err_out;
		    }
		    config->last_entry = config->last_parent;
		    config->last_parent = config->last_entry->parent;
		    continue;
		  }
			
		if ((no_more_opts == 0) && (*opt == '-') && (*(opt+1) != 0)) /* option */
		{
		    /* remove trailing '-' */
		    opt++;

		    mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "this_opt = option: %s\n", opt);
		    // We handle here some specific option
		    if(strcasecmp(opt,"list-options") == 0) {
		      m_config_list_options(config);
		      exit(1);
		      // Loop option when it apply to a group
		    } else if(strcasecmp(opt,"loop") == 0 &&
			      (! config->last_entry || config->last_entry->child) ) {
		      int l;
		      char* end;
		      l = strtol(argv[i+1],&end,0);
		      if(!end)
			tmp = ERR_OUT_OF_RANGE;
		      else {
			play_tree_t* pt = config->last_entry ? config->last_entry : config->last_parent;
			l = l <= 0 ? -1 : l;
			pt->loop = l;
			tmp = 1;
		      }
		    } else // All normal options
		      tmp = m_config_set_option(config, opt, argv[i + 1]);

		    switch (tmp) {
		    case ERR_NOT_AN_OPTION:
		    case ERR_MISSING_PARAM:
		    case ERR_OUT_OF_RANGE:
		    case ERR_FUNC_ERR:
		      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Error: ");
		      m_config_error(tmp,opt,argv[i+1]);
			goto err_out;
		    default:
			i += tmp;
			break;
		    }
		}
		else /* filename */
		{
		    play_tree_t* entry = play_tree_new();
		    mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Adding file %s\n",argv[i]);
		    play_tree_add_file(entry,argv[i]);
		    if(strcasecmp(argv[i],"-") == 0)
		      m_config_set_option(config,"use-stdin",NULL);
		    /* opt is not an option -> treat it as a filename */
		    UNSET_GLOBAL(config); // We start entry specific options
		    if(config->last_entry == NULL)
		      play_tree_set_child(config->last_parent,entry);		      
		    else 
		      play_tree_append_entry(config->last_entry,entry);
		    config->last_entry = entry;
		}
	}

	--config->recursion_depth;
	if(config->last_parent != config->pt)
	  mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Missing }- ?\n");
	config->flags &= (!CONFIG_GLOBAL);
	SET_RUNNING(config);
	return 1; 
#if 0
err_out_mem:
	mp_msg(MSGT_CFGPARSER, MSGL_ERR, "can't allocate memory for filenames (%s)\n", strerror(errno));
#endif
err_out:
	--config->recursion_depth;
	mp_msg(MSGT_CFGPARSER, MSGL_ERR, "command line: %s\n", argv[i]);
	return -1;
}

int
m_config_register_options(m_config_t *config,config_t *args) {
  int list_len = 0;
  config_t** conf_list = config->opt_list;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(args != NULL);
#endif
  
  if(conf_list) {
    for ( ; conf_list[list_len] != NULL; list_len++)
      /* NOTHING */;
  }
  
  conf_list = (config_t**)realloc(conf_list,sizeof(struct conf*)*(list_len+2));
  if(conf_list == NULL) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Can't allocate %d bytes of memory : %s\n",sizeof(struct conf*)*(list_len+2),strerror(errno));
    return 0;
  }
  conf_list[list_len] = args;
  conf_list[list_len+1] = NULL;

  config->opt_list = conf_list;

  return 1;
}

config_t*
m_config_get_option(m_config_t *config, char* arg) {
  int i,j;
  char *e;
  config_t *conf;
  config_t **conf_list;
  config_t* cl[] = { NULL, NULL };

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  e = strchr(arg,':');

  if(e) {
    char *s;
    s = (char*)malloc((e-arg+1)*sizeof(char));
    strncpy(s,arg,e-arg);
    s[e-arg] = '\0';
    cl[0] = m_config_get_option(config,s);
    conf_list = cl;
    free(s);
  } else
    conf_list = config->opt_list;

  if(conf_list) {
    for(j = 0 ; conf_list[j] != NULL ; j++) {
      conf = conf_list[j];
      for(i=0; conf[i].name != NULL; i++) {
	if(strcasecmp(conf[i].name,arg) == 0)
	  return &conf[i];
      }
    }
  }
  return NULL;
}

void*
m_config_get_option_ptr(m_config_t *config, char* arg) {
  config_t* conf;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  conf = m_config_get_option(config,arg);
  if(!conf) return NULL;
  return conf->p;
}

int
m_config_get_int (m_config_t *config, char* arg,int* err_ret) {
  int *ret;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  ret = m_config_get_option_ptr(config,arg);
  if(err_ret)
    *err_ret = 0;
  if(!ret) {
    if(err_ret)
      *err_ret = 1;
    return -1;
  } else
    return (*ret);
}

float
m_config_get_float (m_config_t *config, char* arg,int* err_ret) {
  float *ret;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  ret = m_config_get_option_ptr(config,arg);
  if(err_ret)
    *err_ret = 0;
  if(!ret) {
    if(err_ret)
      *err_ret = 1;
    return -1;
  } else
    return (*ret);
}

#define AS_INT(c) (*((int*)c->p))

int
m_config_set_int(m_config_t *config, char* arg,int val) {
  config_t* opt;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  opt = m_config_get_option(config,arg);

  if(!opt || opt->type != CONF_TYPE_INT)
    return ERR_NOT_AN_OPTION;

  if(opt->flags & CONF_MIN && val < opt->min)
    return ERR_OUT_OF_RANGE;
  if(opt->flags & CONF_MAX && val > opt->max)
    return ERR_OUT_OF_RANGE;

  m_config_save_option(config,opt,arg,NULL);
  AS_INT(opt) = val;

  return 1;
}

int
m_config_set_float(m_config_t *config, char* arg,float val) {
  config_t* opt;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  opt = m_config_get_option(config,arg);

  if(!opt || opt->type != CONF_TYPE_FLOAT)
    return ERR_NOT_AN_OPTION;

  if(opt->flags & CONF_MIN && val < opt->min)
    return ERR_OUT_OF_RANGE;
  if(opt->flags & CONF_MAX && val > opt->max)
    return ERR_OUT_OF_RANGE;

  m_config_save_option(config,opt,arg,NULL);
  *((float*)opt->p) = val;

  return 1;
}


int
m_config_switch_flag(m_config_t *config, char* opt) {
  config_t *conf;
 
#ifdef MP_DEBUG
  assert(config != NULL);
  assert(opt != NULL);
#endif
 
  conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
  if( AS_INT(conf) == conf->min) AS_INT(conf) = conf->max;
  else if(AS_INT(conf) == conf->max) AS_INT(conf) = conf->min;
  else return 0;

  return 1;
}
    
int
m_config_set_flag(m_config_t *config, char* opt, int state) {
  config_t *conf;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(opt != NULL);
#endif

  conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return 0;
  if(state) AS_INT(conf) = conf->max;
  else AS_INT(conf) = conf->min;
  return 1;
}

int
m_config_get_flag(m_config_t *config, char* opt) {
  config_t *conf;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(opt != NULL);
#endif

  conf = m_config_get_option(config,opt);
  if(!conf || conf->type != CONF_TYPE_FLAG) return -1;
  if(AS_INT(conf) == conf->max)
    return 1;
  else if(AS_INT(conf) == conf->min)
    return 0;
  else
    return -1;
}

int m_config_is_option_set(m_config_t *config, char* arg) {
  config_t* opt;
  config_save_t* save;
  int l,i;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  opt = m_config_get_option(config,arg);

  if(!opt) 
    return -1;

  for(l = config->cs_level ; l >= 0 ; l--) {
    save = config->config_stack[l];
    if(!save) 
      continue;
    for(i = 0 ; save[i].opt != NULL ; i++) {
      if(save[i].opt == opt)
	return 1;
    }
  }

  return 0;
}

#undef AS_INT

static void m_config_print_option_list(char* prefix, config_t* opt_list) {
  char* pf = NULL;
  config_t* opt;
  char min[50],max[50],*type;

  
  for(opt = opt_list ; opt->name != NULL ; opt++) {
    if(opt->type == CONF_TYPE_SUBCONFIG) {
      if(prefix) {
	pf = (char*)malloc(strlen(prefix) + strlen(opt->name) + 1);
	sprintf(pf,"%s:%s",prefix,opt->name);
      } else
	pf = strdup(opt->name);
      m_config_print_option_list(pf,(config_t*)opt->p);
      free(pf);
      continue;
    }
    if(prefix)
      printf("%1.15s:",prefix);
    if(opt->flags & CONF_MIN)
      sprintf(min,"%-8.0f",opt->min);
    else
      strcpy(min,"No");
    if(opt->flags & CONF_MAX)
      sprintf(max,"%-8.0f",opt->max);
    else
      strcpy(max,"No");
    switch(opt->type) {
    case  CONF_TYPE_FLAG:
      type = "Flag";
      break;
    case CONF_TYPE_INT:
      type = "Integer";
      break;
    case CONF_TYPE_FLOAT:
      type = "Float";
      break;
    case CONF_TYPE_STRING:
      type = "String";
      break;
    case CONF_TYPE_FUNC:
    case CONF_TYPE_FUNC_PARAM:
    case CONF_TYPE_FUNC_FULL:
      type = "Function";
      break;
    case CONF_TYPE_PRINT:
      type = "Print";
      break;
    case CONF_TYPE_STRING_LIST:
      type = "String list";
      break;
    default:
      type = "";
      break;
    }
    printf("%-*.15s  %-13.13s %-10.10s %-10.10s %-3.3s %-3.3s %-3.3s\n",
	   30 - (prefix ? strlen(prefix) + 1 : 0),
	   opt->name,
	   type,
	   min,
	   max,
	   opt->flags & CONF_GLOBAL ? "Yes" : "No",
	   opt->flags & CONF_NOCMD ? "No" : "Yes",
	   opt->flags & CONF_NOCFG ? "No" : "Yes");
  }

}


static void m_config_list_options(m_config_t *config) {
  int i;
                                                                         
  printf("\nName                            Type          Min        Max       Glob CL  Cfg\n\n");
  for(i = 0; config->opt_list[i] ; i++)
    m_config_print_option_list(NULL,config->opt_list[i]);
}
  


static void m_config_error(int err,char* opt,char* val) {
  switch(err) {
  case ERR_NOT_AN_OPTION:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"'%s' is not a mplayer/mencoder option\n",opt);
    break;
  case ERR_MISSING_PARAM:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"option '%s' need a parameter\n",opt);
    break;
  case ERR_OUT_OF_RANGE:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"value '%s' of option '%s' is out of range\n",val,opt);
    break;
  case ERR_FUNC_ERR:
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"while parsing option '%s'\n",opt);
    break;
  }
}
