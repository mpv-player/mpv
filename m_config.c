
/// \file
/// \ingroup Config

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "m_config.h"
#include "m_option.h"
#include "mp_msg.h"
#include "help_mp.h"

#define MAX_PROFILE_DEPTH 20

static int
parse_profile(m_option_t* opt,char *name, char *param, void* dst, int src);

static void
set_profile(m_option_t *opt, void* dst, void* src);

static int
show_profile(m_option_t *opt, char* name, char *param);

static void
m_config_add_option(m_config_t *config, m_option_t *arg, char* prefix);

static int
list_options(m_option_t *opt, char* name, char *param);

m_config_t*
m_config_new(void) {
  m_config_t* config;
  static int inited = 0;
  static m_option_type_t profile_opt_type;
  static m_option_t ref_opts[] = {
    { "profile", NULL, &profile_opt_type, CONF_NOSAVE, 0, 0, NULL },
    { "show-profile", show_profile, CONF_TYPE_PRINT_FUNC, CONF_NOCFG, 0, 0, NULL },
    { "list-options", list_options, CONF_TYPE_PRINT_FUNC, CONF_NOCFG, 0, 0, NULL },
    { NULL, NULL, NULL, 0, 0, 0, NULL }
  };
  int i;

  config = calloc(1,sizeof(m_config_t));
  config->lvl = 1; // 0 Is the defaults
  if(!inited) {
    inited = 1;
    profile_opt_type = m_option_type_string_list;
    profile_opt_type.parse = parse_profile;
    profile_opt_type.set = set_profile;
  }
  config->self_opts = malloc(sizeof(ref_opts));
  memcpy(config->self_opts,ref_opts,sizeof(ref_opts));
  for(i = 0 ; config->self_opts[i].name ; i++)
    config->self_opts[i].priv = config;
  m_config_register_options(config,config->self_opts);
  
  return config;
}

void
m_config_free(m_config_t* config) {
  m_config_option_t *i = config->opts, *ct;
  m_config_save_slot_t *sl,*st;
  m_profile_t *p,*pn;
  int j;

#ifdef MP_DEBUG
  assert(config != NULL);
#endif
  
  while(i) {
    if (i->flags & M_CFG_OPT_ALIAS)
      sl = NULL;
    else
      sl = i->slots;
    while(sl) {
      m_option_free(i->opt,sl->data);
      st = sl->prev;
      free(sl);
      sl = st;
    }
    if(i->name != i->opt->name)
      free(i->name);
    ct = i->next;
    free(i);
    i = ct;
  }
  for(p = config->profiles ; p ; p = pn) {
    pn = p->next;
    free(p->name);
    if(p->desc) free(p->desc);
    for(j = 0 ; j < p->num_opts ; j++) {
      free(p->opts[2*j]);
      if(p->opts[2*j+1]) free(p->opts[2*j+1]);
    }
    free(p->opts);
    free(p);
  }
  free(config->self_opts);
  free(config);  
}

void
m_config_push(m_config_t* config) {
  m_config_option_t *co;
  m_config_save_slot_t *slot;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
#endif

  config->lvl++;

  for(co = config->opts ; co ; co = co->next ) {
    if(co->opt->type->flags & M_OPT_TYPE_HAS_CHILD)
      continue;
    if(co->opt->flags & (M_OPT_GLOBAL|M_OPT_NOSAVE))
      continue;
    if((co->opt->flags & M_OPT_OLD) && !(co->flags & M_CFG_OPT_SET))
      continue;
    if(co->flags & M_CFG_OPT_ALIAS)
      continue;

    // Update the current status
    m_option_save(co->opt,co->slots->data,co->opt->p);
    
    // Allocate a new slot    
    slot = calloc(1,sizeof(m_config_save_slot_t) + co->opt->type->size);
    slot->lvl = config->lvl;
    slot->prev = co->slots;
    co->slots = slot;
    m_option_copy(co->opt,co->slots->data,co->slots->prev->data);
    // Reset our set flag
    co->flags &= ~M_CFG_OPT_SET;
  }
  
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Config pushed level is now %d\n",config->lvl);
}

void
m_config_pop(m_config_t* config) {
  m_config_option_t *co;
  m_config_save_slot_t *slot;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 1);
