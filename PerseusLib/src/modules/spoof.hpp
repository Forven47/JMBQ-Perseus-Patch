#pragma once

// full credit of everything in this file goes to cmdtaves

#include <cstddef> // size_t
#include <cstring>
#include <string>

#include "../Structs.h"

extern struct Config config;

#define tostr(x) (static_cast<std::string>(x))
#define stdstr(x) (static_cast<std::string>(OBFUSCATE(x)))
#define STR(str) il2cpp_string_new(OBFUSCATE(str))

Il2CppString *stdstr2ilstr(std::string s);
void luaHookFunc(lua_State *L, std::string field, lua_CFunction func, std::string backup_prefix);
/** const char *lua_tolstring(lua_State *instance, int index, int &strLen); **/
const char *lua_tolstring(lua_State *instance, int index, size_t *strLen);
void parseLv(lua_State *L, double input);
void lc_newclosuretable(lua_State *L, int idx);
void lc_getupvalue(lua_State *L, int tidx, int level, int varid);
void lc_div(lua_State *L, int idxa, int idxb);

int wrapPVDPUpdateInfo(lua_State *L) {
    // call original UpdateInfo
    lua_getglobal(L, STR("PlayerVitaeDetailPage"));
    lua_getfield(L, -1, STR("oldUpdateInfo"));
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_pcall(L, 1, 0, 0);

    // override values
    if (!config.Spoof.name.empty()) {
        lua_getfield(L, 1, STR("nameTxt"));
        lua_pushstring(L, stdstr2ilstr(config.Spoof.name));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
    }
    if (!config.Spoof.id.empty()) {
        lua_getfield(L, 1, STR("idTxt"));
        lua_pushstring(L, stdstr2ilstr(config.Spoof.id));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
    }

    if (config.Spoof.lv > 0) {
        std::string l = stdstr("LV.") + std::to_string(config.Spoof.lvInt);
        lua_getfield(L, 1, STR("levelTxt"));
        lua_pushstring(L, stdstr2ilstr(l));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);

        std::string expTxt = std::to_string(config.Spoof.spoofXp) + stdstr("/") + std::to_string(config.Spoof.lvMax);
        lua_getfield(L, 1, STR("expTxt"));
        //lua_pushstring(L, il2cpp_string_new(expTxt.data()));
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>(expTxt.data())));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
    }
    return 0;
}

int wrapMPIBUpdateExp(lua_State *L) {

    // call old MainPlayerInfoBtn.UpdateExp
    lua_getglobal(L, STR("MainPlayerInfoBtn"));
    lua_getfield(L, -1, STR("oldUpdateExp"));
    lua_pushvalue(L, 1);
    lua_pcall(L, 1, 0, 0);

    // override values
    if (config.Spoof.lv > 0) {
        // replace arg0.expSlider.value with our own spoofLevelFrac
        lua_getfield(L, 1, STR("expSlider"));
        lua_pushnumber(L, config.Spoof.lvFrac);
        lua_setfield(L, -2, STR("value"));
    }

    return 0;
}

int wrapNMPIBUpdateExp(lua_State *L) {

    // call old MainPlayerInfoBtn4Mellow.UpdateExp
    lua_getglobal(L, STR("MainPlayerInfoBtn4Mellow"));
    lua_getfield(L, -1, STR("oldUpdateExp"));
    lua_pushvalue(L, 1);
    lua_pcall(L, 1, 0, 0);

    // override values
    if (config.Spoof.lv > 0) {
        // set level and exp text
        lua_getfield(L, 1, STR("playerLevelTxt"));
        lua_pushnumber(L, config.Spoof.lvInt);
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
        lua_getfield(L, 1, STR("expTxt"));
        lua_pushnumber(L, config.Spoof.lvInt);
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);

        // set xp fraction and text scaling
        lua_getfield(L, 1, STR("expTr"));
        lua_getglobal(L, STR("Vector2"));
        lua_pushnumber(L, 70);
        lua_pushnumber(L, (config.Spoof.lvFrac * 34));
        lua_pcall(L, 2, 1, 0);
        lua_setfield(L, -2, STR("sizeDelta"));
    }

    return 0;
}

