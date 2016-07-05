#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static void stackDump (lua_State *L) {
  int i;
  int top = lua_gettop(L);
  for (i = 1; i <= top; i++) {  /* repeat for each level */
    int t = lua_type(L, i);
    switch (t) {

      case LUA_TSTRING:  /* strings */
        printf("`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN:  /* booleans */
        printf(lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER:  /* numbers */
        printf("%g", lua_tonumber(L, i));
        break;

      default:  /* other values */
        printf("%s", lua_typename(L, t));
        break;

    }
    printf("  ");  /* put a separator */
  }
  printf("\n");  /* end the listing */
}

static lua_State *getco (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "thread expected");
  return co;
}

static int moveyielded(lua_State *L, lua_State *co) {
  int nres = lua_gettop(co);
  if (!lua_checkstack(L, nres + 1)) {
    lua_pop(co, nres);  /* remove results anyway */
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "too many results to resume");
    return 2;  /* return false + error message */
  }
  lua_pushboolean(L, 1);
  lua_xmove(co, L, nres);  /* move yielded values */
  return nres + 1; /* return true + yielded values */
}

static int auxresumek(lua_State *L, int status, lua_KContext ctx) {
  lua_State *co = (lua_State*)ctx;
  int narg;
  if(lua_gettop(L) > 4) {
    narg = 2;
    lua_settop(co, 0);
    lua_xmove(L, co, 2);
  } else {
    narg = status ? lua_gettop(co) : lua_gettop(co)-1;
  }
  lua_pushnil(L);
  lua_rawseti(L, 1, 2); /* coroset[co].stacked = nil */
  status = lua_resume(co, L, narg);
  if (status == LUA_OK) {
    return moveyielded(L, co);
  } else if(status == LUA_YIELD) {
    lua_xmove(co, L, 2); /* move tag and yielder */
    /* stack: coroset[co], old yielder, co, tag, ytag, yielder */
    if(lua_compare(L, -3, -2, LUA_OPEQ)) { /* yield was for me */
      lua_State *yco = lua_tothread(L, -1);
      lua_rawseti(L, 1, 4); /* set new coroset[co].yielder */
      return moveyielded(L, yco);
    } else if(lua_isyieldable(L)) { /* pass it along */
        lua_pushboolean(L, 1);
        lua_rawseti(L, 1, 2); /* coroset[co].stacked = true */
        return lua_yieldk(L, 2, (lua_KContext)co, auxresumek);
    } else { /* end of the line */
      lua_settop(co, 0); /* clear coroutine stack */
      /* return code for trampoline */
      return -1;
    }
  } else {
    lua_pushboolean(L, 0);
    if(lua_gettop(co) > 1 && lua_isthread(co, -2)) {
      lua_xmove(co, L, 2); /* move source and error message */
      lua_rotate(L, -2, 1); /* exchange source and error message */
      lua_rawseti(L, 1, 4); /* set coroset[co].yielder = source */
    } else {
      lua_xmove(co, L, 1);  /* move error message */
    }
    return 2;  /* false + error message */
  }
}

static int auxresume (lua_State *L, lua_State *co, lua_State *yco, int status, int narg) {
  /* stack: coroset[co], yielder, co, <args> */
  if (!lua_checkstack(yco, narg)) {
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "too many arguments to resume");
    return 2;  /* return false + error message */
  }
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "cannot resume dead coroutine");
    return 2;  /* return false + error message */
  }
  if(lua_rawgeti(L, 1, 2) != LUA_TNIL) { /* coroset[co].stacked? */
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "cannot resume stacked coroutine");
    return 2;  /* return false + error message */
  } else lua_pop(L, 1);
  lua_xmove(L, yco, narg); /* arguments go to straight to yielder */
  lua_pushthread(L);
  lua_rawseti(L, 1, 3); /* coroset[co].parent = <running coro> */
  lua_rawgeti(L, 1, 1); /* push tag */
  /* stack: coroset[co], yielder, co, tag */
  int r = auxresumek(L, status, (lua_KContext)co);
  while(r == -1) { /* trampoline */
    lua_pushlightuserdata(co, &getco);
    if(lua_pushthread(L)) {
      lua_pushfstring(co, "tag %s not found", lua_tostring(L, 4));
      lua_pop(L, 3);
    } else {
      lua_pop(L, 3);
      lua_pushliteral(co, "attempt to yield across a C-call boundary");
    }
    r = auxresumek(L, LUA_YIELD, (lua_KContext)co);
  }
  return r;
}

