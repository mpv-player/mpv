
#include "config.h"

#ifdef NEW_CONFIG

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>

#include "m_option.h"
//#include "m_config.h"
#include "mp_msg.h"

// Don't free for 'production' atm
#ifndef MP_DEBUG
#define NO_FREE
#endif

m_option_t* m_option_list_find(m_option_t* list,char* name) {
  int i;

  for(i = 0 ; list[i].name ; i++) {
    int l = strlen(list[i].name) - 1;
    if((list[i].type->flags & M_OPT_TYPE_ALLOW_WILDCARD) &&
       (l > 0) && (list[i].name[l] == '*')) {
      if(strncasecmp(list[i].name,name,l) == 0)
	return &list[i];
    } else if(strcasecmp(list[i].name,name) == 0)
      return &list[i];
  }
  return NULL;
}

// Default function that just do a memcpy

static void copy_opt(m_option_t* opt,void* dst,void* src) {
  if(dst && src)
    memcpy(dst,src,opt->type->size);
}

// Helper for the print funcs (from man printf)
static char* dup_printf(const char *fmt, ...) {       
  /* Guess we need no more than 50 bytes. */
  int n, size = 50;
  char *p;
  va_list ap;
  if ((p = malloc (size)) == NULL)
    return NULL;
  while (1) {
    /* Try to print in the allocated space. */
    va_start(ap, fmt);
    n = vsnprintf (p, size, fmt, ap);
    va_end(ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size)      
      return p;
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
      size = n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ((p = realloc (p, size)) == NULL)
      return NULL;
  }
}


// Flag

#define VAL(x) (*(int*)(x))

static int parse_flag(m_option_t* opt,char *name, char *param, void* dst, int src) {
  if (src == M_CONFIG_FILE) {
    if (!strcasecmp(param, "yes") ||	/* any other language? */
	!strcasecmp(param, "ja") ||
	!strcasecmp(param, "si") ||
	!strcasecmp(param, "igen") ||
	!strcasecmp(param, "y") ||
	!strcasecmp(param, "j") ||
	!strcasecmp(param, "i") ||
	!strcmp(param, "1")) {
      if(dst) VAL(dst) = opt->max;
    } else if (!strcasecmp(param, "no") ||
	       !strcasecmp(param, "nein") ||
	       !strcasecmp(param, "nicht") ||
	       !strcasecmp(param, "nem") ||
	       !strcasecmp(param, "n") ||
	       !strcmp(param, "0")) {
      if(dst) VAL(dst) = opt->min;
    } else {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "invalid parameter for %s flag: %s\n",name, param);
      return M_OPT_INVALID;
    }
    return 1;
  } else {
    if(dst) VAL(dst) = opt->max;
    return 0;
  }
}

static char* print_flag(m_option_t* opt,  void* val) {
  if(VAL(val) == opt->min)
    return strdup("no");
  else
    return strdup("yes");
}

m_option_type_t m_option_type_flag = {
  "Flag",
  "need yes or no in config files",
  sizeof(int),
  0,
  parse_flag,
  print_flag,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};

// Integer

static int parse_int(m_option_t* opt,char *name, char *param, void* dst, int src) {
  long tmp_int;
  char *endptr;
  src = 0;

  if (param == NULL)
    return M_OPT_MISSING_PARAM;

  tmp_int = strtol(param, &endptr, 0);
  if (*endptr) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be an integer: %s\n",name, param);
    return M_OPT_INVALID;
  }

  if ((opt->flags & M_OPT_MIN) && (tmp_int < opt->min)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be >= %d: %s\n", name, (int) opt->min, param);
    return M_OPT_OUT_OF_RANGE;
  }

  if ((opt->flags & M_OPT_MAX) && (tmp_int > opt->max)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be <= %d: %s\n",name, (int) opt->max, param);
    return M_OPT_OUT_OF_RANGE;
  }

  if(dst) VAL(dst) = tmp_int;

  return 1;
}

static char* print_int(m_option_t* opt,  void* val) {
  opt = NULL;
  return dup_printf("%d",VAL(val));
}

m_option_type_t m_option_type_int = {
  "Integer",
  "",
  sizeof(int),
  0,
  parse_int,
  print_int,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};

// Float

#undef VAL
#define VAL(x) (*(float*)(x))

static int parse_float(m_option_t* opt,char *name, char *param, void* dst, int src) {
  float tmp_float;
  char* endptr;
  src = 0;

  if (param == NULL)
    return M_OPT_MISSING_PARAM;

  tmp_float = strtod(param, &endptr);

  switch(*endptr) {
  case ':':
  case '/':
    tmp_float /= strtod(endptr+1, &endptr);
    break;
  case '.':
  case ',':
    /* we also handle floats specified with
     * non-locale decimal point ::atmos
     */
    if(tmp_float<0)
      tmp_float -= 1.0/pow(10,strlen(endptr+1)) * strtod(endptr+1, &endptr);
    else
      tmp_float += 1.0/pow(10,strlen(endptr+1)) * strtod(endptr+1, &endptr);
    break;
  }

  if (*endptr) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be a floating point "
	   "number or a ratio (numerator[:/]denominator): %s\n",name, param);
    return M_OPT_INVALID;
  }

  if (opt->flags & M_OPT_MIN)
    if (tmp_float < opt->min) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be >= %f: %s\n", name, opt->min, param);
      return M_OPT_OUT_OF_RANGE;
    }

  if (opt->flags & M_OPT_MAX)
    if (tmp_float > opt->max) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be <= %f: %s\n", name, opt->max, param);
      return M_OPT_OUT_OF_RANGE;
    }

  if(dst) VAL(dst) = tmp_float;
  return 1;
}

