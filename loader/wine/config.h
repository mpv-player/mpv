/* include/config.h.  Generated automatically by configure.  */
/* include/config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#define HAVE_ALLOCA_H 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if the X Window System is missing or not being used.  */
/* #undef X_DISPLAY_MISSING */

/* Define if symbols declared in assembly code need an underscore prefix */
/* #undef NEED_UNDERSCORE_PREFIX */

/* Define to use .string instead of .ascii */
#define HAVE_ASM_STRING 1

/* Define if struct msghdr contains msg_accrights */
/* #undef HAVE_MSGHDR_ACCRIGHTS */

/* Define if struct sockaddr_un contains sun_len */
/* #undef HAVE_SOCKADDR_SUN_LEN */

/* Define if you have the Xxf86dga library (-lXxf86dga).  */
#define HAVE_LIBXXF86DGA 1

/* Define if you have the Xxf86dga library version 2.0 (-lXxf86dga).  */
/* #undef HAVE_LIBXXF86DGA2 */

/* Define if you have the X Shm extension */
#define HAVE_LIBXXSHM 1

/* Define if you have the Xxf86vm library */
#define HAVE_LIBXXF86VM 1

/* Define if you have the Xpm library */
#define HAVE_LIBXXPM 1

/* Define if you have the Open Sound system.  */
#define HAVE_OSS 1

/* Define if you have the Open Sound system (MIDI interface).  */
#define HAVE_OSS_MIDI 1

/* Define if X libraries are not reentrant (compiled without -D_REENTRANT).  */
/* #undef NO_REENTRANT_X11 */

/* Define if libc is not reentrant  */
/* #undef NO_REENTRANT_LIBC */

/* Define if libc uses __errno_location for reentrant errno */
#define HAVE__ERRNO_LOCATION 1

/* Define if libc uses __error for reentrant errno */
/* #undef HAVE__ERROR */

/* Define if libc uses ___errno for reentrant errno */
/* #undef HAVE___ERRNO */

/* Define if libc uses __thr_errno for reentrant errno */
/* #undef HAVE__THR_ERRNO */

/* Define if all debug messages are to be compiled out */
/* #undef NO_DEBUG_MSGS */

/* Define if TRACE messages are to be compiled out */
/* #undef NO_TRACE_MSGS */

/* Define if the struct statfs has the member bavail */
#define STATFS_HAS_BAVAIL 1

/* Define if the struct statfs has the member bfree */
#define STATFS_HAS_BFREE 1

/* Define if the struct statfs is defined by <sys/vfs.h> */
#define STATFS_DEFINED_BY_SYS_VFS 1

/* Define if the struct statfs is defined by <sys/statfs.h> */
#define STATFS_DEFINED_BY_SYS_STATFS 1

/* Define if the struct statfs is defined by <sys/mount.h> */
/* #undef STATFS_DEFINED_BY_SYS_MOUNT */

/* Define if ncurses have the new resizeterm function */
#define HAVE_RESIZETERM 1

/* Define if ncurses have the new getbkgd function */
#define HAVE_GETBKGD 1

/* Define if IPX should use netipx/ipx.h from libc */
#define HAVE_IPX_GNU 1

/* Define if IPX includes are taken from Linux kernel */
/* #undef HAVE_IPX_LINUX */

/* Define if Mesa is present on the system or not */
/* #undef HAVE_LIBMESAGL */

/* Define if the system has dynamic link library support with the dl* API */
#define HAVE_DL_API 1

/* Define if <linux/joystick.h> defines the Linux 2.2 joystick API */
#define HAVE_LINUX_22_JOYSTICK_API 1

/* Define if the OpenGL implementation supports the GL_EXT_color_table extension */
/* #undef HAVE_GL_COLOR_TABLE */

/* Define if the OpenGL implementation supports the GL_EXT_paletted_texture extension */
/* #undef HAVE_GL_PALETTED_TEXTURE */

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG 8

/* Define if you have the __libc_fork function.  */
/* #undef HAVE___LIBC_FORK */

/* Define if you have the _lwp_create function.  */
/* #undef HAVE__LWP_CREATE */

/* Define if you have the clone function.  */
#define HAVE_CLONE 1