int wrapMPIBUpdateLevelAndName(lua_State *L) {
    // calls original UpdateLevelAndName
    lua_getglobal(L, STR("MainPlayerInfoBtn"));
    lua_getfield(L, -1, STR("oldUpdateLevelAndName"));
    lua_pushvalue(L, 1);
    lua_pcall(L, 1, 0, 0);

    // override values
    if (!config.Spoof.name.empty()) {
        lua_getfield(L, 1, STR("playerNameTxt"));
        lua_pushstring(L, stdstr2ilstr(config.Spoof.name));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
    }

    if (config.Spoof.lv > 0) {
        std::string l = stdstr("LV.") + std::to_string(config.Spoof.lvInt);
        lua_getfield(L, 1, STR("playerLevelTxt"));
        lua_pushstring(L, stdstr2ilstr(l));
        lua_setfield(L, -2, STR("text"));
        lua_pop(L, 1);
    }

    return 0;
}

int wrapBDDRVSetFleetVO(lua_State *L) {
    bool needNameSpoof = !config.Spoof.name.empty();
    bool needLevelSpoof = (config.Spoof.lv > 0);

    // get ys.Battle
    lua_getglobal(L, STR("ys"));
    lua_getfield(L, -1, STR("Battle"));

    // check if this bar is the friendly bar
    lua_getfield(L, -1, STR("BattleConfig"));
    lua_getfield(L, -1, STR("FRIENDLY_CODE"));
    int friendlyCode = lua_tonumber(L, -1);
    lua_pop(L, 2);
    lua_getfield(L, 2, STR("GetIFF"));
    lua_pushvalue(L, 2);
    lua_pcall(L, 1, 1, 0);
    int currentCode = lua_tonumber(L, -1);
    lua_pop(L, 1);
    bool isFriendly = (friendlyCode == currentCode);

    // get args for old function
    lua_getfield(L, -1, STR("BattleDuelDamageRateView"));
    lua_getfield(L, -1, STR("oldSetFleetVO"));
    lua_remove(L, -2);
    for (int i = 1; i <= 2; i++) {
        lua_pushvalue(L, i);
    }
    if (isFriendly) {
        /** int len;
        // override values
        lua_newtable(L);
        lua_getfield(L, 3, STR("name"));
        std::string newName = "";
        if (!needNameSpoof) {
            newName.assign(lua_tolstring(L, -1, len));
        } else {
            newName = config.Spoof.name;
        }
        lua_pop(L, 1);
        lua_pushstring(L, il2cpp_string_new(newName.data()));
        lua_setfield(L, -2, STR("name"));
        lua_getfield(L, 3, STR("level"));
        std::string newLevel = "";
        if (!needLevelSpoof) {
            int levelNum = lua_tonumber(L, -1);
            newLevel.assign(std::to_string(levelNum));
        } else {
            newLevel = std::to_string(config.Spoof.lvInt);
        }
        lua_pop(L, 1);
        lua_pushstring(L, il2cpp_string_new(newLevel.data()));
        lua_setfield(L, -2, STR("level"));
        **/
        // modified:
        lua_newtable(L);
        lua_getfield(L, 3, STR("name"));
        lua_pop(L, 0); // keep stack shape; we'll read value below
        size_t len = 0;
        const char *s = lua_tolstring(L, -1, &len);
        std::string newName;
        if (!needNameSpoof) {
            if (s) {
                if (len > 0) newName.assign(s, len);
                else newName = s;
            } else newName.clear();
        } else {
            newName = config.Spoof.name;
        }
        lua_pop(L, 1);
        //lua_pushstring(L, il2cpp_string_new(newName.c_str()));
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newName.c_str())));
        lua_setfield(L, -2, STR("name"));

        lua_getfield(L, 3, STR("level"));
        if (!needLevelSpoof) {
            int levelNum = lua_tonumber(L, -1);
            std::string newLevel = std::to_string(levelNum);
            //lua_pushstring(L, il2cpp_string_new(newLevel.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newLevel.c_str())));
            lua_setfield(L, -2, STR("level"));
        } else {
            std::string newLevel = std::to_string(config.Spoof.lvInt);
            //lua_pushstring(L, il2cpp_string_new(newLevel.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newLevel.c_str())));
            lua_setfield(L, -2, STR("level"));
        }
        lua_pop(L, 1);
    } else {
        lua_pushvalue(L, 3);
    }
    lua_pcall(L, 3, 0, 0);

    return 0;
}

