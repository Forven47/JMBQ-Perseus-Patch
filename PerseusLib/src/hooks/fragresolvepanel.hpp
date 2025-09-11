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
void lc_add(lua_State *L, int idxa, int idxb);

/* name: var0_0.GetAllBluePrintStrengthenItems
 * function() */
int FRPGetAllBlueprintItemsHook(lua_State *L) {
    lua_checkstack(L, 24);
    enum { lc_nformalargs = 0 };
    lua_settop(L, 0);

    /* local var0_26 = {} */
    lua_newtable(L);
    assert(lua_gettop(L) == 1);

    /* local var1_26 = getProxy(TechnologyProxy) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("getProxy"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("TechnologyProxy"));
    lua_call(L, 1, 1);
    assert(lua_gettop(L) == 2);

    /* local var2_26 = getProxy(BagProxy) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("getProxy"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("BagProxy"));
    lua_call(L, 1, 1);
    assert(lua_gettop(L) == 3);

    /* local var3_26 = pg.ship_data_blueprint */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("pg"));
    lua_pushstring(L, STR("ship_data_blueprint"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    assert(lua_gettop(L) == 4);

    /* for iter0_26, iter1_26 in ipairs(var3_26.all) do
     * internal: local f, s, var = explist */
    enum { lc1 = 4 };
    lua_getfield(L, LUA_GLOBALSINDEX, STR("ipairs"));
    lua_pushstring(L, STR("all"));
    lua_gettable(L, 4);
    lua_call(L, 1, 3);
    while (1) {

        /* internal: local var_1, ..., var_n = f(s, var)
         *           if var_1 == nil then break end
         *           var = var_1 */
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, -3);
        lua_call(L, 2, 2);
        if (lua_isnil(L, -2)) {
            break;
        }
        lua_pushvalue(L, -2);
        lua_replace(L, -4);

        /* internal: local iter0_26 with idx 8
         * internal: local iter1_26 with idx 9 */

        /* local var4_26 = var3_26[iter1_26] */
        lua_pushvalue(L, 9);
        lua_gettable(L, 4);
        assert(lua_gettop(L) == 10);

        /* local var5_26 = var4_26.strengthen_item */
        lua_pushstring(L, STR("strengthen_item"));
        lua_gettable(L, 10);
        assert(lua_gettop(L) == 11);

        /* local var6_26 = var2_26:getItemById(var5_26) */
        lua_pushvalue(L, 3);
        lua_pushstring(L, STR("getItemById"));
        lua_gettable(L, -2);
        lua_insert(L, -2);
        lua_pushvalue(L, 11);
        lua_call(L, 2, 1);
        assert(lua_gettop(L) == 12);

        /* if var6_26 then */
        enum { lc2 = 12 };
        if (lua_toboolean(L, 12)) {

            /* local var7_26 = var1_26:getBluePrintById(var1_26:GetBlueprint4Item(var5_26)) */
            lua_pushvalue(L, 2);
            const int lc3 = lua_gettop(L);
            lua_pushstring(L, STR("getBluePrintById"));
            lua_gettable(L, -2);
            lua_insert(L, -2);
            lua_pushvalue(L, 2);
            lua_pushstring(L, STR("GetBlueprint4Item"));
            lua_gettable(L, -2);
            lua_insert(L, -2);
            lua_pushvalue(L, 11);
            lua_call(L, 2, LUA_MULTRET);
            lua_call(L, (lua_gettop(L) - lc3), 1);
            assert(lua_gettop(L) == 13);

            /* local var8_26 = var6_26.count */
            lua_pushstring(L, STR("count"));
            lua_gettable(L, 12);
            assert(lua_gettop(L) == 14);

            /* if var8_26 > 0 then */
            enum { lc4 = 14 };
            lua_pushnumber(L, 0);
            const int lc5 = lua_lessthan(L, -1, 14);
            lua_pop(L, 1);
            lua_pushboolean(L, lc5);
            const int lc6 = lua_toboolean(L, -1);
            lua_pop(L, 1);
            if (lc6) {

                /* table.insert(var0_26, Drop.New({
                 * 					id = var6_26.id,
                 * 					type = DROP_TYPE_ITEM,
                 * 					count = var6_26.count,
                 * 					maxCount = var6_26.count
                 * 				})) */
                lua_getfield(L, LUA_GLOBALSINDEX, STR("table"));
                lua_pushstring(L, STR("insert"));
                lua_gettable(L, -2);
                lua_remove(L, -2);
                const int lc7 = lua_gettop(L);
                lua_pushvalue(L, 1);
                lua_getfield(L, LUA_GLOBALSINDEX, STR("Drop"));
                lua_pushstring(L, STR("New"));
                lua_gettable(L, -2);
                lua_remove(L, -2);
                lua_createtable(L, 0, 4);
                lua_pushstring(L, STR("id"));
                lua_pushstring(L, STR("id"));
                lua_gettable(L, 12);
                lua_rawset(L, -3);
                lua_pushstring(L, STR("type"));
                lua_getfield(L, LUA_GLOBALSINDEX, STR("DROP_TYPE_ITEM"));
                lua_rawset(L, -3);
                lua_pushstring(L, STR("count"));
                lua_pushstring(L, STR("count"));
                lua_gettable(L, 12);
                lua_rawset(L, -3);
                lua_pushstring(L, STR("maxCount"));
                lua_pushstring(L, STR("count"));
                lua_gettable(L, 12);
                lua_rawset(L, -3);
                lua_call(L, 1, LUA_MULTRET);
                lua_call(L, (lua_gettop(L) - lc7), 0);
                assert(lua_gettop(L) == 14);
            }
            lua_settop(L, lc4);
            assert(lua_gettop(L) == 14);
        }
        lua_settop(L, lc2);
        assert(lua_gettop(L) == 12);

        /* internal: stack cleanup on scope exit */
        lua_pop(L, 5);
    }
    lua_settop(L, lc1);
    assert(lua_gettop(L) == 4);

    /* return var0_26 */
    lua_pushvalue(L, 1);
    return 1;
    assert(lua_gettop(L) == 4);
}