/* Define if you have the connect function.  */
#define HAVE_CONNECT 1

/* Define if you have the dlopen function.  */
/* #undef HAVE_DLOPEN */

/* Define if you have the gethostbyname function.  */
#define HAVE_GETHOSTBYNAME 1

/* Define if you have the getnetbyaddr function.  */
#define HAVE_GETNETBYADDR 1

/* Define if you have the getnetbyname function.  */
#define HAVE_GETNETBYNAME 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getprotobyname function.  */
#define HAVE_GETPROTOBYNAME 1

/* Define if you have the getprotobynumber function.  */
#define HAVE_GETPROTOBYNUMBER 1

/* Define if you have the getservbyport function.  */
#define HAVE_GETSERVBYPORT 1

/* Define if you have the getsockopt function.  */
#define HAVE_GETSOCKOPT 1

/* Define if you have the inet_network function.  */
#define HAVE_INET_NETWORK 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the openpty function.  */
#define HAVE_OPENPTY 1

/* Define if you have the rfork function.  */
/* #undef HAVE_RFORK */

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the sendmsg function.  */
#define HAVE_SENDMSG 1

/* Define if you have the settimeofday function.  */
#define HAVE_SETTIMEOFDAY 1

/* Define if you have the sigaltstack function.  */
#define HAVE_SIGALTSTACK 1

/* Define if you have the statfs function.  */
#define HAVE_STATFS 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strncasecmp function.  */
#define HAVE_STRNCASECMP 1

/* Define if you have the tcgetattr function.  */
#define HAVE_TCGETATTR 1

/* Define if you have the timegm function.  */
#define HAVE_TIMEGM 1

/* Define if you have the usleep function.  */
#define HAVE_USLEEP 1

/* Define if you have the vfscanf function.  */
#define HAVE_VFSCANF 1

/* Define if you have the wait4 function.  */
#define HAVE_WAIT4 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the <GL/gl.h> header file.  */
/* #undef HAVE_GL_GL_H */

/* Define if you have the <GL/glx.h> header file.  */
/* #undef HAVE_GL_GLX_H */

/* Define if you have the <X11/Xlib.h> header file.  */
#define HAVE_X11_XLIB_H 1

/* Define if you have the <X11/extensions/XShm.h> header file.  */
#define HAVE_X11_EXTENSIONS_XSHM_H 1

/* Define if you have the <X11/extensions/xf86dga.h> header file.  */
#define HAVE_X11_EXTENSIONS_XF86DGA_H 1

/* Define if you have the <X11/extensions/xf86vmode.h> header file.  */
#define HAVE_X11_EXTENSIONS_XF86VMODE_H 1

/* Define if you have the <X11/xpm.h> header file.  */
#define HAVE_X11_XPM_H 1

/* Define if you have the <a.out.h> header file.  */
#define HAVE_A_OUT_H 1

/* Define if you have the <a_out.h> header file.  */
#define HAVE_A_OUT_H 1

/* Define if you have the <arpa/inet.h> header file.  */
#define HAVE_ARPA_INET_H 1

/* Define if you have the <arpa/nameser.h> header file.  */
#define HAVE_ARPA_NAMESER_H 1

/* Define if you have the <curses.h> header file.  */
/* #undef HAVE_CURSES_H */

/* Define if you have the <dlfcn.h> header file.  */
#define HAVE_DLFCN_H 1

/* Define if you have the <elf.h> header file.  */
#define HAVE_ELF_H 1

/* Define if you have the <float.h> header file.  */
#define HAVE_FLOAT_H 1

/* Define if you have the <libio.h> header file.  */
#define HAVE_LIBIO_H 1

/* Define if you have the <link.h> header file.  */
#define HAVE_LINK_H 1

/* Define if you have the <linux/cdrom.h> header file.  */
#define HAVE_LINUX_CDROM_H 1

/* Define if you have the <linux/joystick.h> header file.  */
#define HAVE_LINUX_JOYSTICK_H 1

/* Define if you have the <linux/ucdrom.h> header file.  */
/* #undef HAVE_LINUX_UCDROM_H */

/* Define if you have the <machine/soundcard.h> header file.  */
/* #undef HAVE_MACHINE_SOUNDCARD_H */

