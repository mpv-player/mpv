#ifdef DEBUG
#define TRACE printf
#define dbg_printf printf
#else
#define TRACE(...)
#define dbg_printf(...)
#endif
