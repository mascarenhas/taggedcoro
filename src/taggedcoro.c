
#ifdef DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "compat-5.3.h"

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 502
#define lua_isyieldable taggedcoro_isyieldable
LUA_API int taggedcoro_isyieldable (lua_State *L);
#endif

/* exports */
LUAMOD_API int luaopen_taggedcoro (lua_State *L);
/* end exports */

#ifdef DEBUG
static void stack_dump (const char *prefix, lua_State *L) {
  int i;
  int top = lua_gettop(L);
  printf("%s", prefix);
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
#endif

static lua_State *getco (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "thread expected");
  return co;
}

static int moveyielded (lua_State *L, lua_State *co) {
  int nres = lua_gettop(co);
  if (!lua_checkstack(L, nres)) {
    lua_pop(co, nres);  /* remove results anyway */
    return luaL_error(L, "too many results to resume");
  }
  lua_xmove(co, L, nres);  /* move yielded values */
  return nres; /* return yielded values */
}

static int auxcallk(lua_State *L, int status, lua_KContext ctx); /* forward declaration */

LUA_KFUNCTION(untaggedk) {
  /* stack: coroset[co], co, tag, <args> */
  lua_rawgeti(L, 1, 4); /* push yielder */
  lua_pushnil(L);
  lua_rawseti(L, 1, 4); /* clear coroset[co].yielder */
  lua_State *yco = lua_tothread(L, -1);
  lua_pop(L, 1);
  lua_xmove(L, yco, lua_gettop(L) - 3);
  return auxcallk(L, status, ctx);
}

LUA_KFUNCTION(auxcallk) {
  /* stack: coroset[co], co, tag */
  lua_State *co = lua_tothread(L, 2);
  int narg;
  if(lua_gettop(L) > 3) {
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
    if(!lua_islightuserdata(co, -1) || (&getco != lua_topointer(co, -1))) {
      /* yield from coroutine.yield, pretend it was tagged yield */
      lua_pushlightuserdata(L, &getco); /* tag */
      lua_pushvalue(L, 2);              /* i am the yielder */
      lua_pushlightuserdata(L, &getco); /* sentinel */
    } else {
      lua_xmove(co, L, 3); /* move tag, yielder, sentinel */
    }
    /* stack: coroset[co], co, tag, ytag, yielder, sentinel */
    if(lua_compare(L, -4, -3, LUA_OPEQ)) { /* yield was for me */
      lua_pop(L, 1); /* pop sentinel */
      lua_State *yco = lua_tothread(L, -1);
      lua_rawseti(L, 1, 4); /* set new coroset[co].yielder */
      return moveyielded(L, yco);
    } else if(lua_isyieldable(L)) { /* try to pass it along */
      lua_pushthread(L);
      if(lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) { /* parent is tagged, pass it along */
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
        lua_rawseti(L, 1, 2); /* coroset[co].stacked = true */
        return lua_yieldk(L, 3, 0, auxcallk);
      } else if(!lua_islightuserdata(L, -4) || (&getco != lua_topointer(L, -4))) { /* not "untagged" tag */
        lua_pop(L, 1);
        lua_settop(co, 0); /* clear coroutine stack */
        return -1; /* return code for trampoline */
      } else { /* "untagged tag" */
        lua_pop(L, 2); /* pop coroset[parent] and sentinel */
        lua_pushboolean(L, 1);
        lua_rawseti(L, 1, 2); /* coroset[co].stacked = true */
        lua_State *yco = lua_tothread(L, -1);
        lua_rawseti(L, 1, 4); /* set source in co */
        lua_pop(L, 1); /* pop ytag */
        return lua_yieldk(L, moveyielded(L, yco), 0, untaggedk); /* move yielded values and pass them */
      }
    } else { /* end of the line */
      lua_settop(co, 0); /* clear coroutine stack */
      /* return code for trampoline */
      return -1;
    }
  } else {
    lua_pushthread(L);
    if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* coroset[L] */
      if(lua_rawgeti(L, 1, 4) == LUA_TNIL) { /* co is the source */
        lua_pushvalue(L, 2);
        lua_rawseti(L, 1, 4); /* coroset[co].source = co */
        lua_pop(L, 1);
      }
    } else {
      if(lua_rawgeti(L, 1, 4) != LUA_TNIL) { /* co is not the source */
        lua_rawseti(L, -2, 4); /* coroset[L].source = coroset[co].source */
      } else { /* co is the source */
        lua_pushvalue(L, 2);
        lua_rawseti(L, 1, 4); /* coroset[co].source = co */
        lua_pushvalue(L, 2);
        lua_rawseti(L, -3, 4); /* coroset[L].source = co */
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1); /* coroset[L] */
    lua_xmove(co, L, 1); /* move error message */
    lua_settop(co, 0);
    return lua_error(L);
  }
}

