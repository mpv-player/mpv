
/// \defgroup ConfigParsers Config parsers
///
/// The \ref ConfigParsers make use of the \ref Config to setup the config variables,
/// the command line parsers also build the playlist.
///@{

/// \file

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "mp_msg.h"
#include "m_option.h"
#include "m_config.h"

/// Maximal include depth.
#define MAX_RECURSION_DEPTH	8

/// Current include depth.
static int recursion_depth = 0;

/// Setup the \ref Config from a config file.
/** \param config The config object.
 *  \param conffile Path to the config file.
 *  \return 1 on sucess, -1 on error.
 */
int m_config_parse_config_file(m_config_t* config, char *conffile)
{
#define PRINT_LINENUM	mp_msg(MSGT_CFGPARSER,MSGL_V,"%s(%d): ", conffile, line_num)
#define MAX_LINE_LEN	10000
#define MAX_OPT_LEN	1000
#define MAX_PARAM_LEN	1500
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
	int prev_mode = config->mode;
	m_profile_t* profile = NULL;

#ifdef MP_DEBUG
	assert(config != NULL);
	//	assert(conf_list != NULL);
#endif
	mp_msg(MSGT_CFGPARSER,MSGL_V,"Reading config file %s", conffile);

	if (recursion_depth > MAX_RECURSION_DEPTH) {
		mp_msg(MSGT_CFGPARSER,MSGL_ERR,": too deep 'include'. check your configfiles\n");
		ret = -1;
		goto out;
	} else
	  
	config->mode = M_CONFIG_FILE;

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		mp_msg(MSGT_CFGPARSER,MSGL_FATAL,"\ncan't get memory for 'line': %s", strerror(errno));
		ret = -1;
		goto out;
	} else

	mp_msg(MSGT_CFGPARSER,MSGL_V,"\n");

	if ((fp = fopen(conffile, "r")) == NULL) {
	  mp_msg(MSGT_CFGPARSER,MSGL_V,": %s\n", strerror(errno));
		free(line);
		ret = 0;
		goto out;
	}

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
				mp_msg(MSGT_CFGPARSER,MSGL_ERR,"too long option at line %d\n",line_num);
				errors++;
				ret = -1;
				goto nextline;
			}
		}
		if (opt_pos == 0) {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"parse error at line %d\n",line_num);
			ret = -1;
			errors++;
			continue;
		}
		opt[opt_pos] = '\0';
		
		/* Profile declaration */
		if(opt_pos > 2 && opt[0] == '[' && opt[opt_pos-1] == ']') {
			opt[opt_pos-1] = '\0';
			if(strcmp(opt+1,"default"))
				profile = m_config_add_profile(config,opt+1);
			else
				profile = NULL;
			continue;
		}

#ifdef MP_DEBUG
		PRINT_LINENUM;
		mp_msg(MSGT_CFGPARSER,MSGL_V,"option: %s\n", opt);
#endif

		/* skip whitespaces */
		while (isspace(line[line_pos]))
			++line_pos;

		/* check '=' */
		if (line[line_pos++] != '=') {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"Option %s needs a parameter at line %d\n",opt,line_num);
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
					mp_msg(MSGT_CFGPARSER,MSGL_ERR,"Option %s has a too long parameter at line %d\n",opt,line_num);
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
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"Option %s needs a parameter at line %d\n",opt,line_num);
			ret = -1;
			errors++;
			continue;
		}

#ifdef MP_DEBUG
		PRINT_LINENUM;
		mp_msg(MSGT_CFGPARSER,MSGL_V,"parameter: %s\n", param);
#endif

		/* now, check if we have some more chars on the line */
		/* whitespace... */
		while (isspace(line[line_pos]))
			++line_pos;

		/* EOL / comment */
		if (line[line_pos] != '\0' && line[line_pos] != '#') {
			PRINT_LINENUM;
			mp_msg(MSGT_CFGPARSER,MSGL_WARN,"extra characters on line %d: %s\n",line_num, line+line_pos);
			ret = -1;
		}

		if(profile) {
			if(!strcmp(opt,"profile-desc"))
				m_profile_set_desc(profile,param), tmp = 1;
			else
				tmp = m_config_set_profile_option(config,profile,
								  opt,param);
		} else
			tmp = m_config_set_option(config, opt, param);
		if (tmp < 0) {
			PRINT_LINENUM;
			if(tmp == M_OPT_UNKNOWN) {
				mp_msg(MSGT_CFGPARSER,MSGL_WARN,"Warning unknown option %s at line %d\n", opt,line_num);
				continue;
			}
			mp_msg(MSGT_CFGPARSER,MSGL_ERR,"Error parsing option %s=%s at line %d\n",opt,param,line_num);
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
	config->mode = prev_mode;
	--recursion_depth;
	return ret;
}

///@}