int wrapBRLSetPlayer(lua_State *L) {

    bool needNameSpoof = !config.Spoof.name.empty();
    bool needLevelSpoof = (config.Spoof.lv > 0);

    // start calling old function
    lua_getglobal(L, STR("BattleResultLayer"));
    lua_getfield(L, -1, STR("oldsetPlayer"));
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    // replace arg1 if spoofing needed
    if (needNameSpoof || needLevelSpoof) {
        /** int len;
        lua_newtable(L);
        lua_getfield(L, 2, STR("name"));
        std::string newName;
        if (!needNameSpoof) {
            std::strcpy(newName.data(), lua_tolstring(L, -1, len));
        } else {
            newName = config.Spoof.name;
        }
        lua_pop(L, 1);
        lua_pushstring(L, il2cpp_string_new(newName.data()));
        lua_setfield(L, -2, STR("name"));
        lua_getfield(L, 2, STR("level"));
        std::string newLevel;
        if (!needLevelSpoof) {
            std::strcpy(newLevel.data(), lua_tolstring(L, -1, len));
        } else {
            newLevel = config.Spoof.lvInt;
        }
        lua_pop(L, 1);
        lua_pushstring(L, il2cpp_string_new(newLevel.data()));
        lua_setfield(L, -2, STR("level"));
        lua_getfield(L, 2, STR("exp"));
        std::string newXP;
        if (!needLevelSpoof) {
            std::strcpy(newXP.data(), lua_tolstring(L, -1, len));
        } else {
            newXP = config.Spoof.spoofXp;
        }
        lua_pop(L, 1);
        lua_pushstring(L, il2cpp_string_new(newLevel.data()));
        lua_setfield(L, -2, STR("exp"));
        lua_pcall(L, 2, 0, 0);

        // set player correctly
        lua_pushvalue(L, 2);
        lua_setfield(L, 1, STR("player"));
        **/
        // modified: safe usage with size_t and assign
        size_t len = 0;
        lua_newtable(L);

        // name
        lua_getfield(L, 2, STR("name"));
        const char *sname = lua_tolstring(L, -1, &len);
        std::string newName;
        if (!needNameSpoof) {
            if (sname) {
                if (len > 0) newName.assign(sname, len);
                else newName = sname;
            } else newName.clear();
        } else {
            newName = config.Spoof.name;
        }
        lua_pop(L, 1);
        //lua_pushstring(L, il2cpp_string_new(newName.c_str()));
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newName.c_str())));
        lua_setfield(L, -2, STR("name"));

        // level
        lua_getfield(L, 2, STR("level"));
        if (!needLevelSpoof) {
            const int levelNum = lua_tonumber(L, -1);
            std::string newLevel = std::to_string(levelNum);
            lua_pop(L, 1);
            //lua_pushstring(L, il2cpp_string_new(newLevel.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newLevel.c_str())));
            lua_setfield(L, -2, STR("level"));
        } else {
            std::string newLevel = std::to_string(config.Spoof.lvInt);
            lua_pop(L, 1);
            //lua_pushstring(L, il2cpp_string_new(newLevel.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newLevel.c_str())));
            lua_setfield(L, -2, STR("level"));
        }

        // exp
        lua_getfield(L, 2, STR("exp"));
        if (!needLevelSpoof) {
            size_t xplen = 0;
            const char *sxp = lua_tolstring(L, -1, &xplen);
            std::string newXP;
            if (sxp) {
                if (xplen > 0) newXP.assign(sxp, xplen);
                else newXP = sxp;
            } else newXP.clear();
            lua_pop(L, 1);
            //lua_pushstring(L, il2cpp_string_new(newXP.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newXP.c_str())));
            lua_setfield(L, -2, STR("exp"));
        } else {
            std::string newXP = std::to_string(config.Spoof.spoofXp);
            lua_pop(L, 1);
            //lua_pushstring(L, il2cpp_string_new(newXP.c_str()));
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newXP.c_str())));
            lua_setfield(L, -2, STR("exp"));
        }

        lua_pcall(L, 2, 0, 0);

        // set player correctly
        lua_pushvalue(L, 2);
        lua_setfield(L, 1, STR("player"));
    } else {
        lua_pushvalue(L, 2);
        lua_pcall(L, 2, 0, 0);
    }

    return 0;
}

void increaseXp(lua_State *L, int increment) {
    if (config.Spoof.lvInt == 200)
        return;

    config.Spoof.spoofXp += increment;

    if (config.Spoof.spoofXp >= config.Spoof.lvMax) {
        config.Spoof.spoofXp -= config.Spoof.lvMax;

        config.Spoof.lv = config.Spoof.lvInt + 1;
        parseLv(L, config.Spoof.lv);
    }

    config.Spoof.lvFrac = double(config.Spoof.spoofXp) / config.Spoof.lvMax;
}

