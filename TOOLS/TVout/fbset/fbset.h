/*
 *  Linux Frame Buffer Device Configuration
 *
 *  © Copyright 1995-1998 by Geert Uytterhoeven
 *			(Geert.Uytterhoeven@cs.kuleuven.ac.be)
 *
 *  --------------------------------------------------------------------------
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 */


#include <stdio.h>
#include <sys/types.h>

#ifdef __GLIBC__
#include <asm/types.h>
#endif

#define VERSION         "Linux Frame Buffer Device Configuration " \
			"Version 2.1 (23/06/1999)\n"  \
			"(C) Copyright 1995-1999 by Geert Uytterhoeven\n"

#define LOW		(0)
#define HIGH		(1)

#define FALSE		(0)
#define TRUE		(1)

struct color {
    unsigned int length;
    unsigned int offset;
};

struct VideoMode {
    struct VideoMode *next;
    const char *name;
    /* geometry */
    __u32 xres;
    __u32 yres;
    __u32 vxres;
    __u32 vyres;
    __u32 depth;
    __u32 nonstd;
    /* acceleration */
    __u32 accel_flags;
    /* timings */
    __u32 pixclock;
    __u32 left;
    __u32 right;
    __u32 upper;
    __u32 lower;
    __u32 hslen;
    __u32 vslen;
    /* flags */
    unsigned hsync : 1;
    unsigned vsync : 1;
    unsigned csync : 1;
    unsigned gsync : 1;
    unsigned extsync : 1;
    unsigned bcast : 1;
    unsigned laced : 1;
    unsigned dblscan : 1;
    unsigned grayscale : 1;
    /* scanrates */
    double drate;
    double hrate;
    double vrate;
    /* RGB entries */
    struct color red, green, blue, transp;
};

extern FILE *yyin;
extern int line;
extern const char *Opt_modedb;

extern int yyparse(void);
extern void Die(const char *fmt, ...) __attribute__ ((noreturn));
extern void AddVideoMode(const struct VideoMode *vmode);
extern void makeRGBA(struct VideoMode *vmode, const char* opt);
