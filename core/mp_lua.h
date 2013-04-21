#ifndef MP_LUA_H
#define MP_LUA_H

struct MPContext;

void mp_lua_init(struct MPContext *mpctx);
void mp_lua_uninit(struct MPContext *mpctx);
void mp_lua_run(struct MPContext *mpctx, const char *source);
void mp_lua_load_file(struct MPContext *mpctx, const char *fname);
void mp_lua_update(struct MPContext *mpctx);

#endif