int NBRSPExpManager(lua_State *L) {

    /** int len;
    std::string levelText;
    levelText.assign((char *)OBFUSCATE("Lv."));
    lua_getfield(L, lua_upvalueindex(3), STR("level"));
    levelText += std::string(lua_tolstring(L, -1, len));
    lua_pop(L, 1);
    ...
    expText += std::string(lua_tolstring(L, -1, len));
    **/
    size_t len = 0;
    std::string levelText;
    levelText.assign((char *)OBFUSCATE("Lv."));
    lua_getfield(L, lua_upvalueindex(3), STR("level"));
    const char *s = lua_tolstring(L, -1, &len);
    if (s) {
        if (len > 0) levelText.append(s, len);
        else levelText.append(s);
    }
    lua_pop(L, 1);
    lua_getfield(L, lua_upvalueindex(1), STR("playerLv"));
    //lua_pushstring(L, il2cpp_string_new(levelText.data()));
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>(levelText.data())));
    lua_setfield(L, -2, STR("text"));
    lua_pop(L, 1);

    lua_getglobal(L, STR("NewBattleResultUtil"));
    lua_getfield(L, -1, STR("GetPlayerExpOffset"));
    lua_remove(L, -2);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_pushvalue(L, lua_upvalueindex(3));
    lua_pcall(L, 2, 1, 0);

    std::string expText;
    expText.assign((char *)OBFUSCATE("+"));
    len = 0;
    s = lua_tolstring(L, -1, &len);
    if (s) {
        if (len > 0) expText.append(s, len);
        else expText.append(s);
    }
    lua_getfield(L, lua_upvalueindex(1), STR("playerExp"));
    //lua_pushstring(L, il2cpp_string_new(expText.data()));
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>(expText.data())));
    lua_setfield(L, -2, STR("text"));
    lua_pop(L, 1);

    lua_getfield(L, lua_upvalueindex(1), STR("playerExpLabel"));
    lua_pushstring(L, STR("EXP"));
    lua_setfield(L, -2, STR("text"));
    lua_pop(L, 1);

    lua_getfield(L, lua_upvalueindex(1), STR("playerExpBar"));
    lua_pushnumber(L, config.Spoof.lvFrac);
    lua_setfield(L, -2, STR("fillAmount"));

    return 0;
}

int wrapPRUIStaticFlush(lua_State *L) {

    // call original StaticFlush
    lua_getglobal(L, STR("PlayerResUI"));
    lua_getfield(L, -1, STR("oldStaticFlush"));
    for (int i = 1; i <= 6; i++) {
        lua_pushvalue(L, i);
    }
    lua_pcall(L, 6, 0, 0);

    if (config.Spoof.lv > 0) {
        std::string CoinMaxTxt = "MAX: " + std::to_string(config.Spoof.maxCoins);
        //lua_pushstring(L, il2cpp_string_new(CoinMaxTxt.data()));
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>(CoinMaxTxt.data())));
        lua_setfield(L, 2, STR("text"));

        std::string OilMaxTxt = "MAX: " + std::to_string(config.Spoof.maxOil);
        //lua_pushstring(L, il2cpp_string_new(OilMaxTxt.data()));
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>(OilMaxTxt.data())));
        lua_setfield(L, 4, STR("text"));
    }

    return 0;
}

