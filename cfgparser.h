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
#define CONF_TYPE_STRING_LIST           9
#define CONF_TYPE_POSITION	10


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
  /* Use this field when your need to do something before a new value is
     assigned to your option */
	cfg_default_func_t default_func;
};



struct m_config {
  config_t** opt_list;
  config_save_t** config_stack;
  int cs_level;
  int parser_mode;  /* COMMAND_LINE or CONFIG_FILE */
  int flags;
  char* sub_conf; // When we save a subconfig
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
    off_t* as_off_t;
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
 * 	 1 otherwise
 */
int m_config_parse_command_line(m_config_t* config, int argc, char **argv);

m_config_t* m_config_new(play_tree_t* pt);

void m_config_free(m_config_t* config);

void m_config_push(m_config_t* config);

/*
 * Return 0 on error 1 on success
 */
int m_config_pop(m_config_t* config);

/*
 * Return 0 on error 1 on success
 */
int m_config_register_options(m_config_t *config,config_t *args);

/*
 * For all the following function when it's a subconfig option
 * you must give an option name like 'tv:channel' and not just 
 * 'channel'
 */

/*
 * Return 1 on sucess 0 on failure
 */
int m_config_set_option(m_config_t *config,char *opt, char *param);

/*
 * Get the config struct defining an option
 * Return NULL on error
 */
config_t* m_config_get_option(m_config_t *config, char* arg);

/*
 * Get the p field of the struct defining an option
 * Return NULL on error
 */
void* m_config_get_option_ptr(m_config_t *config, char* arg);

/*
 * Tell is an option is alredy set or not
 * Return -1 one error (requested option arg exist)
 * Otherwise 0 or 1
 */
int m_config_is_option_set(m_config_t *config, char* arg);

/*
 * Return 0 on error 1 on success
 */
int m_config_switch_flag(m_config_t *config, char* opt);

/*
 * Return 0 on error 1 on success
 */
int m_config_set_flag(m_config_t *config, char* opt, int max);

/*
 * Return the value of a flag (O or 1) and -1 on error
 */
int m_config_get_flag(m_config_t *config, char* opt);

/*
 * Set the value of an int option
 * Return 0 on error 1 on success
 */
int
m_config_set_int(m_config_t *config, char* arg,int val);

/*
 * Get the value of an int option
 * Return the option value or -1 on error
 * If err_ret is not NULL it's set to 1 on error
 */
int
m_config_get_int (m_config_t *config, char* arg,int* err_ret);

/*
 * Set the value of a float option
 * Return 0 on error 1 on success
 */
int
m_config_set_float(m_config_t *config, char* arg,float val);


/*
 * Get the value of a float option
 * Return the option value or -1 on error
 * If err_ret is not NULL it's set to 1 on error
 */
float
m_config_get_float (m_config_t *config, char* arg,int* err_ret);

#endif /* __CONFIG_H */
