/*
Linux Real Mode Interface - A library of DPMI-like functions for Linux.

Copyright (C) 1998 by Josh Vanderhoof

You are free to distribute and modify this file, as long as you
do not remove this copyright notice and clearly label modified
versions as being modified.

This software has NO WARRANTY.  Use it at your own risk.
Original location: http://cvs.debian.org/lrmi/
*/

#ifndef LRMI_H
#define LRMI_H

struct LRMI_regs
	{
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int reserved;
	unsigned int ebx;
	unsigned int edx;
	unsigned int ecx;
	unsigned int eax;
	unsigned short int flags;
	unsigned short int es;
	unsigned short int ds;
	unsigned short int fs;
	unsigned short int gs;
	unsigned short int ip;
	unsigned short int cs;
	unsigned short int sp;
	unsigned short int ss;
	};


#ifndef LRMI_PREFIX
#define LRMI_PREFIX LRMI_
#endif

#define LRMI_CONCAT2(a, b) 	a ## b
#define LRMI_CONCAT(a, b) 	LRMI_CONCAT2(a, b)
#define LRMI_MAKENAME(a) 	LRMI_CONCAT(LRMI_PREFIX, a)

/*
 Initialize
 returns 1 if sucessful, 0 for failure
*/
#define LRMI_init LRMI_MAKENAME(init)
int
LRMI_init(void);

/*
 Simulate a 16 bit far call
 returns 1 if sucessful, 0 for failure
*/
#define LRMI_call LRMI_MAKENAME(call)
int
LRMI_call(struct LRMI_regs *r);

/*
 Simulate a 16 bit interrupt
 returns 1 if sucessful, 0 for failure
*/
#define LRMI_int LRMI_MAKENAME(int)
int
LRMI_int(int interrupt, struct LRMI_regs *r);

/*
 Allocate real mode memory
 The returned block is paragraph (16 byte) aligned
*/
#define LRMI_alloc_real LRMI_MAKENAME(alloc_real)
void *
LRMI_alloc_real(int size);

/*
 Free real mode memory
*/
#define LRMI_free_real LRMI_MAKENAME(free_real)
void
LRMI_free_real(void *m);

#endif
