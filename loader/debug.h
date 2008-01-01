#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
#define TRACE printf
#define dbg_printf printf
#else
#define TRACE(...)
#define dbg_printf(...)
#endif

#endif /* DEBUG_H */