#endif

  for(co = config->opts ; co ; co = co->next ) {
    int pop = 0;
    if(co->opt->type->flags & M_OPT_TYPE_HAS_CHILD)
      continue;
    if(co->opt->flags & (M_OPT_GLOBAL|M_OPT_NOSAVE))
      continue;
    if(co->flags & M_CFG_OPT_ALIAS)
      continue;
    if(co->slots->lvl > config->lvl)
      mp_msg(MSGT_CFGPARSER, MSGL_WARN,MSGTR_SaveSlotTooOld,config->lvl,co->slots->lvl);
    
    while(co->slots->lvl >= config->lvl) {
      m_option_free(co->opt,co->slots->data);
      slot = co->slots;
      co->slots = slot->prev;
      free(slot);
      pop++;
    }
    if(pop) // We removed some ctx -> set the previous value
      m_option_set(co->opt,co->opt->p,co->slots->data);
  }

  config->lvl--;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Config poped level=%d\n",config->lvl);
}

static void
m_config_add_option(m_config_t *config, m_option_t *arg, char* prefix) {
  m_config_option_t *co;
  m_config_save_slot_t* sl;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
  assert(arg != NULL);
#endif

  // Allocate a new entry for this option
  co = calloc(1,sizeof(m_config_option_t) + arg->type->size);
  co->opt = arg;

  // Fill in the full name
  if(prefix && strlen(prefix) > 0) {
    int l = strlen(prefix) + 1 + strlen(arg->name) + 1;
    co->name = (char*) malloc(l);
    sprintf(co->name,"%s:%s",prefix,arg->name);
  } else
    co->name = arg->name;

  // Option with childs -> add them
  if(arg->type->flags & M_OPT_TYPE_HAS_CHILD) {
    m_option_t *ol = arg->p;
    int i;
    co->slots = NULL;
    for(i = 0 ; ol[i].name != NULL ; i++)
      m_config_add_option(config,&ol[i], co->name);
  } else {
    m_config_option_t *i;
    // Check if there is alredy an option pointing to this address
    if(arg->p) {
      for(i = config->opts ; i ; i = i->next ) {
	if(i->opt->p == arg->p) { // So we don't save the same vars more than 1 time
	  co->slots = i->slots;
	  co->flags |= M_CFG_OPT_ALIAS;
	  break;
	}
      }
    }
    if(!(co->flags & M_CFG_OPT_ALIAS)) {
    // Allocate a slot for the defaults
    sl = calloc(1,sizeof(m_config_save_slot_t) + arg->type->size);
    m_option_save(arg,sl->data,(void**)arg->p);
    // Hack to avoid too much trouble with dynamicly allocated data :
    // We always use a dynamic version
    if((arg->type->flags & M_OPT_TYPE_DYNAMIC) && arg->p && (*(void**)arg->p)) {
      *(void**)arg->p = NULL;
      m_option_set(arg,arg->p,sl->data);
    }
    sl->lvl = 0;
    sl->prev = NULL;
    co->slots = calloc(1,sizeof(m_config_save_slot_t) + arg->type->size);
    co->slots->prev = sl;
    co->slots->lvl = config->lvl;
    m_option_copy(co->opt,co->slots->data,sl->data);
    } // !M_OPT_ALIAS
  }
  co->next = config->opts;
  config->opts = co;
}

int
m_config_register_options(m_config_t *config, m_option_t *args) {
  int i;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
  assert(args != NULL);
#endif

  for(i = 0 ; args[i].name != NULL ; i++)
    m_config_add_option(config,&args[i],NULL);

  return 1;
}

static m_config_option_t* 
m_config_get_co(m_config_t *config, char* arg) {
  m_config_option_t *co;

  for(co = config->opts ; co ; co = co->next ) {
    int l = strlen(co->name) - 1;
    if((co->opt->type->flags & M_OPT_TYPE_ALLOW_WILDCARD) && 
       (co->name[l] == '*')) {
      if(strncasecmp(co->name,arg,l) == 0)
	return co;
    } else if(strcasecmp(co->name,arg) == 0)
      return co;
  }
  return NULL;
}

