/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#define INCL_DOSFILEMGR
#include <os2.h>

static USHORT callgate[3] = {0,0,0};

static __inline__ int enable_os_io(void)
{
	HFILE hfd;
	ULONG dlen,action;
	APIRET rc;
	static char *ioDrvPath = "/dev/fastio$";
 
	if (DosOpen((PSZ)ioDrvPath, (PHFILE)&hfd, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0) != 0) {
		fprintf(stderr,"Error opening fastio$ driver...\n");
		fprintf(stderr,"Please install xf86sup.sys in config.sys!\n");
		return(42);
	}
	callgate[0] = callgate[1] = 0;
 
/* Get callgate from driver for fast io to ports and other stuff */
 
	rc = DosDevIOCtl(hfd, (ULONG)0x76, (ULONG)0x64,
		NULL, 0, NULL,
		(ULONG*)&callgate[2], sizeof(USHORT), &dlen);
	if (rc) {
		fprintf(stderr,"xf86-OS/2: EnableIOPorts failed, rc=%d, dlen=%d; emergency exit\n",
			rc,dlen);
		DosClose(hfd);
		return(42);
	}
 
/* Calling callgate with function 13 sets IOPL for the program */
 
	asm volatile ("movl $13,%%ebx;.byte 0xff,0x1d;.long _callgate"
			: /*no outputs */
			: /*no inputs */
			: "eax","ebx","ecx","edx","cc");
 
        DosClose(hfd);
	return(0);
}

static __inline__ int disable_os_io(void)
{
/* Nothing to do */
        return(0);
}