static char* print_float(m_option_t* opt,  void* val) {
  opt = NULL;
  return dup_printf("%f",VAL(val));
}

m_option_type_t m_option_type_float = {
  "Float",
  "floating point number or ratio (numerator[:/]denominator)",
  sizeof(float),
  0,
  parse_float,
  print_float,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};

///////////// Position
#undef VAL
#define VAL(x) (*(off_t*)(x))

static int parse_position(m_option_t* opt,char *name, char *param, void* dst, int src) {
  off_t tmp_off;
  char dummy;

  if (param == NULL)
    return M_OPT_MISSING_PARAM;
  if (sscanf(param, sizeof(off_t) == sizeof(int) ?
	     "%d%c" : "%lld%c", &tmp_off, &dummy) != 1) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "The %s option must be an integer: %s\n",opt->name,param);
    return M_OPT_INVALID;
  }

  if (opt->flags & M_OPT_MIN)
    if (tmp_off < opt->min) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR,
	     (sizeof(off_t) == sizeof(int) ?
	      "The %s option must be >= %d: %s\n" :
	      "The %s option must be >= %lld: %s\n"),
	     (off_t) opt->min, param);
      return M_OPT_OUT_OF_RANGE;
    }

  if (opt->flags & M_OPT_MAX)
    if (tmp_off > opt->max) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR,
	     (sizeof(off_t) == sizeof(int) ?
	      "The %s option must be <= %d: %s\n" :
	      "The %s option must be <= %lld: %s\n"),
	     (off_t) opt->max, param);
      return M_OPT_OUT_OF_RANGE;
    }

  if(dst)
    VAL(dst) = tmp_off;
  return 1;
}

static char* print_position(m_option_t* opt,  void* val) {
  return dup_printf(sizeof(off_t) == sizeof(int) ?  "%d" : "%lld",VAL(val));
}

m_option_type_t m_option_type_position = {
  "Position",
  "Integer (off_t)",
  sizeof(off_t),
  0,
  parse_position,
  print_position,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};


///////////// String

#undef VAL
#define VAL(x) (*(char**)(x))

static int parse_str(m_option_t* opt,char *name, char *param, void* dst, int src) {
  

  if (param == NULL)
      return M_OPT_MISSING_PARAM;

  if ((opt->flags & M_OPT_MIN) && (strlen(param) < opt->min)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be >= %d chars: %s\n",
	   (int) opt->min, param);
    return M_OPT_OUT_OF_RANGE;
  }

  if ((opt->flags & M_OPT_MAX) && (strlen(param) > opt->max)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "parameter must be <= %d chars: %s\n",
	   (int) opt->max, param);
    return M_OPT_OUT_OF_RANGE;
  }

  if(dst) {
    if(VAL(dst))
      free(VAL(dst));
    VAL(dst) = strdup(param);
  }

  return 1;

}

static char* print_str(m_option_t* opt,  void* val) {
  return (val && VAL(val) && strlen(VAL(val)) > 0) ? strdup(VAL(val)) : NULL;
}

static void copy_str(m_option_t* opt,void* dst, void* src) {
  if(dst && src) {
#ifndef NO_FREE
    if(VAL(dst)) free(VAL(dst)); //FIXME!!!
#endif
    VAL(dst) = VAL(src) ? strdup(VAL(src)) : NULL;
  }
}
  
static void free_str(void* src) {
  if(src && VAL(src)){
#ifndef NO_FREE
    free(VAL(src)); //FIXME!!!
#endif
    VAL(src) = NULL;
  }
}

m_option_type_t m_option_type_string = {
  "String",
  "",
  sizeof(char*),
  M_OPT_TYPE_DYNAMIC,
  parse_str,
  print_str,
  copy_str,
  copy_str,
  copy_str,
  free_str
};
  
//////////// String list

#define LIST_SEPARATOR ','
#undef VAL
#define VAL(x) (*(char***)(x))

#define OP_NONE 0
#define OP_ADD 1
#define OP_PRE 2
#define OP_DEL 3
#define OP_CLR 4

static void free_str_list(void* dst) {
  char** d;
  int i;

  if(!dst || !VAL(dst)) return;
  d = VAL(dst);

// FIXME!!!
#ifndef NO_FREE
  for(i = 0 ; d[i] != NULL ; i++)
    free(d[i]);
  free(d);
#endif
  VAL(dst) = NULL;
}

