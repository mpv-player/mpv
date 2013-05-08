#ifndef MP_LUA_H
#define MP_LUA_H

#include <stdbool.h>

struct MPContext;

void mp_lua_init(struct MPContext *mpctx);
void mp_lua_uninit(struct MPContext *mpctx);
void mp_lua_run(struct MPContext *mpctx, const char *source);
void mp_lua_load_file(struct MPContext *mpctx, const char *fname);
void mp_lua_update(struct MPContext *mpctx);
void mp_lua_mouse_click(struct MPContext *mpctx, bool down);
void mp_lua_mouse_move(struct MPContext *mpctx, int x, int y);

#endif