int wrapNBRSPUpdatePlayer(lua_State *L) {

    bool needNameSpoof = !config.Spoof.name.empty();
    bool needLevelSpoof = (config.Spoof.lv > 0);

    // just call old function if no spoofs needed
    if (!needNameSpoof && !needLevelSpoof) {
        lua_getglobal(L, STR("NewBattleResultStatisticsPage"));
        lua_getfield(L, -1, STR("oldUpdatePlayer"));
        lua_remove(L, -2);
        lua_pushvalue(L, 1);
        lua_pcall(L, 1, 0, 0);

        return 0;
    }

    // otherwise proceed
    /** int len; **/
    size_t len = 0;

    // get old player object
    lua_getfield(L, 1, STR("contextData"));
    lua_getfield(L, -1, STR("oldPlayer"));
    lua_remove(L, -2);

    // get raw data of player
    lua_getglobal(L, STR("getProxy"));
    lua_getglobal(L, STR("PlayerProxy"));
    lua_pcall(L, 1, 1, 0);
    lua_getfield(L, -1, STR("getRawData"));
    lua_insert(L, -2);
    lua_pcall(L, 1, 1, 0);

    // construct spoofed player objects
    if (needLevelSpoof) {
        lua_newtable(L);

        lua_pushnumber(L, config.Spoof.lvInt);
        lua_setfield(L, -2, STR("level"));

        lua_pushnumber(L, config.Spoof.spoofXp);
        lua_setfield(L, -2, STR("exp"));

        lua_getglobal(L, STR("NewBattleResultUtil"));
        lua_getfield(L, -1, STR("GetPlayerExpOffset"));
        lua_remove(L, -2);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_pcall(L, 2, 1, 0);
        int xpDiff = lua_tonumber(L, -1);
        lua_pop(L, 1);
        increaseXp(L, xpDiff);

        lua_newtable(L);

        lua_pushnumber(L, config.Spoof.lvInt);
        lua_setfield(L, -2, STR("level"));

        lua_pushnumber(L, config.Spoof.spoofXp);
        lua_setfield(L, -2, STR("exp"));
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    // spoof name
    lua_getfield(L, -1, STR("GetName"));
    lua_pushvalue(L, -2);
    lua_pcall(L, 1, 1, 0);
    std::string newName;
    if (!needNameSpoof) {
        /** newName.assign(lua_tolstring(L, -1, len)); **/
        len = 0;
        const char *s = lua_tolstring(L, -1, &len);
        if (s) {
            if (len > 0) newName.assign(s, len);
            else newName = s;
        } else newName.clear();
    } else {
        newName = config.Spoof.name;
    }
    lua_pop(L, 1);
    lua_getfield(L, 1, STR("playerName"));
    //lua_pushstring(L, il2cpp_string_new(newName.data()));
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>(newName.data())));
    lua_setfield(L, -2, STR("text"));
    lua_pop(L, 1);

    // prepare upvalues for experience manager
    lua_pushvalue(L, 1);
    if (!needLevelSpoof) {
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
    } else {
        lua_pushvalue(L, 4);
        lua_pushvalue(L, 5);
    }

    // push experience manager as a closure
    lua_pushcclosure(L, NBRSPExpManager, 3);

    lua_getfield(L, 1, STR("contextData"));
    lua_getfield(L, -1, STR("autoSkipFlag"));
    bool autoSkipFlag = (bool)lua_toboolean(L, -1);
    lua_pop(L, 2);

    if (!autoSkipFlag) {
        lua_getglobal(L, STR("NewBattleResultPlayerAniamtion"));
        lua_getfield(L, -1, STR("New"));
        lua_remove(L, -2);
        lua_getfield(L, 1, STR("playerLv"));
        lua_getfield(L, 1, STR("playerExp"));
        lua_getfield(L, 1, STR("playerExpBar"));
        if (needLevelSpoof) {
            lua_pushvalue(L, 5);
            lua_pushvalue(L, 4);
        } else {
            lua_pushvalue(L, 3);
            lua_pushvalue(L, 2);
        }
        lua_pcall(L, 5, 1, 0);

        lua_getfield(L, -1, STR("SetUp"));
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -4);
        lua_pcall(L, 2, 0, 0);

        lua_setfield(L, 1, STR("playerAniamtion"));

        lua_pop(L, 1);
    } else {
        lua_pcall(L, 0, 0, 0);
    }

    return 0;
}