static int str_list_add(char** add, int n,void* dst,int pre) {
  char** lst = VAL(dst);
  int ln;

  if(!dst) return M_OPT_PARSER_ERR;
  lst = VAL(dst);

  for(ln = 0 ; lst && lst[ln] ; ln++)
    /**/;

  lst = realloc(lst,(n+ln+1)*sizeof(char*));
  
  if(pre) {
    memmove(&lst[n],lst,(ln+1)*sizeof(char*));
    memcpy(lst,add,n*sizeof(char*));
  } else 
    memcpy(&lst[ln],add,(n+1)*sizeof(char*));

  free(add);

  VAL(dst) = lst;
  
  return 1;
}

static int str_list_del(char** del, int n,void* dst) {
  char **lst,*ep,**d;
  int i,ln,s;
  long idx;
  
  if(!dst) return M_OPT_PARSER_ERR;
  lst = VAL(dst);

  for(ln = 0 ; lst && lst[ln] ; ln++)
    /**/;
  s = ln;

  for(i = 0 ; del[i] != NULL ; i++) {
    idx = strtol(del[i], &ep, 0);
    if(*ep) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Invalid index: %s\n",del[i]);
      free(del[i]);
      continue;
    }
    free(del[i]);
    if(idx < 0 || idx >= ln) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Index %d is out of range\n",idx);
      continue;
    } else if(!lst[idx])
      continue;
    free(lst[idx]);
    lst[idx] = NULL;
    s--;
  }
  free(del);

  if(s == 0) {
    if(lst) free(lst);
    VAL(dst) = NULL;
    return 1;
  }

  d = calloc(s+1,sizeof(char*));
  for(i = 0, n = 0 ; i < ln ; i++) {
    if(!lst[i]) continue;
    d[n] = lst[i];
    n++;
  }
  d[s] = NULL;

  if(lst) free(lst);
  VAL(dst) = d;

  return 1;
}
  

static int parse_str_list(m_option_t* opt,char *name, char *param, void* dst, int src) {
  int n = 0,len = strlen(opt->name);
  char *ptr = param, *last_ptr, **res;
  int op = OP_NONE;

  if(opt->name[len-1] == '*' && ((int)strlen(name) > len - 1)) {
    char* n = &name[len-1];
    if(strcasecmp(n,"-add") == 0)
      op = OP_ADD;
    else if(strcasecmp(n,"-pre") == 0)
      op = OP_PRE;
    else if(strcasecmp(n,"-del") == 0)
      op = OP_DEL;
    else if(strcasecmp(n,"-clr") == 0)
      op = OP_CLR;
    else
      return M_OPT_UNKNOW;
  }

  // Clear the list ??
  if(op == OP_CLR) {
    if(dst)
      free_str_list(dst);
    return 0;
  }

  // All other op need a param
  if (param == NULL || strlen(param) == 0)
      return M_OPT_MISSING_PARAM;


  while(ptr[0] != '\0') {
    last_ptr = ptr;
    ptr = strchr(ptr,LIST_SEPARATOR);
    if(!ptr) {
      n++;
      break;
    }
    ptr++;
    n++;
  }
  if(n == 0)
    return M_OPT_INVALID;
  if( ((opt->flags & M_OPT_MIN) && (n < opt->min)) || 
      ((opt->flags & M_OPT_MAX) && (n > opt->max)) )
    return M_OPT_OUT_OF_RANGE;

  if(!dst) return 1;

  res = malloc((n+2)*sizeof(char*));
  ptr = param;
  n = 0;

  while(1) {
    last_ptr = ptr;
    ptr = strchr(ptr,LIST_SEPARATOR);
    if(!ptr) {
      res[n] = strdup(last_ptr);
      n++;
      break;
    }
    len = ptr - last_ptr;
    res[n] = (char*)malloc(len + 1);
    if(len) strncpy(res[n],last_ptr,len);
    res[n][len] = '\0';
    ptr++;
    n++;
  }
  res[n] = NULL;

  switch(op) {
  case OP_ADD:
    return str_list_add(res,n,dst,0);
  case OP_PRE:
    return str_list_add(res,n,dst,1);
  case OP_DEL:
    return str_list_del(res,n,dst);
  }

  if(VAL(dst))
    free_str_list(dst);
  VAL(dst) = res;

  return 1;
}
  
static void copy_str_list(m_option_t* opt,void* dst, void* src) {
  int n;
  char **d,**s;

  if(!(dst && src)) return;
  s = VAL(src);

  if(VAL(dst))
    free_str_list(dst);

  if(!s) {
    VAL(dst) = NULL;
    return;
  }

  for(n = 0 ; s[n] != NULL ; n++)
    /* NOTHING */;
  d = (char**)malloc((n+1)*sizeof(char*));
  for( ; n >= 0 ; n--)
    d[n] = s[n] ? strdup(s[n]) : NULL;

  VAL(dst) = d;
}

static char* print_str_list(m_option_t* opt, void* src) {
  char **lst = NULL;
  char *ret = NULL,*last = NULL;
  int i;
  
  if(!(src && VAL(src))) return NULL;
  lst = VAL(src);

  for(i = 0 ; lst[i] ; i++) {
    if(last) {
      ret = dup_printf("%s,%s",last,lst[i]);
      free(last);
    } else
      ret = strdup(lst[i]);
    last = ret;
  }
  if(last && last != ret) free(last);
  return ret;
}

