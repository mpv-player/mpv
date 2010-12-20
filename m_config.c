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
/// \ingroup Config

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "talloc.h"
#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "m_config.h"
#include "m_option.h"
#include "mp_msg.h"

#define MAX_PROFILE_DEPTH 20

static int parse_profile(const m_option_t *opt, const char *name,
                         const char *param, void *dst, int src)
{
    m_config_t *config = opt->priv;
    char **list = NULL;
    int i, r;
    if (param && !strcmp(param, "help")) {
        m_profile_t *p;
        if (!config->profiles) {
            mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "No profiles have been defined.\n");
            return M_OPT_EXIT-1;
        }
        mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "Available profiles:\n");
        for (p = config->profiles; p; p = p->next)
            mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\t%s\t%s\n", p->name,
                   p->desc ? p->desc : "");
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
        return M_OPT_EXIT-1;
    }

    r = m_option_type_string_list.parse(opt, name, param, &list, src);
    if (r < 0)
        return r;
    if (!list || !list[0])
        return M_OPT_INVALID;
    for (i = 0; list[i]; i++)
        if (!m_config_get_profile(config,list[i])) {
            mp_tmsg(MSGT_CFGPARSER, MSGL_WARN, "Unknown profile '%s'.\n",
                    list[i]);
            r = M_OPT_INVALID;
        }
    if (dst)
        m_option_copy(opt, dst, &list);
    else
        m_option_free(opt, &list);
    return r;
}

static void set_profile(const m_option_t *opt, void *dst, const void *src)
{
    m_config_t *config = opt->priv;
    m_profile_t *p;
    char **list = NULL;
    int i;
    if (!src || !*(char***)src)
        return;
    m_option_copy(opt, &list, src);
    for (i = 0; list[i]; i++) {
        p = m_config_get_profile(config, list[i]);
        if (!p)
            continue;
        m_config_set_profile(config, p);
    }
    m_option_free(opt, &list);
}

static int show_profile(m_option_t *opt, char* name, char *param)
{
    m_config_t *config = opt->priv;
    m_profile_t *p;
    int i, j;
    if (!param)
        return M_OPT_MISSING_PARAM;
    if (!(p = m_config_get_profile(config, param))) {
        mp_tmsg(MSGT_CFGPARSER, MSGL_ERR, "Unknown profile '%s'.\n", param);
        return M_OPT_EXIT - 1;
    }
    if (!config->profile_depth)
        mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "Profile %s: %s\n", param,
                p->desc ? p->desc : "");
    config->profile_depth++;
    for (i = 0; i < p->num_opts; i++) {
        char spc[config->profile_depth + 1];
        for (j = 0; j < config->profile_depth; j++)
            spc[j] = ' ';
        spc[config->profile_depth] = '\0';

        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s%s=%s\n", spc,
               p->opts[2 * i], p->opts[2 * i + 1]);

        if (config->profile_depth < MAX_PROFILE_DEPTH
            && !strcmp(p->opts[2*i], "profile")) {
            char *e, *list = p->opts[2 * i + 1];
            while ((e = strchr(list, ','))) {
                int l = e-list;
                char tmp[l+1];
                if (!l)
                    continue;
                memcpy(tmp, list, l);
                tmp[l] = '\0';
                show_profile(opt, name, tmp);
                list = e+1;
            }
            if (list[0] != '\0')
                show_profile(opt, name, list);
        }
    }
    config->profile_depth--;
    if (!config->profile_depth)
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, "\n");
    return M_OPT_EXIT - 1;
}

static int list_options(m_option_t *opt, char *name, char *param)
{
    m_config_t *config = opt->priv;
    m_config_print_option_list(config);
    return M_OPT_EXIT;
}

static void m_option_save(const m_config_t *config, const m_option_t *opt,
                          void *dst)
{
    if (opt->type->save) {
        const void *src = m_option_get_ptr(opt, config->optstruct);
        opt->type->save(opt, dst, src);
    }
}

static void m_option_set(const m_config_t *config, const m_option_t *opt,
			 const void *src)
{
    if (opt->type->set) {
        void *dst = m_option_get_ptr(opt, config->optstruct);
        opt->type->set(opt, dst, src);
    }
}



static void
m_config_add_option(m_config_t *config, const m_option_t *arg, const char* prefix);

