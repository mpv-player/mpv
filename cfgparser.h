/*
 * command line and config file parser
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONF_TYPE_FLAG		0
#define CONF_TYPE_INT		1
#define CONF_TYPE_FLOAT		2
#define CONF_TYPE_STRING	3

#define CONF_CHK_MIN		1<<0
#define CONF_CHK_MAX		1<<1

struct config {
	char *name;
	void *p;
	unsigned int type :2;
	unsigned int flags:2;
	float min,max;
};

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