m_option_type_t m_option_type_string_list = {
  "String list",
  "A list of string separated by ','\n"
  "Option with name that finish by an * allow to use the following suffix : \n"
  "\t-add : add the given parameters at the end of list\n"
  "\t-pre : add the given parameters at the begining of list\n"
  "\t-del : remove the entry at the given indexs\n"
  "\t-clr : clear the list\n"
  "e.g: -vop-add flip,mirror -vop-del 2,5\n",  
  sizeof(char**),
  M_OPT_TYPE_DYNAMIC | M_OPT_TYPE_ALLOW_WILDCARD,
  parse_str_list,
  print_str_list,
  copy_str_list,
  copy_str_list,
  copy_str_list,
  free_str_list
};
 
 
///////////////////  Func based options

// A chained list to save the various calls for func_param and func_full
typedef struct m_func_save m_func_save_t;
struct m_func_save {
  m_func_save_t* next;
  char* name;
  char* param;
};

#undef VAL
#define VAL(x) (*(m_func_save_t**)(x))

static void free_func_pf(void* src) {
  m_func_save_t *s,*n;

  if(!src) return;

  s = VAL(src);
  
  while(s) {
    n = s->next;
    free(s->name);
    if(s->param) free(s->param);
    free(s);
    s = n;
  }
  VAL(src) = NULL;
}

// Parser for func_param and func_full
static int parse_func_pf(m_option_t* opt,char *name, char *param, void* dst, int src) {
  m_func_save_t *s,*p;

  if(!dst)
    return 1;

  s = (m_func_save_t*)calloc(1,sizeof(m_func_save_t));
  s->name = strdup(name);
  s->param = param ? strdup(param) : NULL;

  p = VAL(dst);
  if(p) {
    for( ; p->next != NULL ; p = p->next)
      /**/;
    p->next = s;
  } else
    VAL(dst) = s;

  return 1;
}

static void copy_func_pf(m_option_t* opt,void* dst, void* src) {
  m_func_save_t *d = NULL, *s,* last = NULL;

  if(!(dst && src)) return;
  s = VAL(src);

  if(VAL(dst))
    free_func_pf(dst);

  while(s) {
    d = (m_func_save_t*)malloc(sizeof(m_func_save_t));
    d->name = strdup(s->name);
    d->param = s->param ? strdup(s->param) : NULL;
    if(last)
      last->next = d;
    else
      VAL(dst) = d;
    last = d;
    s = s->next;
  }
  
    
}

/////////////////// Func_param

static void set_func_param(m_option_t* opt, void* dst, void* src) {
  m_func_save_t* s;

  if(!src) return;
  s = VAL(src);
  
  if(!s) return;

  // Revert if needed
  if(opt->priv) ((m_opt_default_func_t)opt->priv)(opt,opt->name);
  for( ; s != NULL ; s = s->next)
    ((m_opt_func_param_t) opt->p)(opt,s->param);
}

m_option_type_t m_option_type_func_param = {
  "Func param",
  "",
  sizeof(m_func_save_t*),
  M_OPT_TYPE_INDIRECT,
  parse_func_pf,
  NULL,
  NULL, // Nothing to do on save
  set_func_param,
  copy_func_pf,
  free_func_pf
};

/////////////////// Func_full

static void set_func_full(m_option_t* opt, void* dst, void* src) {
  m_func_save_t* s;

  if(!src) return;

  for(s = VAL(src) ; s ; s = s->next) {
    // Revert if needed
    if(opt->priv) ((m_opt_default_func_t)opt->priv)(opt,s->name);
    ((m_opt_func_full_t) opt->p)(opt,s->name,s->param);
  }
}

m_option_type_t m_option_type_func_full = {
  "Func full",
  "",
  sizeof(m_func_save_t*),
  M_OPT_TYPE_ALLOW_WILDCARD|M_OPT_TYPE_INDIRECT,
  parse_func_pf,
  NULL,
  NULL, // Nothing to do on save
  set_func_full,
  copy_func_pf,
  free_func_pf
};

/////////////// Func

#undef VAL
#define VAL(x) (*(int*)(x))

static int parse_func(m_option_t* opt,char *name, char *param, void* dst, int src) {
  if(dst)
    VAL(dst) += 1;
  return 0;
}

static void set_func(m_option_t* opt,void* dst, void* src) {
  int i;
  if(opt->priv) ((m_opt_default_func_t)opt->priv)(opt,opt->name);
  for(i = 0 ; i < VAL(src) ; i++)
    ((m_opt_func_t) opt->p)(opt);
}

m_option_type_t m_option_type_func = {
  "Func",
  "",
  sizeof(int),
  M_OPT_TYPE_INDIRECT,
  parse_func,
  NULL,
  NULL, // Nothing to do on save
  set_func,
  NULL,
  NULL
};

/////////////////// Print

static int parse_print(m_option_t* opt,char *name, char *param, void* dst, int src) {
  if(opt->type->flags&M_OPT_TYPE_INDIRECT)
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", *(char **) opt->p);
  else
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "%s", (char *) opt->p);

  if(opt->priv == NULL)
    exit(1); // Call something else instead ??
  return 1;
}