int wrapNDRSPUpdatePlayer(lua_State *L) {
    bool needNameSpoof = !config.Spoof.name.empty();

    if (!needNameSpoof) {
        lua_getglobal(L, STR("NewDuelResultStatisticsPage"));
        lua_getfield(L, -1, STR("oldUpdatePlayer"));
        lua_remove(L, -2);
        lua_pushvalue(L, 1);
        lua_pcall(L, 1, 0, 0);
        return 0;
    }

    enum { lc_nformalargs = 1 };
    lua_settop(L, 1);

    lua_pushstring(L, stdstr2ilstr(config.Spoof.name));
    lua_pushstring(L, STR("playerName"));
    lua_gettable(L, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("text"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 1);

    /* local var1_1 = getProxy(MilitaryExerciseProxy):getSeasonInfo() */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("getProxy"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("MilitaryExerciseProxy"));
    lua_call(L, 1, 1);
    lua_pushstring(L, STR("getSeasonInfo"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    assert(lua_gettop(L) == 2);

    /* local var2_1 = SeasonInfo.getMilitaryRank(var1_1.score, var1_1.rank) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SeasonInfo"));
    lua_pushstring(L, STR("getMilitaryRank"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("score"));
    lua_gettable(L, 2);
    lua_pushstring(L, STR("rank"));
    lua_gettable(L, 2);
    lua_call(L, 2, 1);
    assert(lua_gettop(L) == 3);

    /* local var3_1, var4_1 = SeasonInfo.getNextMilitaryRank(var1_1.score, var1_1.rank) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("SeasonInfo"));
    lua_pushstring(L, STR("getNextMilitaryRank"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, STR("score"));
    lua_gettable(L, 2);
    lua_pushstring(L, STR("rank"));
    lua_gettable(L, 2);
    lua_call(L, 2, 2);
    assert(lua_gettop(L) == 5);

    /* arg0_1.playerLv.text = var2_1.name */
    lua_pushstring(L, STR("name"));
    lua_gettable(L, 3);
    lua_pushstring(L, STR("playerLv"));
    lua_gettable(L, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("text"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 5);

    /* arg0_1.playerExpLabel.text = i18n(STR("word_rankScore")) */
    lua_getfield(L, LUA_GLOBALSINDEX, STR("i18n"));
    lua_pushstring(L, STR("word_rankScore"));
    lua_call(L, 1, 1);
    lua_pushstring(L, STR("playerExpLabel"));
    lua_gettable(L, 1);
    lua_insert(L, -2);
    lua_pushstring(L, STR("text"));
    lua_insert(L, -2);
    lua_settable(L, -3);
    lua_pop(L, 1);
    assert(lua_gettop(L) == 5);

    /* if not arg0_1.contextData.autoSkipFlag then */
    enum { lc2 = 5 };
    lua_pushstring(L, STR("contextData"));
    lua_gettable(L, 1);
    lua_pushstring(L, STR("autoSkipFlag"));
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_pushboolean(L, !(lua_toboolean(L, -1)));
    lua_remove(L, -2);
    const int lc3 = lua_toboolean(L, -1);
    lua_pop(L, 1);
    if (lc3) {

        /* arg0_1.duelAniamtion = NewBattleResultDuelAniamtion.New(arg0_1.playerExp, arg0_1.playerExpBar, var4_1, arg0_1.contextData.oldRank, var1_1) */
        lua_getfield(L, LUA_GLOBALSINDEX, STR("NewBattleResultDuelAniamtion"));
        lua_pushstring(L, STR("New"));
        lua_gettable(L, -2);
        lua_remove(L, -2);
        lua_pushstring(L, STR("playerExp"));
        lua_gettable(L, 1);
        lua_pushstring(L, STR("playerExpBar"));
        lua_gettable(L, 1);
        lua_pushvalue(L, 5);
        lua_pushstring(L, STR("contextData"));
        lua_gettable(L, 1);
        lua_pushstring(L, STR("oldRank"));
        lua_gettable(L, -2);
        lua_remove(L, -2);
        lua_pushvalue(L, 2);
        lua_call(L, 5, 1);
        lua_pushstring(L, STR("duelAniamtion"));
        lua_insert(L, -2);
        lua_settable(L, 1);
        assert(lua_gettop(L) == 5);

        /* arg0_1.duelAniamtion:SetUp(var5_1) */
        lua_pushstring(L, STR("duelAniamtion"));
        lua_gettable(L, 1);
        lua_pushstring(L, STR("SetUp"));
        lua_gettable(L, -2);
        lua_insert(L, -2);
        lua_getfield(L, LUA_GLOBALSINDEX, STR("var5_1"));
        lua_call(L, 2, 0);
        assert(lua_gettop(L) == 5);
    } else {
        lua_pushstring(L, STR("+"));
        lua_getfield(L, LUA_GLOBALSINDEX, STR("NewBattleResultUtil"));
        lua_pushstring(L, STR("GetSeasonScoreOffset"));
        lua_gettable(L, -2);
        lua_remove(L, -2);
        lua_pushstring(L, STR("contextData"));
        lua_gettable(L, 1);
        lua_pushstring(L, STR("oldRank"));
        lua_gettable(L, -2);
        lua_remove(L, -2);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 1);
        lua_concat(L, 2);
        lua_pushstring(L, STR("playerExp"));
        lua_gettable(L, 1);
        lua_insert(L, -2);
        lua_pushstring(L, STR("text"));
        lua_insert(L, -2);
        lua_settable(L, -3);
        lua_pop(L, 1);
        assert(lua_gettop(L) == 5);

        /* arg0_1.playerExpBar.fillAmount = var1_1.score / var4_1 */
        lua_pushstring(L, STR("score"));
        lua_gettable(L, 2);
        lc_div(L, -1, 5);
        lua_remove(L, -2);
        lua_pushstring(L, STR("playerExpBar"));
        lua_gettable(L, 1);
        lua_insert(L, -2);
        lua_pushstring(L, STR("fillAmount"));
        lua_insert(L, -2);
        lua_settable(L, -3);
        lua_pop(L, 1);
        assert(lua_gettop(L) == 5);
    }
    lua_settop(L, lc2);
    assert(lua_gettop(L) == 5);
    return 0;
}

std::string getRealName(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, STR("getProxy"));
    lua_getfield(L, LUA_GLOBALSINDEX, STR("PlayerProxy"));
    lua_call(L, 1, 1);
    lua_pushstring(L, STR("getRawData"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    lua_pushstring(L, STR("GetName"));
    lua_gettable(L, -2);
    lua_insert(L, -2);
    lua_call(L, 1, 1);
    /** int siz;
    std::string name = lua_tolstring(L, -1, siz);
    lua_pop(L, 1);
    return name; **/
    size_t siz = 0;
    const char *s = lua_tolstring(L, -1, &siz);
    std::string name;
    if (s) {
        if (siz > 0) name.assign(s, siz);
        else name = s;
    } else {
        name.clear();
    }
    lua_pop(L, 1);
    return name;
}


int wrapRCupdate(lua_State *L) {
    bool needNameSpoof = !config.Spoof.name.empty();
    bool needLevelSpoof = (config.Spoof.lv > 0);
    
    int type;
    /** int siz; **/
    size_t siz;
    std::string name;
    
    lua_getglobal(L, STR("RankCard"));
    lua_getfield(L, -1, STR("oldupdate"));
    lua_remove(L, -2);

    lua_pushvalue(L, 1);
    lua_getfield(L, -1, STR("_type"));
    type = (int) lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_pushvalue(L, 2);
    lua_getfield(L, -1, STR("name"));
    /** name = lua_tolstring(L, -1, siz); **/
    siz = 0;
    const char *s = lua_tolstring(L, -1, &siz);
    if (s) {
        if (siz > 0) name.assign(s, siz);
        else name = s;
    } else {
        name.clear();
    }
    lua_pop(L, 1);
    
    percyLog(OBFUSCATE("name: %s, type: %d"), name.data(), type);
    if (type == 1 || name == getRealName(L)) {
        if (needNameSpoof) {
            lua_pushstring(L, stdstr2ilstr(config.Spoof.name));
            lua_setfield(L, -2, STR("name"));
        }
        if (needLevelSpoof) {
            lua_pushstring(L, stdstr2ilstr(std::to_string(config.Spoof.lvInt)));
            lua_setfield(L, -2, STR("lv"));
        }
    }

    lua_pushvalue(L, 3);
    lua_pcall(L, 3, 0, 0);
    return 0;
}

void modSpoof(lua_State *L) {
    luaHookFunc(L, OBFUSCATE("PlayerVitaeDetailPage.UpdateInfo"), wrapPVDPUpdateInfo, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("MainPlayerInfoBtn.UpdateLevelAndName"), wrapMPIBUpdateLevelAndName, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("MainPlayerInfoBtn.UpdateExp"), wrapMPIBUpdateExp, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("MainPlayerInfoBtn4Mellow.UpdateExp"), wrapNMPIBUpdateExp, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("ys.Battle.BattleDuelDamageRateView.SetFleetVO"), wrapBDDRVSetFleetVO, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("PlayerResUI.StaticFlush"), wrapPRUIStaticFlush, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("NewBattleResultStatisticsPage.UpdatePlayer"), wrapNBRSPUpdatePlayer, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("NewDuelResultStatisticsPage.UpdatePlayer"), wrapNDRSPUpdatePlayer, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("BattleResultLayer.setPlayer"), wrapBRLSetPlayer, OBFUSCATE("old"));
    luaHookFunc(L, OBFUSCATE("RankCard.update"), wrapRCupdate, OBFUSCATE("old"));
}
