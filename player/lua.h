#ifndef MP_LUA_H
#define MP_LUA_H

#include <stdbool.h>

struct MPContext;

void mp_lua_init(struct MPContext *mpctx);
void mp_lua_uninit(struct MPContext *mpctx);
void mp_lua_event(struct MPContext *mpctx, const char *name, const char *arg);
void mp_lua_script_dispatch(struct MPContext *mpctx, char *script_name,
                            int id, char *event);

#endif