m_option_type_t m_option_type_print = {
  "Print",
  "",
  0,
  0,
  parse_print,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

m_option_type_t m_option_type_print_indirect = {
  "Print",
  "",
  0,
  M_OPT_TYPE_INDIRECT,
  parse_print,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/////////////////////// Subconfig
#undef VAL
#define VAL(x) (*(char***)(x))

static int parse_subconf(m_option_t* opt,char *name, char *param, void* dst, int src) {
  char *subparam;
  char *subopt;
  int nr = 0,i,r;
  m_option_t *subopts;
  char *token;
  char *p;
  char** lst = NULL;

  if (param == NULL || strlen(param) == 0)
    return M_OPT_MISSING_PARAM;

  subparam = malloc(strlen(param)+1);
  subopt = malloc(strlen(param)+1);
  p = strdup(param); // In case that param is a static string (cf man strtok)

  subopts = opt->p;

  token = strtok(p, (char *)&(":"));
  while(token)
    {
      int sscanf_ret;
      /* clear out */
      subopt[0] = subparam[0] = 0;
			    
      sscanf_ret = sscanf(token, "%[^=]=%[^:]", subopt, subparam);

      mp_msg(MSGT_CFGPARSER, MSGL_DBG3, "token: '%s', subopt='%s', subparam='%s' (ret: %d)\n", token, subopt, subparam, sscanf_ret);
      switch(sscanf_ret)
	{
	case 1:
	  subparam[0] = 0;
	case 2:
	  for(i = 0 ; subopts[i].name ; i++) {
	    if(!strcmp(subopts[i].name,subopt)) break;
	  }
	  if(!subopts[i].name) {
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: Unknow suboption %s\n",name,subopt);
	    return M_OPT_UNKNOW;
	  }
	  r = m_option_parse(&subopts[i],subopt,
			     subparam[0] == 0 ? NULL : subparam,NULL,src);
	  if(r < 0) return r;
	  if(dst) {
	    lst = (char**)realloc(lst,2 * (nr+2) * sizeof(char*));
	    lst[2*nr] = strdup(subopt);
	    lst[2*nr+1] = subparam[0] == 0 ? NULL : strdup(subparam);
	    memset(&lst[2*(nr+1)],0,2*sizeof(char*));
	    nr++;
	  }
	  break;
	default:
	  mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Invalid subconfig argument! ('%s')\n", token);
	  return M_OPT_INVALID;
	}
      token = strtok(NULL, (char *)&(":"));
    }

  free(subparam);
  free(subopt);
  free(p);
  if(dst)
    VAL(dst) = lst;

  return 1;
}

m_option_type_t m_option_type_subconfig = {
  "Subconfig",
  "The syntax is -option opt1=foo:flag:opt2=blah",
  sizeof(int),
  M_OPT_TYPE_HAS_CHILD,
  parse_subconf,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

#include "libmpcodecs/img_format.h"
// TODO : use an array so we parse/print easily

static int parse_imgfmt(m_option_t* opt,char *name, char *param, void* dst, int src) {
  uint32_t fmt = 0;

  if (param == NULL || strlen(param) == 0)
    return M_OPT_MISSING_PARAM;

  // From vf_format
  if(!strcasecmp(param,"444p")) fmt=IMGFMT_444P; else
  if(!strcasecmp(param,"422p")) fmt=IMGFMT_422P; else
  if(!strcasecmp(param,"411p")) fmt=IMGFMT_411P; else
  if(!strcasecmp(param,"yuy2")) fmt=IMGFMT_YUY2; else
  if(!strcasecmp(param,"yv12")) fmt=IMGFMT_YV12; else
  if(!strcasecmp(param,"i420")) fmt=IMGFMT_I420; else
  if(!strcasecmp(param,"yvu9")) fmt=IMGFMT_YVU9; else
  if(!strcasecmp(param,"if09")) fmt=IMGFMT_IF09; else
  if(!strcasecmp(param,"iyuv")) fmt=IMGFMT_IYUV; else
  if(!strcasecmp(param,"uyvy")) fmt=IMGFMT_UYVY; else
  if(!strcasecmp(param,"bgr24")) fmt=IMGFMT_BGR24; else
  if(!strcasecmp(param,"bgr32")) fmt=IMGFMT_BGR32; else
  if(!strcasecmp(param,"bgr16")) fmt=IMGFMT_BGR16; else
  if(!strcasecmp(param,"bgr15")) fmt=IMGFMT_BGR15; else
  if(!strcasecmp(param,"bgr8")) fmt=IMGFMT_BGR8; else
  if(!strcasecmp(param,"bgr4")) fmt=IMGFMT_BGR4; else
  if(!strcasecmp(param,"bgr1")) fmt=IMGFMT_BGR1; else
  if(!strcasecmp(param,"rgb24")) fmt=IMGFMT_RGB24; else
  if(!strcasecmp(param,"rgb32")) fmt=IMGFMT_RGB32; else
  if(!strcasecmp(param,"rgb16")) fmt=IMGFMT_RGB16; else
  if(!strcasecmp(param,"rgb15")) fmt=IMGFMT_RGB15; else
  if(!strcasecmp(param,"rgb8")) fmt=IMGFMT_RGB8; else
  if(!strcasecmp(param,"rgb4")) fmt=IMGFMT_RGB4; else
  if(!strcasecmp(param,"rgb1")) fmt=IMGFMT_RGB1; else { 
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: unknown format name: '%s'\n",param);
    return M_OPT_INVALID;
  }

  if(dst)
    *((uint32_t*)dst) = fmt;

  return 1;
}

m_option_type_t m_option_type_imgfmt = {
  "Image format (aka colorspace)",
  "Pls report any missing colorspace",
  sizeof(uint32_t),
  0,
  parse_imgfmt,
  NULL,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};

/// Span stuff : Not finished
static int parse_play_pos(m_play_pos_t* pos,char* opt, char *s) {
  char *ep;
  long tmp;
  int i;

  memset(pos,0,sizeof(m_play_pos_t));

  if(!s || s[0] == '\0')
    return M_OPT_MISSING_PARAM;

  // Track index
  tmp = strtol(s, &ep, 0);
  if(ep != s) {
    if(tmp < 1) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: Track/Chapter index must be > 0\n", opt);
      return M_OPT_OUT_OF_RANGE;
    }
    pos->idx = tmp;
    if(ep[0] == '\0')
      return 1;
    s = ep;
  }

  // Span
  tmp = strlen(s);
  if(s[0] != '[' || s[tmp-1] != ']') {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: invalid argument ?\n", opt);
    return M_OPT_INVALID;
  }
  s[tmp-1] = '\0';
  s++;

  // hh:mm:ss
  for( i = 2 ; i >= 0 && s[0] != 0 ; i--) {
    if(s[0] == ':') {
      tmp = 0;
      s++;
    } else {
      tmp = strtol(s, &ep, 0);
      if(tmp < 0 || (ep[0] != '\0' && ep[0] != (i > 0 ? ':' : '.') )) {
	mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: invalid argument ?\n", opt);
	return M_OPT_INVALID;
      }
      pos->seconds += tmp * pow(60,i);
      s = ep;
      if(s[0] != '\0')
	s++;

    }
  }

  // sectors
  if(s[0]) {
    tmp = strtol(s, &ep, 0);
    if(tmp < 0 || ep[0] != '\0') {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: invalid argument ?\n", opt);
      return M_OPT_INVALID;
    }
    pos->sectors = tmp;
  }

  return 1;
}


static int parse_span(m_option_t* opt,char *name, char *param, void* dst, int src) {
  m_span_t* span = dst;
  char *s = param,*e = NULL;
  int r = M_OPT_INVALID;

  if(param == NULL)
    return M_OPT_MISSING_PARAM;

  e = strchr(param,'-');
  if(e) {
    e[0] = '\0';
    e++;
  }

  if(s[0] == '\0' && e[0] == '\0') {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: invalid argument\n");
    return M_OPT_INVALID;
  }

  if(s[0]) {
    r = parse_play_pos(&span->start,name,s);
    if(r < 0) return r;
  }
  if(e && e[0] != '\0')
    r = parse_play_pos(&span->end,name,s);
  
  return r;  
}

m_option_type_t m_option_type_span = {
  "Span",
  "The syntax is 1[hh:mm:ss.zz]-5[hh:mm:ss.zz]",
  sizeof(m_span_t),
  0,
  parse_span,
  NULL,
  copy_opt,
  copy_opt,
  NULL,
  NULL
};


//// Objects (ie filters, etc) settings

#include "m_struct.h"

#undef VAL
#define VAL(x) (*(m_obj_settings_t**)(x))

static int find_obj_desc(char* name,m_obj_list_t* l,m_struct_t** ret) {
  int i;
  char* n;

  for(i = 0 ; l->list[i] ; i++) {
    n = M_ST_MB(char*,l->list[i],l->name_off);
    if(!strcmp(n,name)) {
      *ret = M_ST_MB(m_struct_t*,l->list[i],l->desc_off);
      return 1;
    }
  }
  return 0;
}

static int get_obj_param(char* opt_name,char* obj_name, m_struct_t* desc,
			 char* str,int* nold,int oldmax,char** dst) {
  char* eq,param;
  m_option_t* opt;
  int r;

  eq = strchr(str,'=');
  if(eq && eq == str)
    eq = NULL;

  if(eq) {
    char* p = eq + 1;
    if(p[0] == '\0') p = NULL;
    eq[0] = '\0';
    opt = m_option_list_find(desc->fields,str);
    if(!opt) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: %s doesn't have a %s parameter\n",opt_name,obj_name,str);
      return M_OPT_UNKNOW;
    }
    r = m_option_parse(opt,str,p,NULL,M_CONFIG_FILE);
    if(r < 0) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: error while parsing %s parameter %s (%s)\n",opt_name,obj_name,str,p);
      eq[0] = '=';
      return r;
    }
    if(dst) {
      dst[0] = strdup(str);
      dst[1] = p ? strdup(p) : NULL;
    }
    eq[0] = '=';
  } else {
    if((*nold) >= oldmax) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: %s have only %d params, so yon can't give more than that unnamed params\n",
	     opt_name,obj_name,oldmax);
      return M_OPT_OUT_OF_RANGE;
    }
    opt = &desc->fields[(*nold)];
    r = m_option_parse(opt,opt->name,str,NULL,M_CONFIG_FILE);
    if(r < 0) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: error while parsing %s parameter %s (%s)\n",opt_name,obj_name,opt->name,str);
      return r;
    }
    if(dst) {
      dst[0] = strdup(opt->name);
      dst[1] = strdup(str);
    }
    (*nold)++;
  }
  return 1;
}