static int
m_config_parse_option(m_config_t *config, char* arg, char* param,int set) {
  m_config_option_t *co;
  int r = 0;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
  assert(arg != NULL);
#endif

  co = m_config_get_co(config,arg);
  if(!co){
//    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Unknown option: %s\n",arg);
    return M_OPT_UNKNOWN;
  }

#ifdef MP_DEBUG
  // This is the only mandatory function
  assert(co->opt->type->parse);
#endif

  // Check if this option isn't forbiden in the current mode
  if((config->mode == M_CONFIG_FILE) && (co->opt->flags & M_OPT_NOCFG)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,MSGTR_InvalidCfgfileOption,arg);
    return M_OPT_INVALID;
  }
  if((config->mode == M_COMMAND_LINE) && (co->opt->flags & M_OPT_NOCMD)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,MSGTR_InvalidCmdlineOption,arg);
    return M_OPT_INVALID;
  }

  // Option with childs are a bit different to parse
  if(co->opt->type->flags & M_OPT_TYPE_HAS_CHILD) {
    char** lst = NULL;
    int i,sr;
    // Parse the child options
    r = m_option_parse(co->opt,arg,param,&lst,M_COMMAND_LINE);
    // Set them now
    if(r >= 0)
    for(i = 0 ; lst && lst[2*i] ; i++) {
      int l = strlen(co->name) + 1 + strlen(lst[2*i]) + 1;
      if(r >= 0) {
	// Build the full name
	char n[l];
	sprintf(n,"%s:%s",co->name,lst[2*i]);
	sr = m_config_parse_option(config,n,lst[2*i+1],set);
	if(sr < 0){
	  if(sr == M_OPT_UNKNOWN){
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR,MSGTR_InvalidSuboption,co->name,lst[2*i]);
	    r = M_OPT_INVALID;
	  } else
	  if(sr == M_OPT_MISSING_PARAM){
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR,MSGTR_MissingSuboptionParameter,lst[2*i],co->name);
	    r = M_OPT_INVALID;
	  } else
	    r = sr;
	}
      }
      free(lst[2*i]);
      free(lst[2*i+1]);
    }
    if(lst) free(lst);      
  } else
    r = m_option_parse(co->opt,arg,param,set ? co->slots->data : NULL,config->mode);

  // Parsing failed ?
  if(r < 0)
    return r;
  // Set the option
  if(set) {
    m_option_set(co->opt,co->opt->p,co->slots->data);
    co->flags |= M_CFG_OPT_SET;
  }

  return r;
}

int
m_config_set_option(m_config_t *config, char* arg, char* param) {
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Setting %s=%s\n",arg,param);
  return m_config_parse_option(config,arg,param,1);
}

int
m_config_check_option(m_config_t *config, char* arg, char* param) {
  int r;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Checking %s=%s\n",arg,param);
  r=m_config_parse_option(config,arg,param,0);
  if(r==M_OPT_MISSING_PARAM){
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,MSGTR_MissingOptionParameter,arg);
    return M_OPT_INVALID;
  }
  return r;
}


m_option_t*
m_config_get_option(m_config_t *config, char* arg) {
  m_config_option_t *co;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
  assert(arg != NULL);
#endif

  co = m_config_get_co(config,arg);
  if(co)
    return co->opt;
  else
    return NULL;
}

void*
m_config_get_option_ptr(m_config_t *config, char* arg) {
  m_option_t* conf;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(arg != NULL);
#endif

  conf = m_config_get_option(config,arg);
  if(!conf) return NULL;
  return conf->p;
}

void
m_config_print_option_list(m_config_t *config) {
  char min[50],max[50];
  m_config_option_t* co;
  int count = 0;

  if(!config->opts) return;

  mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_OptionListHeader);
  for(co = config->opts ; co ; co = co->next) {
    m_option_t* opt = co->opt;
    if(opt->type->flags & M_OPT_TYPE_HAS_CHILD) continue;
    if(opt->flags & M_OPT_MIN)
      sprintf(min,"%-8.0f",opt->min);
    else
      strcpy(min,"No");
    if(opt->flags & M_OPT_MAX)
      sprintf(max,"%-8.0f",opt->max);
    else
      strcpy(max,"No");
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %-20.20s %-15.15s %-10.10s %-10.10s %-3.3s   %-3.3s   %-3.3s\n",
	   co->name,
	   co->opt->type->name,
	   min,
	   max,
	   opt->flags & CONF_GLOBAL ? "Yes" : "No",
	   opt->flags & CONF_NOCMD ? "No" : "Yes",
	   opt->flags & CONF_NOCFG ? "No" : "Yes");
    count++;
  }
  mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_TotalOptions,count);
}

m_profile_t*
m_config_get_profile(m_config_t* config, char* name) {
  m_profile_t* p;
  for(p = config->profiles ; p ; p = p->next)
    if(!strcmp(p->name,name)) return p;
  return NULL;
}

m_profile_t*
m_config_add_profile(m_config_t* config, char* name) {
  m_profile_t* p = m_config_get_profile(config,name);
  if(p) return p;
  p = calloc(1,sizeof(m_profile_t));
  p->name = strdup(name);
  p->next = config->profiles;
  config->profiles = p;
  return p;
}