static int auxcall (lua_State *L, lua_State *co, lua_State *yco, int status, int narg) {
  /* stack: coroset[co], co, <args> */
  luaL_checkstack(yco, narg, "too many arguments to resume");
  lua_xmove(L, yco, narg); /* arguments go to straight to yielder */
  lua_pushnil(L);
  lua_rawseti(L, 1, 4); /* clear coroset[co].source */
  lua_pushthread(L);
  lua_rawseti(L, 1, 3); /* coroset[co].parent = <running coro> */
  lua_rawgeti(L, 1, 1); /* push tag */
  /* stack: coroset[co], co, tag */
  int r = auxcallk(L, status, 0);
  while(r == -1) { /* trampoline */
    lua_pushlightuserdata(co, &getco);
    if(lua_pushthread(L)) {
      if(lua_islightuserdata(L, 4)) {
        lua_pop(co, 1);
        lua_pushnil(co);
        lua_pushfstring(co, "untagged coroutine not found", lua_tostring(L, 4));
      } else {
        lua_pushfstring(co, "coroutine for tag %s not found", lua_tostring(L, 4));
      }
    } else if(lua_isyieldable(L)) {
      lua_pushfstring(co, "attempt to yield across untagged coroutine");
    } else {
      lua_pushliteral(co, "attempt to yield across a C-call boundary");
    }
    lua_pop(L, 4);
    r = auxcallk(L, LUA_YIELD, 0);
  }
  return r;
}

static int taggedcoro_cocall (lua_State *L) {
  lua_State *co = getco(L);
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    return luaL_error(L, "cannot resume dead coroutine");
  }
  lua_Debug ar;
  if (lua_status(co) == LUA_OK && lua_getstack(co, 0, &ar) > 0) {  /* does it have frames? */
    return luaL_error(L, "cannot resume non-suspended coroutine");
  }
  lua_pushvalue(L, 1); /* copy co to top */
  if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* coroset[co] */
    return luaL_error(L, "cannot resume untagged coroutine");
  }
  if(lua_rawgeti(L, -1, 2) != LUA_TNIL) { /* coroset[co].stacked? */
    return luaL_error(L, "cannot resume stacked coroutine");
  } else lua_pop(L, 1);
  lua_State *yco = co;
  if(lua_rawgeti(L, -1, 4) != LUA_TNIL) { /* yielder == nil? */
    yco = lua_tothread(L, -1);
  }
  lua_pop(L, 1);
  lua_insert(L, 1); /* move coroset[co] to front */
  return auxcall(L, co, yco, 0, lua_gettop(L) - 2); /* stack: coroset[co], co, <args> */
}

LUA_KFUNCTION(resumek) {
  if (status != LUA_OK && status != LUA_YIELD) {  /* error? */
    lua_pushboolean(L, 0);  /* first result (false) */
    lua_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  } else {
    lua_pushboolean(L, 1);
    lua_insert(L, 1);
    return lua_gettop(L);  /* return true + all results */
  }
}

static int taggedcoro_coresume (lua_State *L) {
  getco(L); /* to validate the parameter */
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushcclosure(L, taggedcoro_cocall, 1);
  lua_insert(L, 1);
  return resumek(L, lua_pcallk(L, lua_gettop(L) - 1, LUA_MULTRET, 0, 0, resumek), 0);
}

static int taggedcoro_cocreate (lua_State *L) {
  lua_State *NL;
  if(lua_isnoneornil(L, 1)) {
    lua_pushliteral(L, "coroutine");
    lua_replace(L, 1);
  }
  luaL_checktype(L, 2, LUA_TFUNCTION);
  NL = lua_newthread(L);
  lua_pushvalue(L, 2);  /* copy function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  lua_pushvalue(L, -1); /* dup NL */
  lua_createtable(L, 4, 0); /* meta = { <tag>, <stacked>, <parent>, <yielder> } */
  lua_pushvalue(L, 1); /* copy tag to top */
  lua_rawseti(L, -2, 1); /* meta[1] = tag */
  lua_rawset(L, lua_upvalueindex(1)); /* coroset[co] = meta */
  return 1;
}

static int taggedcoro_cocreatec (lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_insert(L, 1);
  return taggedcoro_cocreate(L);
}

LUA_KFUNCTION(yieldk) {
  if(lua_islightuserdata(L, 1) && (&getco == lua_topointer(L, 1))) {
    return lua_error(L);
  }
  return lua_gettop(L);
}