static int get_obj_params(char* opt_name, char* name,char* params,
			  m_struct_t* desc,char*** _ret) {
  int n = 0,nold = 0, nopts,r;
  char* ptr,*last_ptr = params,*eq;
  char** ret;

  if(!strcmp(params,"help")) { // Help
    char min[50],max[50];
    if(!desc->fields) {
      printf("%s doesn't have any options\n\n",name);
      //exit_player();
      exit(0);
    }
    printf("\n Name                 Type            Min        Max\n\n");
    for(n = 0 ; desc->fields[n].name ; n++) {
      m_option_t* opt = &desc->fields[n];
      if(opt->type->flags & M_OPT_TYPE_HAS_CHILD) continue;
      if(opt->flags & M_OPT_MIN)
	sprintf(min,"%-8.0f",opt->min);
      else
	strcpy(min,"No");
      if(opt->flags & M_OPT_MAX)
	sprintf(max,"%-8.0f",opt->max);
      else
	strcpy(max,"No");
      printf(" %-20.20s %-15.15s %-10.10s %-10.10s\n",
	     opt->name,
	     opt->type->name,
	     min,
	     max);
    }
    printf("\n");
    //exit_player() isn't avaible in mencoder
    exit(0);
  }

  for(nopts = 0 ; desc->fields[nopts].name ; nopts++)
    /* NOP */;

  // TODO : Check that each opt can be parsed
  r = 1;
  while(last_ptr && last_ptr[0] != '\0') {
    ptr = strchr(last_ptr,':');
    if(!ptr) {
      r = get_obj_param(opt_name,name,desc,last_ptr,&nold,nopts,NULL);
      n++;
      break;
    }
    ptr[0] = '\0';
    r = get_obj_param(opt_name,name,desc,last_ptr,&nold,nopts,NULL);
    ptr[0] = ':';
    if(r < 0) break;
    n++;
    last_ptr = ptr+1;
  }
  if(r < 0) return r;
  if(!_ret) // Just test
    return 1;

  ret = malloc((n+2)*2*sizeof(char*));
  n = nold = 0;
  last_ptr = params;
  
  while(last_ptr && last_ptr[0] != '\0') {
    ptr = strchr(last_ptr,':');
    if(!ptr) {
      get_obj_param(opt_name,name,desc,last_ptr,&nold,nopts,&ret[n*2]);
      n++;
      break;
    }
    ptr[0] = '\0';
    get_obj_param(opt_name,name,desc,last_ptr,&nold,nopts,&ret[n*2]);
    n++;
    last_ptr = ptr+1;
  }
  ret[n*2] = ret[n*2+1] = NULL;  
  *_ret = ret;
  
  return 1;
}