static int taggedcoro_coresume (lua_State *L) {
  lua_State *co = getco(L);
  lua_pushvalue(L, 1); /* copy co to top */
  if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* coroset[co] */
    return luaL_argerror(L, 1, "attempt to resume untagged coroutine");
  }
  if(lua_rawgeti(L, -1, 4) == LUA_TNIL) { /* yielder == nil? */
    lua_rotate(L, 1, 2); /* move coroset[co] and yielder to front */
    return auxresume(L, co, co, 0, lua_gettop(L) - 3); /* stack: coroset[co], yielder, co, <args> */
  } else {
    lua_State *yco = lua_tothread(L, -1);
    lua_pushnil(L);
    lua_rawseti(L, -3, 4); /* clear coroset[co].yielder */
    lua_rotate(L, 1, 2); /* move coroset[co] and yielder to front */
    return auxresume(L, co, yco, LUA_YIELD, lua_gettop(L) - 3); /* stack: coroset[co], yielder, co, <args> */
  }
}

static int moveyieldedcall (lua_State *L, lua_State *co) {
  int nres = lua_gettop(co);
  if (!lua_checkstack(L, nres)) {
    lua_pop(co, nres);  /* remove results anyway */
    return luaL_error(L, "too many results to resume");
  }
  lua_xmove(co, L, nres);  /* move yielded values */
  return nres; /* return yielded values */
}

static int auxcallk (lua_State *L, int status, lua_KContext ctx) {
  lua_State *co = (lua_State*)ctx;
  int narg;
  if(lua_gettop(L) > 4) {
    narg = 2;
    lua_settop(co, 0);
    lua_xmove(L, co, 2);
  } else {
    narg = status ? lua_gettop(co) : lua_gettop(co)-1;
  }
  lua_pushnil(L);
  lua_rawseti(L, 1, 2); /* coroset[co].stacked = nil */
  status = lua_resume(co, L, narg);
  if (status == LUA_OK) {
    return moveyieldedcall(L, co);
  } else if(status == LUA_YIELD) {
    lua_xmove(co, L, 2); /* move tag and yielder */
    /* stack: coroset[co], old yielder, co, tag, ytag, yielder */
    if(lua_compare(L, -3, -2, LUA_OPEQ)) { /* yield was for me */
      lua_State *yco = lua_tothread(L, -1);
      lua_rawseti(L, 1, 4); /* set new coroset[co].yielder */
      return moveyieldedcall(L, yco);
    } else if(lua_isyieldable(L)) { /* pass it along */
      lua_pushboolean(L, 1);
      lua_rawseti(L, 1, 2); /* coroset[co].stacked = true */
      return lua_yieldk(L, 2, (lua_KContext)co, auxcallk);
    } else { /* end of the line */
      lua_settop(co, 0); /* clear coroutine stack */
      /* return code for trampoline */
      return -1;
    }
  } else {
    if(lua_gettop(co) > 1 && lua_isthread(co, -2)) {
      lua_xmove(co, L, 2);  /* move source and error message */
    } else { /* i am the source */
      lua_pushvalue(L, 3);
      lua_xmove(co, L, 2);  /* move error message */
    }
    //lua_xmove(co, L, 1);  /* move error message */
    //if (lua_type(L, -1) == LUA_TSTRING) {  /* error object is a string? */
    //  luaL_where(L, 1);  /* add extra info */
    //  lua_insert(L, -2);
    //  lua_concat(L, 2);
    //}
    return lua_error(L);
  }
}

static int auxcall (lua_State *L, lua_State *co, lua_State *yco, int status, int narg) {
  /* stack: coroset[co], co, <args> */
  luaL_checkstack(yco, narg, "too many arguments to resume");
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    return luaL_error(L, "cannot resume dead coroutine");
  }
  if(lua_rawgeti(L, 1, 2) != LUA_TNIL) { /* coroset[co].stacked? */
    return luaL_error(L, "cannot resume stacked coroutine");
  } else lua_pop(L, 1);
  lua_xmove(L, yco, narg); /* arguments go to straight to yielder */
  lua_pushthread(L);
  lua_rawseti(L, 1, 3); /* coroset[co].parent = <running coro> */
  lua_rawgeti(L, 1, 1); /* push tag */
  /* stack: coroset[co], yielder, co, tag */
  int r = auxcallk(L, status, (lua_KContext)co);
  while(r == -1) { /* trampoline */
    lua_pushlightuserdata(co, &getco);
    if(lua_pushthread(L)) {
      lua_pushfstring(co, "tag %s not found", lua_tostring(L, 4));
      lua_pop(L, 3);
    } else {
      lua_pop(L, 3);
      lua_pushliteral(co, "attempt to yield across a C-call boundary");
    }
    r = auxcallk(L, LUA_YIELD, (lua_KContext)co);
  }
  return r;
}