void
m_profile_set_desc(m_profile_t* p, char* desc) {
  if(p->desc) free(p->desc);
  p->desc = desc ? strdup(desc) : NULL;
}

int
m_config_set_profile_option(m_config_t* config, m_profile_t* p,
			    char* name, char* val) {
  int i = m_config_check_option(config,name,val);
  if(i < 0) return i;
  if(p->opts) p->opts = realloc(p->opts,2*(p->num_opts+2)*sizeof(char*));
  else p->opts = malloc(2*(p->num_opts+2)*sizeof(char*));
  p->opts[p->num_opts*2] = strdup(name);
  p->opts[p->num_opts*2+1] = val ? strdup(val) : NULL;
  p->num_opts++;
  p->opts[p->num_opts*2] = p->opts[p->num_opts*2+1] = NULL;
  return 1;
}

static void
m_config_set_profile(m_config_t* config, m_profile_t* p) {
  int i;
  if(config->profile_depth > MAX_PROFILE_DEPTH) {
    mp_msg(MSGT_CFGPARSER, MSGL_WARN, MSGTR_ProfileInclusionTooDeep);
    return;
  }
  config->profile_depth++;
  for(i = 0 ; i < p->num_opts ; i++)
    m_config_set_option(config,p->opts[2*i],p->opts[2*i+1]);
  config->profile_depth--;
}

static int
parse_profile(m_option_t* opt,char *name, char *param, void* dst, int src) {
  m_config_t* config = opt->priv;
  char** list = NULL;
  int i,r;
  if(param && !strcmp(param,"help")) {
    m_profile_t* p;
    if(!config->profiles) {
      mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_NoProfileDefined);
      return M_OPT_EXIT-1;
    }
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_AvailableProfiles);
    for(p = config->profiles ; p ; p = p->next)
      mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\t%s\t%s\n",p->name,
	     p->desc ? p->desc : "");
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
    return M_OPT_EXIT-1;
  }
    
  r = m_option_type_string_list.parse(opt,name,param,&list,src);
  if(r < 0) return r;
  if(!list || !list[0]) return M_OPT_INVALID;
  for(i = 0 ; list[i] ; i++)
    if(!m_config_get_profile(config,list[i])) {
      mp_msg(MSGT_CFGPARSER, MSGL_WARN, MSGTR_UnknownProfile,
             list[i]);
      r = M_OPT_INVALID;
    }
  if(dst)
    m_option_copy(opt,dst,&list);
  else
    m_option_free(opt,&list);
  return r;
}

static void
set_profile(m_option_t *opt, void* dst, void* src) {
  m_config_t* config = opt->priv;
  m_profile_t* p;
  char** list = NULL;
  int i;
  if(!src || !*(char***)src) return;
  m_option_copy(opt,&list,src);
  for(i = 0 ; list[i] ; i++) {
    p = m_config_get_profile(config,list[i]);
    if(!p) continue;
    m_config_set_profile(config,p);
  }
  m_option_free(opt,&list);
}

static int
show_profile(m_option_t *opt, char* name, char *param) {
  m_config_t* config = opt->priv;
  m_profile_t* p;
  int i,j;
  if(!param) return M_OPT_MISSING_PARAM;
  if(!(p = m_config_get_profile(config,param))) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, MSGTR_UnknownProfile, param);
    return M_OPT_EXIT-1;
  }
  if(!config->profile_depth)
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_Profile, param,
	   p->desc ? p->desc : "");
  config->profile_depth++;
  for(i = 0 ; i < p->num_opts ; i++) {
    char spc[config->profile_depth+1];
    for(j = 0 ; j < config->profile_depth ; j++)
      spc[j] = ' ';
    spc[config->profile_depth] = '\0';
    
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s%s=%s\n", spc,
	   p->opts[2*i], p->opts[2*i+1]);
    

    if(config->profile_depth < MAX_PROFILE_DEPTH &&
       !strcmp(p->opts[2*i],"profile")) {
      char* e,*list = p->opts[2*i+1];
      while((e = strchr(list,','))) {
	int l = e-list;
	char tmp[l+1];
	if(!l) continue;
	memcpy(tmp,list,l);
	tmp[l] = '\0';
	show_profile(opt,name,tmp);
	list = e+1;
      }
      if(list[0] != '\0')
	show_profile(opt,name,list);
    }
  }
  config->profile_depth--;
  if(!config->profile_depth) mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
  return M_OPT_EXIT-1;
}

static int
list_options(m_option_t *opt, char* name, char *param) {
  m_config_t* config = opt->priv;
  m_config_print_option_list(config);
  return M_OPT_EXIT;
}