m_config_t *m_config_new(void *optstruct,
                         int includefunc(m_option_t *conf, char *filename))
{
  m_config_t* config;
  static int initialized = 0;
  static m_option_type_t profile_opt_type;
  static const m_option_t ref_opts[] = {
    { "profile", NULL, &profile_opt_type, CONF_NOSAVE, 0, 0, NULL },
    { "show-profile", show_profile, CONF_TYPE_PRINT_FUNC, CONF_NOCFG, 0, 0, NULL },
    { "list-options", list_options, CONF_TYPE_PRINT_FUNC, CONF_NOCFG, 0, 0, NULL },
    { NULL, NULL, NULL, 0, 0, 0, NULL }
  };
  int i;

  config = talloc_zero(NULL, m_config_t);
  config->lvl = 1; // 0 Is the defaults
  if(!initialized) {
    initialized = 1;
    profile_opt_type = m_option_type_string_list;
    profile_opt_type.parse = parse_profile;
    profile_opt_type.set = set_profile;
  }
  m_option_t *self_opts = talloc_memdup(config, ref_opts, sizeof(ref_opts));
  for (i = 0; self_opts[i].name; i++)
      self_opts[i].priv = config;
  m_config_register_options(config, self_opts);
  if (includefunc) {
      struct m_option *p = talloc_ptrtype(config, p);
      *p = (struct m_option){"include", includefunc, CONF_TYPE_FUNC_PARAM,
                             CONF_NOSAVE, 0, 0, config};
      m_config_add_option(config, p, NULL);
  }
  config->optstruct = optstruct;

  return config;
}

void m_config_free(m_config_t* config)
{
    m_config_option_t *opt;
    for (opt = config->opts; opt; opt = opt->next) {
        if (opt->flags & M_CFG_OPT_ALIAS)
            continue;
        m_config_save_slot_t *sl;
        for (sl = opt->slots; sl; sl = sl->prev)
            m_option_free(opt->opt, sl->data);
    }
    talloc_free(config);
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
    m_option_save(config, co->opt, co->slots->data);

    // Allocate a new slot
    slot = talloc_zero_size(co, sizeof(m_config_save_slot_t) +
                                co->opt->type->size);
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
      mp_tmsg(MSGT_CFGPARSER, MSGL_WARN,"Save slot found from lvl %d is too old: %d !!!\n",config->lvl,co->slots->lvl);

    while(co->slots->lvl >= config->lvl) {
      m_option_free(co->opt,co->slots->data);
      slot = co->slots;
      co->slots = slot->prev;
      talloc_free(slot);
      pop++;
    }
    if(pop) // We removed some ctx -> set the previous value
      m_option_set(config, co->opt, co->slots->data);
  }

  config->lvl--;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Config poped level=%d\n",config->lvl);
}

static void
m_config_add_option(m_config_t *config, const m_option_t *arg, const char* prefix) {
  m_config_option_t *co;
  m_config_save_slot_t* sl;

#ifdef MP_DEBUG
  assert(config != NULL);
  assert(config->lvl > 0);
  assert(arg != NULL);
#endif

  // Allocate a new entry for this option
  co = talloc_zero_size(config, sizeof(m_config_option_t) + arg->type->size);
  co->opt = arg;

  // Fill in the full name
  if(prefix && strlen(prefix) > 0) {
      co->name = talloc_asprintf(co, "%s:%s", prefix, arg->name);
  } else
    co->name = arg->name;

  // Option with children -> add them
  if(arg->type->flags & M_OPT_TYPE_HAS_CHILD) {
    const m_option_t *ol = arg->p;
    int i;
    co->slots = NULL;
    for(i = 0 ; ol[i].name != NULL ; i++)
      m_config_add_option(config,&ol[i], co->name);
  } else {
    m_config_option_t *i;
    // Check if there is already an option pointing to this address
    if(arg->p || arg->new && arg->offset >= 0) {
      for(i = config->opts ; i ; i = i->next ) {
        if (arg->new ? (i->opt->new && i->opt->offset == arg->offset)
            : (!i->opt->new && i->opt->p == arg->p)) {
          // So we don't save the same vars more than 1 time
	  co->slots = i->slots;
	  co->flags |= M_CFG_OPT_ALIAS;
	  break;
	}
      }
    }
    if(!(co->flags & M_CFG_OPT_ALIAS)) {
        // Allocate a slot for the defaults
        sl = talloc_zero_size(co, sizeof(m_config_save_slot_t) +
                              arg->type->size);
        m_option_save(config, arg, sl->data);
        // Hack to avoid too much trouble with dynamically allocated data :
        // We always use a dynamic version
        if ((arg->type->flags & M_OPT_TYPE_DYNAMIC)) {
            char **hackptr = arg->new ? (char*)config->optstruct + arg->offset
                                      : arg->p;
            if (hackptr && *hackptr) {
                *hackptr = NULL;
                m_option_set(config, arg, sl->data);
            }
        }
        sl->lvl = 0;
        sl->prev = NULL;
        co->slots = talloc_zero_size(co, sizeof(m_config_save_slot_t) +
                                     arg->type->size);
        co->slots->prev = sl;
        co->slots->lvl = config->lvl;
        m_option_copy(co->opt, co->slots->data, sl->data);
    }
  }
  co->next = config->opts;
  config->opts = co;
}