static int taggedcoro_cocall (lua_State *L) {
  lua_State *co = getco(L);
  lua_pushvalue(L, 1); /* copy co to top */
  if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* coroset[co] */
    return luaL_argerror(L, 1, "attempt to resume untagged coroutine");
  }
  if(lua_rawgeti(L, -1, 4) == LUA_TNIL) { /* yielder == nil? */
    lua_rotate(L, 1, 2); /* move coroset[co] and yielder to front */
    return auxcall(L, co, co, 0, lua_gettop(L) - 3); /* stack: coroset[co], yielder, co, <args> */
  } else {
    lua_State *yco = lua_tothread(L, -1);
    lua_pushnil(L);
    lua_rawseti(L, -3, 4); /* clear coroset[co].yielder */
    lua_rotate(L, 1, 2); /* move coroset[co] and yielder to front */
    return auxcall(L, co, yco, LUA_YIELD, lua_gettop(L) - 3); /* stack: coroset[co], yielder, co, <args> */
  }
}

static int taggedcoro_cocreate (lua_State *L) {
  lua_State *NL;
  if(lua_isnoneornil(L, 1)) {
    lua_pushliteral(L, "coroutine");
    lua_replace(L, 1);
  }
  luaL_checktype(L, 2, LUA_TFUNCTION);
  NL = lua_newthread(L);
  lua_pushvalue(L, -1); /* dup */
  lua_createtable(L, 4, 0); /* meta = { <tag>, <stacked>, <parent>, <yielder> } */
  lua_pushvalue(L, 1); /* copy tag to top */
  lua_rawseti(L, -2, 1); /* meta[1] = tag */
  lua_rawset(L, lua_upvalueindex(1)); /* coroset[co] = meta */
  lua_pushvalue(L, 2);  /* copy function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}

static int taggedcoro_cocreatec (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  return taggedcoro_cocreate(L);
}

static int yieldk(lua_State *L, int status, lua_KContext ctx) {
  if(lua_gettop(L) == 2 && lua_islightuserdata(L, 1)) {
    lua_pushlightuserdata(L, &getco);
    if(lua_rawequal(L, 1, -1)) {
      lua_pop(L, 1);
      return lua_error(L);
    }
    lua_pop(L, 1);
  }
  return lua_gettop(L);
}

static int taggedcoro_yield (lua_State *L) {
  lua_rotate(L, 1, -1); /* move tag to top */
  lua_pushthread(L); /* push yielder */
  return lua_yieldk(L, lua_gettop(L), 0, yieldk);
}

static int taggedcoro_yieldc (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_pushthread(L); /* push yielder */
  return lua_yieldk(L, lua_gettop(L), 0, yieldk);
}

static int taggedcoro_coparent(lua_State *L) {
  lua_State *co = getco(L);
  lua_pushvalue(L, 1);
  if(lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
    lua_rawgeti(L, -1, 3);
  }
  return 1;
}

static int taggedcoro_cotag(lua_State *L) {
  lua_State *co = getco(L);
  lua_pushvalue(L, 1);
  if(lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
    lua_rawgeti(L, -1, 1);
  }
  return 1;
}

static int taggedcoro_cosource(lua_State *L) {
  lua_State *co = getco(L);
  lua_pushvalue(L, 1);
  if(lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
    lua_rawgeti(L, -1, 4);
  }
  return 1;
}

static int taggedcoro_costatus (lua_State *L) {
  lua_State *co = getco(L);
  if (L == co) lua_pushliteral(L, "running");
  else {
    switch (lua_status(co)) {
      case LUA_YIELD:
        lua_pushvalue(L, 1);
        lua_rawget(L, lua_upvalueindex(1));
        if(lua_rawgeti(L, -1, 2) == LUA_TNIL) {
          lua_pushliteral(L, "suspended");
        } else {
          lua_pushliteral(L, "stacked");
        }
        break;
      case LUA_OK: {
        lua_Debug ar;
        if (lua_getstack(co, 0, &ar) > 0) {  /* does it have frames? */
          lua_pushliteral(L, "normal");  /* it is running */
        } else if (lua_gettop(co) == 0) {
            lua_pushliteral(L, "dead");
        } else {
          lua_pushliteral(L, "suspended");  /* initial state */
        }
        break;
      }
      default:  /* some error occurred */
        lua_pushliteral(L, "dead");
        break;
    }
  }
  return 1;
}

