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

#define CONF_MIN		(1<<0)
#define CONF_MAX		(1<<1)
#define CONF_RANGE		(CONF_MIN|CONF_MAX)
#define CONF_NOCFG		(1<<2)
#define CONF_NOCMD		(1<<3)

struct config {
	char *name;
	void *p;
	unsigned int type :3;
	unsigned int flags:4;
	float min,max;
};

typedef int (*cfg_func_param_t)(struct config *, char *);
typedef int (*cfg_func_t)(struct config *);

/* parse_config_file returns:
 * 	-1 on error (can't malloc, invalid option...)
 * 	 0 if can't open configfile
 * 	 1 on success
 */
int parse_config_file(struct config *conf, char *conffile);

/* parse_command_line reutrns:
 * 	-1 on error (invalid option...)
 * 	 0 if there was no filename on command line
 * 	 1 if it found a filename
 */
int parse_command_line(struct config *conf, int argc, char **argv, char **envp, char **filename);

#endif /* __CONFIG_H */
