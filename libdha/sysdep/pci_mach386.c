/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/

static int io_fd;

static __inline__ void enable_os_io(void)
{
    io_fd = -1 ;
    if ((io_fd = open("/dev/iopl", O_RDWR, 0)) < 0) {
        perror("/dev/iopl");
        exit(1);
    }
}

static __inline__ void disable_os_io(void)
{
    close(io_fd);
}
