#ifndef MPLAYER_DEBUGTOOLS_H
#define MPLAYER_DEBUGTOOLS_H

#include <stdarg.h>
#include "config.h"
#include "windef.h"

struct GUID;

/* Internal definitions (do not use these directly) */

enum DEBUG_CLASS { DBCL_FIXME, DBCL_ERR, DBCL_WARN, DBCL_TRACE, DBCL_COUNT };

#ifndef NO_TRACE_MSGS
# define GET_DEBUGGING_trace(dbch) ((dbch)[0] & (1 << DBCL_TRACE))
#else
# define GET_DEBUGGING_trace(dbch) 0
#endif

#ifndef NO_DEBUG_MSGS
# define GET_DEBUGGING_warn(dbch)  ((dbch)[0] & (1 << DBCL_WARN))
# define GET_DEBUGGING_fixme(dbch) ((dbch)[0] & (1 << DBCL_FIXME))
#else
# define GET_DEBUGGING_warn(dbch)  0
# define GET_DEBUGGING_fixme(dbch) 0
#endif

/* define error macro regardless of what is configured */
#define GET_DEBUGGING_err(dbch)  ((dbch)[0] & (1 << DBCL_ERR))

#define GET_DEBUGGING(dbcl,dbch)  GET_DEBUGGING_##dbcl(dbch)
#define SET_DEBUGGING(dbcl,dbch,on) \
    ((on) ? ((dbch)[0] |= 1 << (dbcl)) : ((dbch)[0] &= ~(1 << (dbcl))))

/* Exported definitions and macros */

/* These function return a printable version of a string, including
   quotes.  The string will be valid for some time, but not indefinitely
   as strings are re-used.  */
LPCSTR debugstr_an( LPCSTR s, int n );
LPCSTR debugstr_wn( LPCWSTR s, int n );
LPCSTR debugres_a( LPCSTR res );
LPCSTR debugres_w( LPCWSTR res );
LPCSTR debugstr_guid( const struct GUID *id );
LPCSTR debugstr_hex_dump( const void *ptr, int len );
int dbg_header_err( const char *dbg_channel, const char *func );
int dbg_header_warn( const char *dbg_channel, const char *func );
int dbg_header_fixme( const char *dbg_channel, const char *func );
int dbg_header_trace( const char *dbg_channel, const char *func );
int dbg_vprintf( const char *format, va_list args );

static inline LPCSTR debugstr_a( LPCSTR s )  { return debugstr_an( s, 80 ); }
static inline LPCSTR debugstr_w( LPCWSTR s ) { return debugstr_wn( s, 80 ); }

#define TRACE_(X) TRACE
#define WARN_(X) TRACE
#define WARN TRACE
#define ERR_(X) printf
#define ERR printf
#define FIXME_(X) TRACE
#define FIXME TRACE

#define TRACE_ON(X) 1
#define ERR_ON(X) 1

#define DECLARE_DEBUG_CHANNEL(ch) \
    extern char dbch_##ch[];
#define DEFAULT_DEBUG_CHANNEL(ch) \
    extern char dbch_##ch[]; static char * const __dbch_default = dbch_##ch;

#endif  /* MPLAYER_DEBUGTOOLS_H */
