#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// filled on the python side with .format()
{additional_lua_test_header}

void test_lua(void) {{
    lua_State *L = luaL_newstate();
    lua_pushstring(L, "test");
    lua_setglobal(L, "test");
}}

void test_other(void) {{
    // filled on the python side with .format()
    {additional_lua_test_code}
}}

int main(void) {{
    test_lua();
    test_other();
    return 0;
}}