static int parse_obj_settings(char* opt,char* str,m_obj_list_t* list,
			      m_obj_settings_t **_ret, int ret_n) {
  int r;
  char *param,**plist = NULL;
  m_struct_t* desc;
  m_obj_settings_t *ret = _ret ? *_ret : NULL;
  

  // Now check that the object exist
  param = strchr(str,'=');
  if(param) {
    param[0] = '\0';
    param++;
    if(strlen(param) <= 0)
      param = NULL;
  }


  if(!find_obj_desc(str,list,&desc)) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: %s doesn't exist\n",opt,str);
    return M_OPT_INVALID;
  }

  if(param) {
    if(!desc && _ret) {
      plist = calloc(4,sizeof(char*));
      plist[0] = strdup("_oldargs_");
      plist[1] = strdup(param);
    } else if(desc) {
      r = get_obj_params(opt,str,param,desc,_ret ? &plist : NULL);
      if(r < 0)
	return r;
    }
  }
  if(!_ret)
    return 1;

  ret = realloc(ret,(ret_n+2)*sizeof(m_obj_settings_t));
  memset(&ret[ret_n],0,2*sizeof(m_obj_settings_t));
  ret[ret_n].name = strdup(str);
  ret[ret_n].attribs = plist;

  *_ret = ret;
  return 1;
}


