#include "lua.h"

/* declarations for Lua 5.2 */
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 502

#include "lstate502.h"

LUA_API int taggedcoro_isyieldable (lua_State *L) {
  return L->nny == 0;
}

#endif /* Lua 5.2 */
