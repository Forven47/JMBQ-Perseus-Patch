#pragma once

#include "../Structs.h"

extern struct Config config;

#define tostr(x) (static_cast<std::string>(x))
#define stdstr(x) (static_cast<std::string>(OBFUSCATE(x)))
#define STR(str) il2cpp_string_new(OBFUSCATE(str))

Il2CppString *stdstr2ilstr(std::string s);
void percyLog(const char *fmt, ...);
//const char *lua_tolstring(lua_State *instance, int index, int &strLen);
const char *lua_tolstring(lua_State *instance, int index, size_t *strLen);
void lc_newclosuretable(lua_State *L, int idx);
void lc_getupvalue(lua_State *L, int tidx, int level, int varid);

/* function(arg0_3) */
static int lcf4(lua_State *L) {
    enum { lc_nformalargs = 1 };
    lua_settop(L, 1);

    /* arg0_2:emit(CombatLoadMediator.FINISH, arg0_2._loadObs) */
    lc_getupvalue(L, lua_upvalueindex(1), 0, 1);
    lua_pushstring(L, STR("emit"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_getfield(L, LUA_GLOBALSINDEX, STR("CombatLoadMediator"));
    lua_pushstring(L, STR("FINISH"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lc_getupvalue(L, lua_upvalueindex(1), 0, 1);
    lua_pushstring(L, STR("_loadObs"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_call(L, 3, 0);
    assert(lua_gettop(L) == 1);
    return 0;
}

/* name: var0_0.init
 * function(arg0_2) */
static int CLUInitHook(lua_State *L) {
    enum { lc_nformalargs = 1 };
    lua_settop(L, 1);
    lc_newclosuretable(L, lua_upvalueindex(1));
    enum { lc1 = 2 };
    assert((lua_gettop(L) == lc1));
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);

    /* local var0_2 = arg0_2:findTF(STR("loading")) */
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("findTF"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("loading"));
    lua_call(L, 2, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._loadingProgress =
     * var0_2:Find(STR("loading_bar")):GetComponent(typeof(Slider)) */
    lua_pushvalue(L, 3);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("loading_bar"));
    lua_call(L, 2, 1);
    const int lc2 = lua_gettop(L);
    lua_pushstring(L, STR("GetComponent"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_getfield(L, LUA_GLOBALSINDEX, STR("typeof"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("Slider"));
    lua_call(L, 1, LUA_MULTRET);
    lua_call(L, (lua_gettop(L) - lc2), 1);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_loadingProgress"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._loadingProgress.value = 0 */
    lua_pushnumber(L, 0);
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_loadingProgress"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("value"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._loadingText =
     * var0_2:Find(STR("loading_label/percent")):GetComponent(typeof(Text)) */
    lua_pushvalue(L, 3);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("loading_label/percent"));
    lua_call(L, 2, 1);
    const int lc3 = lua_gettop(L);
    lua_pushstring(L, STR("GetComponent"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_getfield(L, LUA_GLOBALSINDEX, STR("typeof"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("Text"));
    lua_call(L, 1, LUA_MULTRET);
    lua_call(L, (lua_gettop(L) - lc3), 1);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_loadingText"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._loadingAnima = var0_2:Find(STR("loading_anima")) */
    lua_pushvalue(L, 3);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("loading_anima"));
    lua_call(L, 2, 1);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_loadingAnima"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._loadingAnimaPosY = arg0_2._loadingAnima.anchoredPosition.y */
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_loadingAnima"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("anchoredPosition"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("y"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_loadingAnimaPosY"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* arg0_2._finishAnima = var0_2:Find(STR("done_anima")) */
    lua_pushvalue(L, 3);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("done_anima"));
    lua_call(L, 2, 1);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_finishAnima"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 3);

    /* SetActive(arg0_2._loadingAnima, true) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SetActive"));
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_loadingAnima"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushboolean(L, 1);
    lua_call(L, 2, 0);
    assert(lua_gettop(L) == 3);

    /* SetActive(arg0_2._finishAnima, false) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SetActive"));
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_finishAnima"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushboolean(L, 0);
    lua_call(L, 2, 0);
    assert(lua_gettop(L) == 3);

    /* arg0_2._finishAnima:GetComponent(STR("DftAniEvent")):SetEndEvent(function(arg0_3)
     * 		arg0_2:emit(CombatLoadMediator.FINISH, arg0_2._loadObs)
     * 	end) */
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_finishAnima"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("GetComponent"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("DftAniEvent"));
    lua_call(L, 2, 1);
    lua_pushstring(L, STR("SetEndEvent"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushvalue(L, lc1);
    lua_pushcclosure(L, lcf4, 1);
    lua_call(L, 2, 0);
    assert(lua_gettop(L) == 3);

    /* local var1_2 = arg0_2._tf:Find(STR("bg")) */
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_tf"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("bg"));
    lua_call(L, 2, 1);
    assert(lua_gettop(L) == 4);

    /* local var2_2 = arg0_2._tf:Find(STR("bg2")) */
    lc_getupvalue(L, lc1, 0, 1);
    lua_pushstring(L, STR("_tf"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("bg2"));
    lua_call(L, 2, 1);
    assert(lua_gettop(L) == 5);

    /* local var3_2 = PlayerPrefs.GetInt(STR("bgFitMode"), 0) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("PlayerPrefs"));
    lua_pushstring(L, STR("GetInt"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("bgFitMode"));
    lua_pushnumber(L, 0);
    lua_call(L, 2, 1);
    assert(lua_gettop(L) == 6);

    /* local var4_2 = var3_2 == 1 and var2_2 or var1_2 */
    lua_pushvalue(L, 6);
    lua_pushnumber(L, 1);
    const int lc5 = lua_equal(L, -2, -1);
    lua_pop(L, 2);
    lua_pushboolean(L, lc5);
    if (lua_toboolean(L, -1)) {
        lua_pop(L, 1);
        lua_pushvalue(L, 5);
    }
    if (!(lua_toboolean(L, -1))) {
        lua_pop(L, 1);
        lua_pushvalue(L, 4);
    }
    assert(lua_gettop(L) == 7);

    /* SetActive(var1_2, var3_2 ~= 1) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SetActive"));
    lua_pushvalue(L, 4);
    lua_pushnumber(L, 1);
    const int lc6 = lua_equal(L, 6, -1);
    lua_pop(L, 1);
    lua_pushboolean(L, lc6);
    lua_pushboolean(L, !(lua_toboolean(L, -1)));
    lua_remove(L, -2);
    lua_call(L, 2, 0);
    assert(lua_gettop(L) == 7);

    /* SetActive(var2_2, var3_2 == 1) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SetActive"));
    lua_pushvalue(L, 5);
    lua_pushvalue(L, 6);
    lua_pushnumber(L, 1);
    const int lc7 = lua_equal(L, -2, -1);
    lua_pop(L, 2);
    lua_pushboolean(L, lc7);
    lua_call(L, 2, 0);
    assert(lua_gettop(L) == 7);

    /* arg0_2._tipsText = var0_2:Find(STR("tipsText")):GetComponent(typeof(Text)) */
    lua_pushvalue(L, 3);
    lua_pushstring(L, STR("Find"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_pushstring(L, STR("tipsText"));
    lua_call(L, 2, 1);
    const int lc8 = lua_gettop(L);
    lua_pushstring(L, STR("GetComponent"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_getfield(L, LUA_GLOBALSINDEX, STR("typeof"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("Text"));
    lua_call(L, 1, LUA_MULTRET);
    lua_call(L, (lua_gettop(L) - lc8), 1);
    lc_getupvalue(L, lc1, 0, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("_tipsText"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 7);
    return 0;
}