/* Define if you have the <ncurses.h> header file.  */
#define HAVE_NCURSES_H 1

/* Define if you have the <net/if.h> header file.  */
#define HAVE_NET_IF_H 1

/* Define if you have the <netinet/in.h> header file.  */
#define HAVE_NETINET_IN_H 1

/* Define if you have the <netinet/tcp.h> header file.  */
#define HAVE_NETINET_TCP_H 1

/* Define if you have the <pty.h> header file.  */
#define HAVE_PTY_H 1

/* Define if you have the <resolv.h> header file.  */
#define HAVE_RESOLV_H 1

/* Define if you have the <sched.h> header file.  */
#define HAVE_SCHED_H 1

/* Define if you have the <socket.h> header file.  */
/* #undef HAVE_SOCKET_H */

/* Define if you have the <soundcard.h> header file.  */
/* #undef HAVE_SOUNDCARD_H */

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/cdio.h> header file.  */
/* #undef HAVE_SYS_CDIO_H */

/* Define if you have the <sys/errno.h> header file.  */
#define HAVE_SYS_ERRNO_H 1

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/filio.h> header file.  */
/* #undef HAVE_SYS_FILIO_H */

/* Define if you have the <sys/ipc.h> header file.  */
#define HAVE_SYS_IPC_H 1

/* Define if you have the <sys/lwp.h> header file.  */
/* #undef HAVE_SYS_LWP_H */

/* Define if you have the <sys/mman.h> header file.  */
#define HAVE_SYS_MMAN_H 1

/* Define if you have the <sys/modem.h> header file.  */
/* #undef HAVE_SYS_MODEM_H */

/* Define if you have the <sys/mount.h> header file.  */
#define HAVE_SYS_MOUNT_H 1

/* Define if you have the <sys/msg.h> header file.  */
#define HAVE_SYS_MSG_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/reg.h> header file.  */
#define HAVE_SYS_REG_H 1

/* Define if you have the <sys/shm.h> header file.  */
#define HAVE_SYS_SHM_H 1

/* Define if you have the <sys/signal.h> header file.  */
#define HAVE_SYS_SIGNAL_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/sockio.h> header file.  */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define if you have the <sys/soundcard.h> header file.  */
#define HAVE_SYS_SOUNDCARD_H 1

/* Define if you have the <sys/statfs.h> header file.  */
#define HAVE_SYS_STATFS_H 1

/* Define if you have the <sys/strtio.h> header file.  */
/* #undef HAVE_SYS_STRTIO_H */

/* Define if you have the <sys/syscall.h> header file.  */
#define HAVE_SYS_SYSCALL_H 1

/* Define if you have the <sys/v86.h> header file.  */
/* #undef HAVE_SYS_V86_H */

/* Define if you have the <sys/v86intr.h> header file.  */
/* #undef HAVE_SYS_V86INTR_H */

/* Define if you have the <sys/vfs.h> header file.  */
#define HAVE_SYS_VFS_H 1

/* Define if you have the <sys/vm86.h> header file.  */
#define HAVE_SYS_VM86_H 1

/* Define if you have the <sys/wait.h> header file.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the <syscall.h> header file.  */
#define HAVE_SYSCALL_H 1

/* Define if you have the <ucontext.h> header file.  */
#define HAVE_UCONTEXT_H 1

/* Define if you have the <wctype.h> header file.  */
#define HAVE_WCTYPE_H 1

/* Define if you have the curses library (-lcurses).  */
/* #undef HAVE_LIBCURSES */

/* Define if you have the i386 library (-li386).  */
/* #undef HAVE_LIBI386 */

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM 1

/* Define if you have the mmap library (-lmmap).  */
/* #undef HAVE_LIBMMAP */

/* Define if you have the ncurses library (-lncurses).  */
#define HAVE_LIBNCURSES 1

/* Define if you have the ossaudio library (-lossaudio).  */
/* #undef HAVE_LIBOSSAUDIO */

/* Define if you have the w library (-lw).  */
/* #undef HAVE_LIBW */

/* Define if you have the xpg4 library (-lxpg4).  */
/* #undef HAVE_LIBXPG4 */
