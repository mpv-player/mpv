/*
 * command line and config file parser
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONF_TYPE_FLAG		0
#define CONF_TYPE_INT		1
#define CONF_TYPE_FLOAT		2
#define CONF_TYPE_STRING	3
#define CONF_TYPE_FUNC		4
#define CONF_TYPE_FUNC_PARAM	5
#define CONF_TYPE_PRINT		6
#define CONF_TYPE_FUNC_FULL	7
#define CONF_TYPE_SUBCONFIG	8


#define ERR_NOT_AN_OPTION	-1
#define ERR_MISSING_PARAM	-2
#define ERR_OUT_OF_RANGE	-3
#define ERR_FUNC_ERR		-4



#define CONF_MIN		(1<<0)
#define CONF_MAX		(1<<1)
#define CONF_RANGE		(CONF_MIN|CONF_MAX)
#define CONF_NOCFG		(1<<2)
#define CONF_NOCMD		(1<<3)
#define CONF_GLOBAL		(1<<4)
#define CONF_NOSAVE                            (1<<5)


typedef struct config config_t;
typedef struct m_config m_config_t;
typedef struct config_save config_save_t;

#include "playtree.h"

typedef void (*cfg_default_func_t)(config_t *, char*);

struct config {
	char *name;
	void *p;
	unsigned int type;
	unsigned int flags;
	float min,max;
	cfg_default_func_t default_func;
};



struct m_config {
  config_t** opt_list;
  config_save_t** config_stack;
  int cs_level;
  int parser_mode;  /* COMMAND_LINE or CONFIG_FILE */
  int global; // Are we parsing global option
  play_tree_t* pt; // play tree we use for playlist option, etc
  play_tree_t* last_entry; // last added entry
  play_tree_t* last_parent; // if last_entry is NULL we must create child of this
  int recursion_depth;
};

struct config_save {
  config_t* opt;
  union {
    int as_int;
    float as_float;
    void* as_pointer;
  } param;
  char* opt_name;
};


typedef int (*cfg_func_arg_param_t)(config_t *, char *, char *);
typedef int (*cfg_func_param_t)(config_t *, char *);
typedef int (*cfg_func_t)(config_t *);

/* parse_config_file returns:
 * 	-1 on error (can't malloc, invalid option...)
 * 	 0 if can't open configfile
 * 	 1 on success
 */
int m_config_parse_config_file(m_config_t *config, char *conffile);

/* parse_command_line returns:
 * 	-1 on error (invalid option...)
 * 	 0 if there was no filename on command line
 * 	 1 if there were filenames
 */
int m_config_parse_command_line(m_config_t* config, int argc, char **argv, char **envp);


void m_config_register_options(m_config_t *config,config_t *args);

int m_config_set_option(m_config_t *config,char *opt, char *param);

config_t* m_config_get_option(m_config_t *config, char* arg);

m_config_t* m_config_new(play_tree_t* pt);

void m_config_free(m_config_t* config);

void m_config_push(m_config_t* config);

int m_config_pop(m_config_t* config);
#endif /* __CONFIG_H */