static int taggedcoro_yield (lua_State *L) {
  lua_rotate(L, 1, -1); /* move tag to top */
  lua_pushthread(L); /* push yielder */
  lua_pushlightuserdata(L, &getco); /* sentinel */
  return lua_yieldk(L, lua_gettop(L), 0, yieldk);
}

static int taggedcoro_yieldc (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_pushthread(L); /* push yielder */
  lua_pushlightuserdata(L, &getco); /* sentinel */
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
        if((lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) ||
           (lua_rawgeti(L, -1, 2) == LUA_TNIL)) {
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
    if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* parent is untagged */
      lua_pushboolean(L, 0);
      return 1;
    }
    lua_rawgeti(L, -1, 1);
    if(lua_compare(L, 1, -1, LUA_OPEQ)) { /* match tag */
      lua_pushboolean(L, 1);
      return 1;
    }
    lua_pop(L, 1);
    if(lua_rawgeti(L, -1, 3) == LUA_TNIL) { /* parent is nil */
      lua_pushboolean(L, 0);
      return 1;
    }
    if(!lua_isyieldable(lua_tothread(L, -1))) { /* parent is main tread */
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
  if(lua_isthread(L, 1)) {
    lua_pushvalue(L, 1);
    if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) {
      return luaL_error(L, "attempt to wrap an untagged coroutine");
    } else lua_pop(L, 1);
    lua_pushvalue(L, 1);
  } else taggedcoro_cocreate(L);
  lua_pushvalue(L, -1);
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_insert(L, -2);
  lua_pushcclosure(L, taggedcoro_auxwrap, 2);
  lua_insert(L, -2);
  return 2;
}

static int taggedcoro_cowrapc (lua_State *L) {
  if(!lua_isthread(L, 1)) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_insert(L, 1);
  }
  return taggedcoro_cowrap(L);
}

/*
** {===============================================================
** Traceback - taken with modifications from lauxlib.c and ldblib.c
** ================================================================
*/

#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */

/*
** search for 'objidx' in table at index -1.
** return 1 + string at top if find a good name.
*/
static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua_istable(L, -1))
    return 0;  /* not found */
  lua_pushnil(L);  /* start 'next' loop */
  while (lua_next(L, -2)) {  /* for each pair in table */
    if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua_rawequal(L, objidx, -1)) {  /* found object? */
        lua_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        lua_remove(L, -2);  /* remove table (but keep name) */
        lua_pushliteral(L, ".");
        lua_insert(L, -2);  /* place '.' between the two names */
        lua_concat(L, 3);
        return 1;
      }
    }
    lua_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}

/*
** Search for a name for a function in all loaded modules
** (registry._LOADED).
*/
static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);  /* push function */
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  if (findfield(L, top + 1, 2)) {
    const char *name = lua_tostring(L, -1);
    if (strncmp(name, "_G.", 3) == 0) {  /* name start with '_G.'? */
      lua_pushstring(L, name + 3);  /* push name without prefix */
      lua_remove(L, -2);  /* remove original name */
    }
    lua_copy(L, -1, top + 1);  /* move name to proper place */
    lua_pop(L, 2);  /* remove pushed values */
    return 1;
  }
  else {
    lua_settop(L, top);  /* remove function and global table */
    return 0;
  }
}

static void pushfuncname (lua_State *L, lua_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
    lua_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    lua_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      lua_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
    lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    lua_pushliteral(L, "?");
}


static int lastlevel (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}

static void auxtraceback (lua_State *L, const char *msg, int lvl) {
  lua_Debug ar;
  int top = lua_gettop(L);
  if (msg) { lua_pushfstring(L, "%s\n", msg); }
  lua_pushliteral(L, "stack traceback:");
  int level = lvl;
  do {
    lua_State* current = lua_tothread(L, top); /* get current thread */
    int last = lastlevel(current);
    int n1 = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
    luaL_checkstack(L, 10, NULL);
    while (lua_getstack(current, level++, &ar)) {
      if (n1-- == 0) {  /* too many levels? */
        lua_pushliteral(L, "\n\t...");  /* add a '...' */
        level = last - LEVELS2 + 1;  /* and skip to last ones */
      } else {
        lua_getinfo(current, "Slnt", &ar);
        lua_pushfstring(L, "\n\t%s:", ar.short_src);
        if (ar.currentline > 0) { lua_pushfstring(L, "%d:", ar.currentline); }
        lua_pushliteral(L, " in ");
        pushfuncname(L, &ar);
        if (ar.istailcall) { lua_pushliteral(L, "\n\t(...tail calls...)"); }
        lua_concat(L, lua_gettop(L) - top);
      }
    }
    lua_pushvalue(L, top); /* get current "from" */
    if(lua_rawget(L, lua_upvalueindex(1)) == LUA_TNIL) { /* coroset[from] */
      lua_pop(L, 1);
      lua_pushliteral(L, "\n\treached untagged coroutine, aborting traceback");
      break;
    }
    if(lua_rawgeti(L, -1, 3) == LUA_TNIL) {
      lua_pop(L, 2);
      break;
    }
    lua_replace(L, top); /* replace "from" */
    lua_pop(L, 1);
    level = 1;
  } while(1);
  lua_concat(L, lua_gettop(L) - top);
}

