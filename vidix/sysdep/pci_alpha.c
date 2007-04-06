/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/

static int pci_config_type( void ) { return 1; }

static int pci_get_vendor(
          unsigned char bus,
          unsigned char dev,
          int func)
{
    unsigned long retval;
    pciconfig_read(bus, dev<<3, PCI_ID_REG, 4, &retval);
    return retval;
}

static long pci_config_read_long(
          unsigned char bus,
          unsigned char dev,
          int func, 
          unsigned cmd)
{
    unsigned long retval;
    pciconfig_read(bus, dev<<3, cmd, 4, &retval);
    return retval;
}

