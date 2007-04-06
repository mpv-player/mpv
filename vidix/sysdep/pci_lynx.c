/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/

#if defined(Lynx_22)
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif

/* let's mimick the Linux Alpha stuff for LynxOS so we don't have
 * to change too much code
 */
#include <smem.h>
 
static unsigned char *pciConfBase;

static __inline__ void enable_os_io(void)
{
    pciConfBase = (unsigned char *) smem_create("PCI-CONF",
    	    (char *)0x80800000, 64*1024, SM_READ|SM_WRITE);
    if (pciConfBase == (void *) -1)
        exit(1);
}

static __inline__ void disable_os_io(void)
{
    smem_create(NULL, (char *) pciConfBase, 0, SM_DETACH);
    smem_remove("PCI-CONF");
    pciConfBase = NULL;
}

#include <smem.h>
 
static unsigned char *pciConfBase;
 
static __inline__ unsigned long
static swapl(unsigned long val)
{
	unsigned char *p = (unsigned char *)&val;
	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | (p[0] << 0));
}
 
 
#define BUS(tag) (((tag)>>16)&0xff)
#define DFN(tag) (((tag)>>8)&0xff)
 
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_SUCCESSFUL		0x00
 
static int pciconfig_read(
          unsigned char bus,
          unsigned char dev,
          unsigned char offset,
          int len,		/* unused, alway 4 */
          unsigned long *val)
{
	unsigned long _val;
	unsigned long *ptr;
 
	dev >>= 3;
	if (bus || dev >= 16) {
		*val = 0xFFFFFFFF;
		return PCIBIOS_DEVICE_NOT_FOUND;
	} else {
		ptr = (unsigned long *)(pciConfBase + ((1<<dev) | offset));
		_val = swapl(*ptr);
	}
	*val = _val;
	return PCIBIOS_SUCCESSFUL;
}
 
static int pciconfig_write(
          unsigned char bus,
          unsigned char dev,
          unsigned char offset,
          int len,		/* unused, alway 4 */
          unsigned long val)
{
	unsigned long _val;
	unsigned long *ptr;
 
	dev >>= 3;
	_val = swapl(val);
	if (bus || dev >= 16) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	} else {
		ptr = (unsigned long *)(pciConfBase + ((1<<dev) | offset));
		*ptr = _val;
	}
	return PCIBIOS_SUCCESSFUL;
}