static void pushsource (lua_State *L, int *arg) {
  lua_settop(L, 3);
  /* push passed thread or running thread */
  if (lua_isthread(L, 1)) {
    *arg = 1;
    lua_pushvalue(L, 1);
  }
  else {
    *arg = 0;
    lua_pushthread(L);
  }
  lua_pushvalue(L, -1); /* we may need the top */
  if(lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) { /* coroset[co] */
    if(lua_rawgeti(L, -1, 4) == LUA_TNIL) { /* coroset[co].source is "from" thread */
      lua_pop(L, 2); /* remove nil and coroset[co], use saved top */
    } else { /* stack: ... saved_top coroset[co] source */
      lua_insert(L, -3);
      lua_pop(L, 2);
    }
  } else lua_pop(L, 1); /* remove nil, use saved top */
}

static int taggedcoro_traceback (lua_State *L) {
  int arg;
  pushsource(L, &arg); /* push starting thread to top */
  const char *msg = luaL_optstring(L, arg + 1, NULL);
  int level = (int)luaL_optinteger(L, arg + 2, 1);
  auxtraceback(L, msg, level);
  return 1;
}

/* }====================================================== */

static const luaL_Reg ftc_funcs[] = {
  {"create", taggedcoro_cocreatec},
  {"wrap", taggedcoro_cowrapc},
  {"yield", taggedcoro_yieldc},
  {"isyieldable", taggedcoro_yieldablec},
  {NULL, NULL}
};

static const luaL_Reg ftuc_funcs[] = {
  {"resume", taggedcoro_coresume},
  {"call", taggedcoro_cocall},
  {"running", taggedcoro_corunning},
  {"status", taggedcoro_costatus},
  {"parent", taggedcoro_coparent},
  {"source", taggedcoro_cosource},
  {"tag", taggedcoro_cotag},
  {"traceback", taggedcoro_traceback},
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
  return 1;
}

static int taggedcoro_make (lua_State *L) {
  if(lua_isnoneornil(L, 1)) {
    lua_newtable(L);
    lua_replace(L, 1);
  }
  return taggedcoro_fortag(L);
}

static const luaL_Reg mt_funcs[] = {
  {"resume", taggedcoro_coresume},
  {"status", taggedcoro_costatus},
  {"parent", taggedcoro_coparent},
  {"source", taggedcoro_cosource},
  {"call", taggedcoro_cocall},
  {"tag", taggedcoro_cotag},
  {NULL, NULL}
};

static int taggedcoro_install(lua_State *L) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lua_createtable(L, 0, 2);
  luaL_newlibtable(L, mt_funcs); /* __index */
  lua_pushvalue(L, lua_upvalueindex(1)); /* extra metadada for each coroutine */
  luaL_setfuncs(L, mt_funcs, 1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, taggedcoro_cocall); /* __call */
  lua_setfield(L, -2, "__call");
  lua_setmetatable(L, -2);
  luaL_requiref(L, "taggedcoro", luaopen_taggedcoro, 0);
  return 1;
}

static const luaL_Reg tc_funcs[] = {
  {"create", taggedcoro_cocreate},
  {"resume", taggedcoro_coresume},
  {"call", taggedcoro_cocall},
  {"running", taggedcoro_corunning},
  {"status", taggedcoro_costatus},
  {"wrap", taggedcoro_cowrap},
  {"yield", taggedcoro_yield},
  {"parent", taggedcoro_coparent},
  {"isyieldable", taggedcoro_yieldable},
  {"fortag", taggedcoro_fortag},
  {"make", taggedcoro_make},
  {"source", taggedcoro_cosource},
  {"tag", taggedcoro_cotag},
  {"install", taggedcoro_install},
  {"traceback", taggedcoro_traceback},
  {NULL, NULL}
};

LUAMOD_API int luaopen_taggedcoro (lua_State *L) {
  luaL_newlibtable(L, tc_funcs);
  lua_newtable(L); /* extra metadata for each coroutine */
  lua_newtable(L); /* metatable for previous table */
  lua_pushliteral(L, "k");
  lua_setfield(L, -2, "__mode");
  lua_setmetatable(L, -2);
  lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
  lua_createtable(L, 4, 0);
  lua_rawset(L, -3);
  luaL_setfuncs(L, tc_funcs, 1);
  return 1;
}