int
m_config_register_options(m_config_t *config, const m_option_t *args) {
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
m_config_get_co(const m_config_t *config, char *arg) {
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
m_config_parse_option(const m_config_t *config, char *arg, char *param, int set) {
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

  // Check if this option isn't forbidden in the current mode
  if((config->mode == M_CONFIG_FILE) && (co->opt->flags & M_OPT_NOCFG)) {
    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,"The %s option can't be used in a config file.\n",arg);
    return M_OPT_INVALID;
  }
  if((config->mode == M_COMMAND_LINE) && (co->opt->flags & M_OPT_NOCMD)) {
    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,"The %s option can't be used on the command line.\n",arg);
    return M_OPT_INVALID;
  }
  // During command line preparse set only pre-parse options
  // Otherwise only set pre-parse option if they were not already set.
  if(((config->mode == M_COMMAND_LINE_PRE_PARSE) &&
      !(co->opt->flags & M_OPT_PRE_PARSE)) ||
     ((config->mode != M_COMMAND_LINE_PRE_PARSE) &&
      (co->opt->flags & M_OPT_PRE_PARSE) && (co->flags & M_CFG_OPT_SET)))
    set = 0;

  // Option with children are a bit different to parse
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
	    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,"Error: option '%s' has no suboption '%s'.\n",co->name,lst[2*i]);
	    r = M_OPT_INVALID;
	  } else
	  if(sr == M_OPT_MISSING_PARAM){
	    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,"Error: suboption '%s' of '%s' must have a parameter!\n",lst[2*i],co->name);
	    r = M_OPT_INVALID;
	  } else
	    r = sr;
	}
      }
      free(lst[2*i]);
      free(lst[2*i+1]);
    }
    free(lst);
  } else
    r = m_option_parse(co->opt,arg,param,set ? co->slots->data : NULL,config->mode);

  // Parsing failed ?
  if(r < 0)
    return r;
  // Set the option
  if(set) {
    m_option_set(config, co->opt, co->slots->data);
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
m_config_check_option(const m_config_t *config, char *arg, char *param) {
  int r;
  mp_msg(MSGT_CFGPARSER, MSGL_DBG2,"Checking %s=%s\n",arg,param);
  r=m_config_parse_option(config,arg,param,0);
  if(r==M_OPT_MISSING_PARAM){
    mp_tmsg(MSGT_CFGPARSER, MSGL_ERR,"Error: option '%s' must have a parameter!\n",arg);
    return M_OPT_INVALID;
  }
  return r;
}


const m_option_t*
m_config_get_option(const m_config_t *config, char *arg) {
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

void
m_config_print_option_list(const m_config_t *config) {
  char min[50],max[50];
  m_config_option_t* co;
  int count = 0;

  if(!config->opts) return;

  mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "\n Name                 Type            Min        Max      Global  CL    Cfg\n\n");
  for(co = config->opts ; co ; co = co->next) {
    const m_option_t* opt = co->opt;
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
  mp_tmsg(MSGT_CFGPARSER, MSGL_INFO, "\nTotal: %d options\n",count);
}

m_profile_t*
m_config_get_profile(const m_config_t *config, char *name) {
  m_profile_t* p;
  for(p = config->profiles ; p ; p = p->next)
    if(!strcmp(p->name,name)) return p;
  return NULL;
}

m_profile_t*
m_config_add_profile(m_config_t* config, char* name) {
  m_profile_t* p = m_config_get_profile(config,name);
  if(p) return p;
  p = talloc_zero(config, m_profile_t);
  p->name = talloc_strdup(p, name);
  p->next = config->profiles;
  config->profiles = p;
  return p;
}

void
m_profile_set_desc(m_profile_t* p, char* desc) {
    talloc_free(p->desc);
    p->desc = talloc_strdup(p, desc);
}

int
m_config_set_profile_option(m_config_t* config, m_profile_t* p,
			    char* name, char* val) {
  int i = m_config_check_option(config,name,val);
  if(i < 0) return i;
  p->opts = talloc_realloc(p, p->opts, char *, 2*(p->num_opts+2));
  p->opts[p->num_opts*2] = talloc_strdup(p, name);
  p->opts[p->num_opts*2+1] = talloc_strdup(p, val);
  p->num_opts++;
  p->opts[p->num_opts*2] = p->opts[p->num_opts*2+1] = NULL;
  return 1;
}

void
m_config_set_profile(m_config_t* config, m_profile_t* p) {
  int i;
  if(config->profile_depth > MAX_PROFILE_DEPTH) {
    mp_tmsg(MSGT_CFGPARSER, MSGL_WARN, "WARNING: Profile inclusion too deep.\n");
    return;
  }
  int prev_mode = config->mode;
  config->mode = M_CONFIG_FILE;
  config->profile_depth++;
  for(i = 0 ; i < p->num_opts ; i++)
    m_config_set_option(config,p->opts[2*i],p->opts[2*i+1]);
  config->profile_depth--;
  config->mode = prev_mode;
}