static int taggedcoro_yieldable (lua_State *L) {
  if(lua_isnoneornil(L, 1)) {
    lua_pushliteral(L, "coroutine");
    lua_replace(L, 1);
  }
  if(!lua_isyieldable(L)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  lua_pushthread(L);
  while(1) { /* loop until parent is untagged or parent = nil or match tag */
    if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) {
      lua_pushboolean(L, 0);
      return 1;
    }
    lua_rawgeti(L, -1, 1);
    if(lua_compare(L, 1, -1, LUA_OPEQ)) {
      lua_pushboolean(L, 1);
      return 1;
    }
    lua_pop(L, 1);
    if(lua_rawgeti(L, -1, 3) == LUA_TNIL) {
      lua_pushboolean(L, 0);
      return 1;
    }
    if(!lua_isyieldable(lua_tothread(L, -1))) {
      lua_pushboolean(L, 0);
      return 1;
    }
  }
}

static int taggedcoro_yieldablec (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  return taggedcoro_yieldable(L);
}

static int taggedcoro_corunning (lua_State *L) {
  int ismain = lua_pushthread(L);
  lua_pushboolean(L, ismain);
  return 2;
}

static int taggedcoro_auxwrap (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  return taggedcoro_cocall(L);
}

static int taggedcoro_cowrap (lua_State *L) {
  taggedcoro_cocreate(L);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_insert(L, -2);
  lua_pushcclosure(L, taggedcoro_auxwrap, 2);
  return 1;
}

static int taggedcoro_cowrapc (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  return taggedcoro_cowrap(L);
}

static const luaL_Reg ftc_funcs[] = {
  {"create", taggedcoro_cocreatec},
  {"wrap", taggedcoro_cowrapc},
  {"yield", taggedcoro_yieldc},
  {"isyieldable", taggedcoro_yieldablec},
  {NULL, NULL}
};

static const luaL_Reg ftuc_funcs[] = {
  {"resume", taggedcoro_coresume},
  {"running", taggedcoro_corunning},
  {"status", taggedcoro_costatus},
  {"parent", taggedcoro_coparent},
  {"source", taggedcoro_cosource},
  {"tag", taggedcoro_cotag},
  {NULL, NULL}
};

static int taggedcoro_fortag(lua_State *L) {
  if(lua_isnoneornil(L, 1)) {
    lua_pushliteral(L, "coroutine");
    lua_replace(L, 1);
  }
  lua_newtable(L);
  lua_pushvalue(L, lua_upvalueindex(1));
  luaL_setfuncs(L, ftuc_funcs, 1);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, 1);
  luaL_setfuncs(L, ftc_funcs, 2);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushcclosure(L, taggedcoro_fortag, 1);
  lua_setfield(L, -2, "fortag");
  return 1;
}

static const luaL_Reg tc_funcs[] = {
  {"create", taggedcoro_cocreate},
  {"resume", taggedcoro_coresume},
  {"running", taggedcoro_corunning},
  {"status", taggedcoro_costatus},
  {"wrap", taggedcoro_cowrap},
  {"yield", taggedcoro_yield},
  {"parent", taggedcoro_coparent},
  {"isyieldable", taggedcoro_yieldable},
  {"fortag", taggedcoro_fortag},
  {"source", taggedcoro_cosource},
  {"tag", taggedcoro_cotag},
  {NULL, NULL}
};

static const luaL_Reg mt_funcs[] = {
  {"resume", taggedcoro_coresume},
  {"status", taggedcoro_costatus},
  {"parent", taggedcoro_coparent},
  {"source", taggedcoro_cosource},
  {"tag", taggedcoro_cotag},
  {NULL, NULL}
};

LUAMOD_API int luaopen_taggedcoro (lua_State *L) {
  lua_newtable(L); /* extra metadata for each coroutine */
  lua_newtable(L); /* metatable for previous table */
  lua_pushliteral(L, "k");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lua_createtable(L, 4, 0);
  lua_settable(L, -3);
  luaL_newmetatable(L, "taggedcoro");
  lua_pushvalue(L, -2); /* extra metadada for each coroutine */
  lua_newtable(L); /* __index */
  luaL_setfuncs(L, mt_funcs, 1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, taggedcoro_cocall); /* __call */
  lua_setfield(L, -2, "__call");
  lua_pop(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  luaL_setmetatable(L, "taggedcoro");
  lua_pop(L, 1);
  luaL_newlibtable(L, tc_funcs);
  luaL_setfuncs(L, tc_funcs, 1);
  return 1;
}
