/*
 * command line and config file parser
 */

//#define DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define ERR_NOT_AN_OPTION	-1
#define ERR_MISSING_PARAM	-2
#define ERR_OUT_OF_RANGE	-3
#define ERR_FUNC_ERR		-4

#define COMMAND_LINE		0
#define CONFIG_FILE		1

#define MAX_RECURSION_DEPTH	8

#ifdef DEBUG
#include <assert.h>
#endif

#include "cfgparser.h"

static struct config *config;
static int nr_options;		/* number of options in 'conf' */
static int parser_mode;		/* COMMAND_LINE or CONFIG_FILE */
static int recursion_depth = 0;

static int init_conf(struct config *conf, int mode)
{
#ifdef DEBUG
	assert(conf != NULL);
#endif

	/* calculate the number of options in 'conf' */
	for (nr_options = 0; conf[nr_options].name != NULL; nr_options++)
		/* NOTHING */;

	config = conf;
#ifdef DEBUG
	if (mode != COMMAND_LINE && mode != CONFIG_FILE) {
		printf("init_conf: wrong mode!\n");
		return -1;
	}
#endif
	parser_mode = mode;
	return 1;
}

static int read_option(char *opt, char *param)
{
	int i;
	int tmp_int;
	float tmp_float;
	int ret = -1;

	for (i = 0; i < nr_options; i++) {
		if (!strcasecmp(opt, config[i].name))
			break;
	}
	if (i == nr_options) {
		printf("invalid option:\n");
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	if (config[i].flags & CONF_NOCFG && parser_mode == CONFIG_FILE) {
		printf("this option can only be used on command line:\n");
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}
	if (config[i].flags & CONF_NOCMD && parser_mode == COMMAND_LINE) {
		printf("this option can only be used in config file:\n");
		ret = ERR_NOT_AN_OPTION;
		goto out;
	}

	switch (config[i].type) {
		case CONF_TYPE_FLAG:
			/* flags need a parameter in config file */
			if (parser_mode == CONFIG_FILE) {
				if (!strcasecmp(param, "yes") ||	/* any other language? */
				    !strcasecmp(param, "ja") ||
				    !strcasecmp(param, "si") ||
				    !strcasecmp(param, "igen") ||
				    !strcasecmp(param, "y") ||
				    !strcasecmp(param, "i") ||
				    !strcmp(param, "1"))
					*((int *) config[i].p) = config[i].max;
				else if (!strcasecmp(param, "no") ||
				    !strcasecmp(param, "nein") ||
				    !strcasecmp(param, "nicht") ||
				    !strcasecmp(param, "nem") ||
				    !strcasecmp(param, "n") ||
				    !strcmp(param, "0"))
					*((int *) config[i].p) = config[i].min;
				else {
					printf("invalid parameter for flag:\n");
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}
				ret = 1;
			} else {	/* parser_mode == COMMAND_LINE */
				*((int *) config[i].p) = config[i].max;
				ret = 0;
			}
			break;
		case CONF_TYPE_INT:
			if (param == NULL)
				goto err_missing_param;
			if (!isdigit(*param)) {
				printf("parameter must be an integer:\n");
				ret = ERR_OUT_OF_RANGE;
				goto out;
			}

			tmp_int = atoi(param);

			if (config[i].flags & CONF_MIN)
				if (tmp_int < config[i].min) {
					printf("parameter must be >= %d:\n", (int) config[i].min);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (config[i].flags & CONF_MAX)
				if (tmp_int > config[i].max) {
					printf("parameter must be <= %d:\n", (int) config[i].max);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((int *) config[i].p) = tmp_int;
			ret = 1;
			break;
		case CONF_TYPE_FLOAT:
			if (param == NULL)
				goto err_missing_param;
			if (!isdigit(*param)) {
				printf("parameter must be a floating point number:\n");
				ret = ERR_MISSING_PARAM;
				goto out;
			}

			tmp_float = atof(param);

			if (config[i].flags & CONF_MIN)
				if (tmp_float < config[i].min) {
					printf("parameter must be >= %f:\n", config[i].min);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (config[i].flags & CONF_MAX)
				if (tmp_float > config[i].max) {
					printf("parameter must be <= %f:\n", config[i].max);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((float *) config[i].p) = tmp_float;
			ret = 1;
			break;
		case CONF_TYPE_STRING:
			if (param == NULL)
				goto err_missing_param;

			if (config[i].flags & CONF_MIN)
				if (strlen(param) < config[i].min) {
					printf("parameter must be >= %d chars:\n",
							(int) config[i].min);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			if (config[i].flags & CONF_MAX)
				if (strlen(param) > config[i].max) {
					printf("parameter must be <= %d chars:\n",
							(int) config[i].max);
					ret = ERR_OUT_OF_RANGE;
					goto out;
				}

			*((char **) config[i].p) = strdup(param);
			ret = 1;
			break;
		case CONF_TYPE_FUNC_PARAM:
			if (param == NULL)
				goto err_missing_param;
			if ((((cfg_func_param_t) config[i].p)(config + i, param)) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 1;
			break;
		case CONF_TYPE_FUNC:
			if ((((cfg_func_t) config[i].p)(config + i)) < 0) {
				ret = ERR_FUNC_ERR;
				goto out;
			}
			ret = 0;
			break;
		case CONF_TYPE_PRINT:
			printf("%s", (char *) config[i].p);
			exit(1);
		default:
			printf("picsaba\n");
			break;
	}
out:
	return ret;
err_missing_param:
	printf("missing parameter:\n");
	ret = ERR_MISSING_PARAM;
	goto out;
}

int parse_config_file(struct config *conf, char *conffile)
{
#define PRINT_LINENUM	printf("%s(%d): ", conffile, line_num)
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

#ifdef DEBUG
	assert(conffile != NULL);
#endif
	if (++recursion_depth > MAX_RECURSION_DEPTH) {
		printf("too deep 'include'. check your configfiles\n");
		--recursion_depth;
		return -1;
	}		

	printf("Reading config file: %s\n", conffile);

	if (init_conf(conf, CONFIG_FILE) == -1) {
		ret = -1;
		goto out;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		perror("parse_config_file: can't get memory for 'line'");
		ret = -1;
		goto out;
	}

	if ((fp = fopen(conffile, "r")) == NULL) {
		perror("parse_config_file: can't open filename");
		free(line);
		ret = 0;
		goto out;
	}

	while (fgets(line, MAX_LINE_LEN, fp)) {
		line_num++;
		line_pos = 0;

		/* skip whitespaces */
		while (isspace(line[line_pos]))
			++line_pos;

		/* EOL / comment */
		if (line[line_pos] == '\0' || line[line_pos] == '#')
			continue;

		/* read option. accept char if isalnum(char) */
		for (opt_pos = 0; isalnum(line[line_pos]); /* NOTHING */) {
			opt[opt_pos++] = line[line_pos++];
			if (opt_pos >= MAX_OPT_LEN) {
				PRINT_LINENUM;
				printf("too long option\n");
				ret = -1;
				continue;
			}
		}
		if (opt_pos == 0) {
			PRINT_LINENUM;
			printf("parse error\n");
			ret = -1;
			continue;
		}
		opt[opt_pos] = '\0';
#ifdef DEBUG
		PRINT_LINENUM;
		printf("option: %s\n", opt);
#endif

		/* skip whitespaces */
		while (isspace(line[line_pos]))
			++line_pos;

		/* check '=' */
		if (line[line_pos++] != '=') {
			PRINT_LINENUM;
			printf("option without parameter\n");
			ret = -1;
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
					printf("too long parameter\n");
					ret = -1;
					continue;
				}
			}
			line_pos++;	/* skip the closing " or ' */
		} else {
			for (param_pos = 0; isprint(line[line_pos]) && !isspace(line[line_pos])
					&& line[line_pos] != '#'; /* NOTHING */) {
				param[param_pos++] = line[line_pos++];
				if (param_pos >= MAX_PARAM_LEN) {
					PRINT_LINENUM;
					printf("too long parameter\n");
					ret = -1;
					continue;
				}
			}
		}
		param[param_pos] = '\0';

		/* did we read a parameter? */
		if (param_pos == 0) {
			PRINT_LINENUM;
			printf("option without parameter\n");
			ret = -1;
			continue;
		}
#ifdef DEBUG
		PRINT_LINENUM;
		printf("parameter: %s\n", param);
#endif
		/* now, check if we have some more chars on the line */
		/* whitespace... */
		while (isspace(line[line_pos]))
			++line_pos;

		/* EOL / comment */
		if (line[line_pos] != '\0' && line[line_pos] != '#') {
			PRINT_LINENUM;
			printf("extra characters on line: %s\n", line+line_pos);
			ret = -1;
		}

		tmp = read_option(opt, param);
		switch (tmp) {
		case ERR_NOT_AN_OPTION:
		case ERR_MISSING_PARAM:
		case ERR_OUT_OF_RANGE:
		case ERR_FUNC_ERR:
			PRINT_LINENUM;
			printf("%s\n", opt);
			ret = -1;
			continue;
			/* break */
		}	
	}

	free(line);
	fclose(fp);
out:
	--recursion_depth;
	return ret;
}

int parse_command_line(struct config *conf, int argc, char **argv, char **envp, char **filename)
{
	int i;
	int found_filename = 0;
	int tmp;
	char *opt;

#ifdef DEBUG
	assert(argv != NULL);
	assert(envp != NULL);
	assert(argc >= 1);
#endif

	if (init_conf(conf, COMMAND_LINE) == -1)
		return -1;

	for (i = 1; i < argc; i++) {
		opt = argv[i];
		if (*opt != '-')
			goto not_an_option;

		/* remove trailing '-' */
		opt++;

		tmp = read_option(opt, argv[i + 1]);

		switch (tmp) {
		case ERR_NOT_AN_OPTION:
not_an_option:
			/* opt is not an option -> treat it as a filename */
			if (found_filename) {
				/* we already have a filename */
				goto err_out;
			} else {
				found_filename = 1;
				*filename = argv[i];
				printf("parse_command_line: found filename: %s\n", *filename);
				continue;	/* next option */
			}
			break;
		case ERR_MISSING_PARAM:
		case ERR_OUT_OF_RANGE:
		case ERR_FUNC_ERR:
			goto err_out;
			/* break; */
		}

		i += tmp;	/* we already processed the params (if there was any) */
	}	
	return found_filename;
err_out:
	printf("parse_command_line: %s\n", argv[i]);
	return -1;
}
