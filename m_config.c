
#include "config.h"

#ifdef NEW_CONFIG

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

m_config_t*
m_config_new(void) {
  m_config_t* config;

  config = (m_config_t*)calloc(1,sizeof(m_config_t));
  config->lvl = 1; // 0 Is the defaults
  return config;
}

void
m_config_free(m_config_t* config) {
  m_config_option_t *i = config->opts, *ct;
  m_config_save_slot_t *sl,*st;

#ifdef MP_DEBUG
  assert(config != NULL);
#endif
  
  while(i) {
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
    ct = i;
  }
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
    if((co->opt->flags & M_OPT_OLD) && !co->flags)
      continue;

    // Update the current status
    m_option_save(co->opt,co->slots->data,co->opt->p);
    
    // Allocate a new slot    
    slot = (m_config_save_slot_t*)calloc(1,sizeof(m_config_save_slot_t) + co->opt->type->size);
    slot->lvl = config->lvl;
    slot->prev = co->slots;
    co->slots = slot;
    m_option_copy(co->opt,co->slots->data,co->slots->prev->data);
    // Reset our flags
    co->flags=0;
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
    if(co->slots->lvl > config->lvl)
      mp_msg(MSGT_CFGPARSER, MSGL_WARN,"Too old save slot found from lvl %d : %d !!!\n",config->lvl,co->slots->lvl);
    
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
  co = (m_config_option_t*)calloc(1,sizeof(m_config_option_t) + arg->type->size);
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
    for(i = 0 ; ol[i].name != NULL ; i++)
      m_config_add_option(config,&ol[i], co->name);
  } else {
    // Allocate a slot for the defaults
    sl = (m_config_save_slot_t*)calloc(1,sizeof(m_config_save_slot_t) + arg->type->size);
    m_option_save(arg,sl->data,(void**)arg->p);
    // Hack to avoid too much trouble with dynamicly allocated data :
    // We always use a dynamic version
    if((arg->type->flags & M_OPT_TYPE_DYNAMIC) && arg->p && (*(void**)arg->p)) {
      *(void**)arg->p = NULL;
      m_option_set(arg,arg->p,sl->data);
    }
    sl->lvl = 0;
    co->slots = (m_config_save_slot_t*)calloc(1,sizeof(m_config_save_slot_t) + arg->type->size);
    co->slots->prev = sl;
    co->slots->lvl = config->lvl;
    m_option_copy(co->opt,co->slots->data,sl->data);
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
    return M_OPT_UNKNOW;
  }

#ifdef MP_DEBUG
  // This is the only mandatory function
  assert(co->opt->type->parse);
#endif

  // Check if this option isn't forbiden in the current mode
  if((config->mode == M_CONFIG_FILE) && (co->opt->flags & M_OPT_NOCFG)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"The %s option can't be used in a config file\n",arg);
    return M_OPT_INVALID;
  }
  if((config->mode == M_COMMAND_LINE) && (co->opt->flags & M_OPT_NOCMD)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"The %s option can't be used on the command line\n",arg);
    return M_OPT_INVALID;
  }

  // Option with childs are a bit different to parse
  if(co->opt->type->flags & M_OPT_TYPE_HAS_CHILD) {
    char** lst = NULL;
    int i,sr;
    // Parse the child options
    r = m_option_parse(co->opt,arg,param,&lst,config->mode);
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
	  if(sr == M_OPT_UNKNOW){
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Error: option '%s' has no suboption '%s'\n",co->name,lst[2*i]);
	    r = M_OPT_INVALID;
	  } else
	  if(sr == M_OPT_MISSING_PARAM){
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Error: suboption '%s' of '%s' must have a parameter!\n",lst[2*i],co->name);
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
    co->flags = 1;
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
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Error: option '%s' must have a parameter!\n",arg);
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

  printf("\n Name                 Type            Min        Max      Global  CL    Cfg\n\n");
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
    printf(" %-20.20s %-15.15s %-10.10s %-10.10s %-3.3s   %-3.3s   %-3.3s\n",
	   co->name,
	   co->opt->type->name,
	   min,
	   max,
	   opt->flags & CONF_GLOBAL ? "Yes" : "No",
	   opt->flags & CONF_NOCMD ? "No" : "Yes",
	   opt->flags & CONF_NOCFG ? "No" : "Yes");
    count++;
  }
  printf("\nTotal: %d options\n",count);
}

#endif // NEW_CONFIG