static int parse_obj_settings_list(m_option_t* opt,char *name,
				   char *param, void* dst, int src) {
  int n = 0,r;
  char *str;
  char *ptr, *last_ptr;
  m_obj_settings_t *res = NULL;

  // We need the objects list
  if(!opt->priv)
    return M_OPT_INVALID;

  if (param == NULL || strlen(param) == 0)
    return M_OPT_MISSING_PARAM;

  if(!strcmp(param,"help")) {
    m_obj_list_t* ol = opt->priv;
    for(n = 0 ; ol->list[n] ; n++)
      mp_msg(MSGT_VFILTER,MSGL_INFO,"  %-15s: %s\n",
	     M_ST_MB(char*,ol->list[n],ol->name_off),
	     M_ST_MB(char*,ol->list[n],ol->info_off));
    exit(0);
  }
  ptr = str = strdup(param);

  while(ptr[0] != '\0') {
    last_ptr = ptr;
    ptr = strchr(ptr,LIST_SEPARATOR);
    if(!ptr) {
      r = parse_obj_settings(name,last_ptr,opt->priv,dst ? &res : NULL,n);
      if(r < 0) {
	free(str);
	return r;
      }
      n++;
      break;
    }
    ptr[0] = '\0';
    r = parse_obj_settings(name,last_ptr,opt->priv,dst ? &res : NULL,n);
    if(r < 0) {
      free(str);
      return r;
    }
    ptr++;
    n++;
  }
  free(str);
  if(n == 0)
    return M_OPT_INVALID;

  if( ((opt->flags & M_OPT_MIN) && (n < opt->min)) || 
      ((opt->flags & M_OPT_MAX) && (n > opt->max)) )
    return M_OPT_OUT_OF_RANGE;
  
  if(dst)
    VAL(dst) = res;

  return 1;
}

static void free_obj_settings_list(void* dst) {
  int n;
  m_obj_settings_t *d;

  if(!dst || !VAL(dst)) return;

  d = VAL(dst);
#ifndef NO_FREE
  for(n = 0 ; d[n].name ; n++) {
    free(d[n].name);
    free_str_list(&(d[n].attribs));
  }
  free(d);
#endif
  VAL(dst) = NULL;
}

static void copy_obj_settings_list(m_option_t* opt,void* dst, void* src) {
  m_obj_settings_t *d,*s;
  int n;

  if(!(dst && src))
    return;

  s = VAL(src);

  if(VAL(dst))
    free_obj_settings_list(dst);
  if(!s) return;
    
    
  
  for(n = 0 ; s[n].name ; n++)
    /* NOP */;
  d = malloc((n+1)*sizeof(m_obj_settings_t));
  for(n = 0 ; s[n].name ; n++) {
    d[n].name = strdup(s[n].name);
    d[n].attribs = NULL;
    copy_str_list(NULL,&(d[n].attribs),&(s[n].attribs));
  }
  d[n].name = d[n].attribs = NULL;
  VAL(dst) = d;
}

m_option_type_t m_option_type_obj_settings_list = {
  "Object settings list",
  "",
  sizeof(m_obj_settings_t*),
  M_OPT_TYPE_DYNAMIC,
  parse_obj_settings_list,
  NULL,
  copy_obj_settings_list,
  copy_obj_settings_list,
  copy_obj_settings_list,
  free_obj_settings_list,
};



static int parse_obj_presets(m_option_t* opt,char *name,
			    char *param, void* dst, int src) {
  m_obj_presets_t* obj_p = (m_obj_presets_t*)opt->priv;
  m_struct_t *in_desc,*out_desc;
  int s,i;
  unsigned char* pre = obj_p->presets;
  char* pre_name = NULL;

  if(!obj_p) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: presets need a pointer to a m_obj_presets_t in the priv field\n",name);
    return M_OPT_PARSER_ERR;
  }

  if(!param)
    return M_OPT_MISSING_PARAM;

  in_desc = obj_p->in_desc;
  out_desc = obj_p->out_desc ? obj_p->out_desc : obj_p->in_desc;
  s = in_desc->size;

  if(!strcmp(param,"help")) {
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, "Avaible presets for %s->%s :",out_desc->name,name);
    for(pre = obj_p->presets;(pre_name = M_ST_MB(char*,pre,obj_p->name_off)) ; 
	pre +=  s) 
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s",pre_name);
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "\n");
    exit(0);
  }

  for(pre_name = M_ST_MB(char*,pre,obj_p->name_off) ; pre_name ;
      pre +=  s, pre_name = M_ST_MB(char*,pre,obj_p->name_off)) {
    if(!strcmp(pre_name,param)) break;
  }
  if(!pre_name) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: there no preset named %s\n"
	   "Avaible presets are :",name,param);
    for(pre = obj_p->presets;(pre_name = M_ST_MB(char*,pre,obj_p->name_off)) ; 
	pre +=  s) 
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, " %s",pre_name);
    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "\n");
    return M_OPT_INVALID;
  }

  if(!dst) return 1;
  
  for(i = 0 ; in_desc->fields[i].name ; i++) {
    m_option_t* out_opt = m_option_list_find(out_desc->fields,
					     in_desc->fields[i].name);
    if(!out_opt) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option %s: unable to find the target option for field %s.\nYou should report that to the developpers\n",name,in_desc->fields[i].name);
      return M_OPT_PARSER_ERR;
    }
    m_option_copy(out_opt,M_ST_MB_P(dst,out_opt->p),M_ST_MB_P(pre,in_desc->fields[i].p));
  }
  return 1;
}


m_option_type_t m_option_type_obj_presets = {
  "Object presets",
  "",
  0,
  0,
  parse_obj_presets,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

#endif
