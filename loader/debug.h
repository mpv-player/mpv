#ifndef MPLAYER_DEBUG_H
#define MPLAYER_DEBUG_H

#ifdef DEBUG
#define TRACE printf
#define dbg_printf printf
#else
#define TRACE(...)
#define dbg_printf(...)
#endif

#endif /* MPLAYER_DEBUG_H */
