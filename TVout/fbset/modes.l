
/*
 *  Linux Frame Buffer Device Configuration
 *
 *  © Copyright 1995-1998 by Geert Uytterhoeven
 *		       (Geert.Uytterhoeven@cs.kuleuven.ac.be)
 *
 *  --------------------------------------------------------------------------
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 */


%{

#define YYSTYPE		long

#include <string.h>
#include <stdlib.h>

#include "fbset.h"
#include "modes.tab.h"

struct keyword {
    const char *name;
    int token;
    int value;
};

static struct keyword keywords[] = {
    { "mode", MODE, 0 },
    { "geometry", GEOMETRY, 0 },
    { "timings", TIMINGS, 0 },
    { "hsync", HSYNC, 0 },
    { "vsync", VSYNC, 0 },
    { "csync", CSYNC, 0 },
    { "gsync", GSYNC, 0 },
    { "extsync", EXTSYNC, 0 },
    { "bcast", BCAST, 0 },
    { "laced", LACED, 0 },
    { "double", DOUBLE, 0 },
    { "rgba", RGBA, 0 },
    { "nonstd", NONSTD, 0 },
    { "accel", ACCEL, 0 },
    { "grayscale", GRAYSCALE, 0 },
    { "endmode", ENDMODE, 0 },
    { "low", POLARITY, LOW },
    { "high", POLARITY, HIGH },
    { "false", BOOLEAN, FALSE },
    { "true", BOOLEAN, TRUE },
    { "", -1, 0 }
};

int line = 1;


void yyerror(const char *s)
{
    Die("%s:%d: %s\n", Opt_modedb, line, s);
}


int yywrap(void)
{
    return 1;
}


static int FindToken(const char *s)
{
    int i;

    for (i = 0; keywords[i].token > 0; i++)
	if (!strcasecmp(s, keywords[i].name)) {
	    yylval = keywords[i].value;
	    return keywords[i].token;
	}
    Die("%s:%d: Unknown keyword `%s'\n", Opt_modedb, line, s);
}


static const char *CopyString(const char *s)
{
    int len;
    char *s2;

    len = strlen(s)-2;
    if (!(s2 = malloc(len+1)))
	Die("No memory\n");
    strncpy(s2, s+1, len);
    s2[len] = '\0';
    return s2;
}


%}

keyword	[a-zA-Z][a-zA-Z0-9]*
number	[0-9]*
string	\"[^\"\n]*\"
comment	\#([^\n]*)
space	[ \t]+
junk	.

%%

{keyword}   {
		return FindToken(yytext);
	    }

{number}    {
		yylval = strtoul(yytext, NULL, 0);
		return NUMBER;
	    }

{string}    {
		yylval = (unsigned long)CopyString(yytext);
		return STRING;
	    }

{comment}$  break;

{space}	    break;

\n	    {
		line++;
		break;
	    }

{junk}	    {
		Die("%s:%d: Invalid token `%s'\n", Opt_modedb, line, yytext);
	    }

%%
