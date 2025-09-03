#include <android/log.h>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <jni.h>
#include <list>
#include <map>
#include <sstream>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include "Includes/Logger.h"
#include "Includes/Toast.hpp"
#include "Includes/Utils.h"

#include "Includes/Macros.h"
#include "Includes/json.hpp"
#include "Structs.h"
#include "hooks/combatloadui.hpp"
#include "hooks/fragresolvepanel.hpp"
#include "modules/command.hpp"
#include "modules/spoof.hpp"

// Target lib here
#define targetLibName "libil2cpp.so"
#define GETLUAFUNC(method) getFunctionAddress("LuaInterface", "LuaDLL", method)

using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

bool exec = false;
std::string configPath;
std::string skinPath;
std::map<int, int> skins;
Config config;
Il2CppImage *image;

const char *lua_tolstring(lua_State *instance, int index, int &strLen);
Il2CppString *stdstr2ilstr(std::string s) { return il2cpp_string_new((char *)s.c_str()); }

void crash() {
    abort();             // std posix
    *((char *)-1) = 'x'; // redis
    int *p = (int *)-1;
    *p = 1; // random
}

void percyLog(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_VERBOSE, "Perseus", fmt, arg);
    va_end(arg);
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

bool writeConfigFile() {
    std::ofstream configFile(configPath);

    if (configFile.is_open()) {
        configFile << _default.dump(2) << "\n";
        configFile.close();
        return true;
    }
    return false;
}

bool writeSkinsFile() {
    std::ofstream skinsFile(skinPath);

    if (skinsFile.is_open()) {
        for (auto const &kv : skins) {
            skinsFile << kv.first << ":" << kv.second << "\n";
        }
        skinsFile.close();
        return true;
    }
    return false;
}

bool readSkinsFile() {
    std::vector<std::string> fileLines;
    std::string l;
    std::ifstream skinsFile(skinPath);

    if (!skinsFile.is_open()) {
        return false;
    }

    while (getline(skinsFile, l)) {
        if (l.length() < 3) {
            continue;
        }
        fileLines.push_back(l);
    }
    skinsFile.close();

    for (const std::string &line : fileLines) {
        std::vector<std::string> splitLines = split(line, ':');
        int shipId = std::stoi(splitLines[0]);
        int skinId = std::stoi(splitLines[1]);
        skins.insert(std::pair<int, int>(shipId, skinId));
    }

    return true;
}

bool isEnabled(const std::string &param) { return param != "false"; }

bool isEnabled(const int param) { return param != -1; }

int getValue(const std::string &param) { return std::stoi(param); }

int getValue(const int param) { return param; }

void checkHeader(const std::string &line, const std::string &header) {
    if (line != header) {
        config.Valid = false;
    }
}

bool getKeyEnabled(const std::string &line, const std::string &key) {
    std::vector<std::string> splitLine = split(line, '=');
    std::string value = splitLine[1];
    if (value != "true" && value != "false" || splitLine[0] != key) {
        config.Valid = false;
        return false;
    }
    return isEnabled(value);
}

std::string getKeyValue(const std::string &line, const std::string &key) {
    std::vector<std::string> splitLine = split(line, '=');
    std::string value = splitLine[1];
    try {
        if (splitLine[0] != key) {
            throw 1;
        }
        if (value == "false") {
            return value;
        }
        if (getValue(value) < 0) {
            throw 1;
        }
    } catch (...) {
        config.Valid = false;
        return "false";
    }
    return value;
}

void loadil2cppfuncs() {
    // populate all il2cpp functions
    il2cpp_domain_get = (Il2CppDomain * (*)()) GETSYM(targetLibName, "il2cpp_domain_get");
    il2cpp_domain_assembly_open = (Il2CppAssembly * (*)(void *, char *)) GETSYM(targetLibName, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (Il2CppImage * (*)(void *)) GETSYM(targetLibName, "il2cpp_assembly_get_image");
    il2cpp_class_from_name = (void *(*)(void *, char *, char *))GETSYM(targetLibName, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (MethodInfo * (*)(void *, char *, int)) GETSYM(targetLibName, "il2cpp_class_get_method_from_name");
    il2cpp_string_new = (Il2CppString * (*)(char *)) GETSYM(targetLibName, "il2cpp_string_new");

    // call the functions necessary to get the image
    Il2CppDomain *domain = il2cpp_domain_get();
    Il2CppAssembly *assembly = il2cpp_domain_assembly_open(domain, "Assembly-CSharp");
    image = il2cpp_assembly_get_image(assembly);
}

Il2CppMethodPointer *getFunctionAddress(char *namespaze, char *klass, char *method) {
    void *iklass = il2cpp_class_from_name(image, namespaze, klass);
    MethodInfo *imethod = il2cpp_class_get_method_from_name(iklass, method, -1);
    return imethod->methodPointer;
}

void loadluafuncs() {
    // populate lua funcs
    lua_newthread = (lua_State * (*)(lua_State *)) GETLUAFUNC("lua_newthread");
    lua_getfield = (void (*)(lua_State *, int, Il2CppString *))GETLUAFUNC("lua_getfield");
    lua_gettable = (void (*)(lua_State *, int))GETLUAFUNC("lua_gettable");
    lua_setfield = (void (*)(lua_State *, int, Il2CppString *))GETLUAFUNC("lua_setfield");
    lua_objlen = (size_t(*)(lua_State *, int))GETLUAFUNC("lua_objlen");
    lua_pushnil = (void (*)(lua_State *))GETLUAFUNC("lua_pushnil");
    lua_createtable = (void (*)(lua_State *, int, int))GETLUAFUNC("lua_createtable");
    lua_pushnumber = (void (*)(lua_State *, double))GETLUAFUNC("lua_pushnumber");
    lua_pushboolean = (void (*)(lua_State *, int))GETLUAFUNC("lua_pushboolean");
    lua_settop = (void (*)(lua_State *, int))GETLUAFUNC("lua_settop");
    lua_next = (int (*)(lua_State *, int))GETLUAFUNC("lua_next");
    lua_tonumber = (double (*)(lua_State *, int))GETLUAFUNC("lua_tonumber");
    lua_type = (int (*)(lua_State *, int))GETLUAFUNC("lua_type");
    lua_pushcclosure = (void (*)(lua_State *, lua_CFunction, int))GETLUAFUNC("lua_pushcclosure");
    lua_pcall = (int (*)(lua_State *, int, int, int))GETLUAFUNC("lua_pcall");
    lua_call = (void (*)(lua_State *, int, int))GETLUAFUNC("lua_call");
    lua_insert = (void (*)(lua_State *, int))GETLUAFUNC("lua_insert");
    lua_pushvalue = (void (*)(lua_State *, int))GETLUAFUNC("lua_pushvalue");
    lua_pushstring = (void (*)(lua_State *, Il2CppString *))GETLUAFUNC("lua_pushstring");
    lua_remove = (void (*)(lua_State *, int))GETLUAFUNC("lua_remove");
    lua_gettop = (int (*)(lua_State *))GETLUAFUNC("lua_gettop");
    lua_settable = (void (*)(lua_State *, int))GETLUAFUNC("lua_settable");
    lua_rawseti = (void (*)(lua_State *, int, int))GETLUAFUNC("lua_rawseti");
    lua_rawgeti = (void (*)(lua_State *, int, int))GETLUAFUNC("lua_rawgeti");
    lua_rawset = (void (*)(lua_State *, int))GETLUAFUNC("lua_rawset");
    lua_rawget = (void (*)(lua_State *, int))GETLUAFUNC("lua_rawget");
    lua_setupvalue = (const char *(*)(lua_State *, int, int))GETLUAFUNC("lua_setupvalue");
    lua_equal = (int (*)(lua_State *, int, int))GETLUAFUNC("lua_equal");
    lua_toboolean = (int (*)(lua_State *, int))GETLUAFUNC("lua_toboolean");
    lua_lessthan = (int (*)(lua_State *, int, int))GETLUAFUNC("lua_lessthan");
    lua_replace = (void (*)(lua_State *, int))GETLUAFUNC("lua_replace");
    lua_concat = (void (*)(lua_State *, int))GETLUAFUNC("lua_concat");
    lua_isnumber = (int (*)(lua_State *, int))GETLUAFUNC("lua_isnumber");
    lua_checkstack = (int (*)(lua_State *, int))GETLUAFUNC("lua_checkstack");

    luaL_getmetafield = (int (*)(lua_State *, int, Il2CppString *))GETLUAFUNC("luaL_getmetafield");
}

void replaceAttributeN(lua_State *L, Il2CppString *attribute, int number) {
    lua_pushnumber(L, number);
    lua_setfield(L, -2, attribute);
}

void emptyAttributeT(lua_State *L, Il2CppString *attribute) {
    lua_newtable(L);
    lua_setfield(L, -2, attribute);
}

std::vector<int> getTableIds(lua_State *L) {
    std::vector<int> tableIds;

    // get table "all", where all ids are stored
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("all")));

    // loop it
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        tableIds.push_back((int)lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    // pop "all" table
    lua_pop(L, 1);
    return tableIds;
}

void modAircraft(lua_State *L) {
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("aircraft_template")));

    std::vector<int> aircraft = getTableIds(L);

    for (const int &aircraftId : aircraft) {
        lua_pushnumber(L, aircraftId);
        lua_gettable(L, -2);

        if (isEnabled(config.Aircraft.Accuracy)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("accuracy")), getValue(config.Aircraft.Accuracy));
        }
        if (isEnabled(config.Aircraft.AccuracyGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("ACC_growth")), getValue(config.Aircraft.AccuracyGrowth));
        }
        if (isEnabled(config.Aircraft.AttackPower)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("attack_power")), getValue(config.Aircraft.AttackPower));
        }
        if (isEnabled(config.Aircraft.AttackPowerGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("AP_growth")), getValue(config.Aircraft.AttackPowerGrowth));
        }
        if (isEnabled(config.Aircraft.CrashDamage)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("crash_DMG")), getValue(config.Aircraft.CrashDamage));
        }
        if (isEnabled(config.Aircraft.Hp)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("max_hp")), getValue(config.Aircraft.Hp));
        }
        if (isEnabled(config.Aircraft.HpGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("hp_growth")), getValue(config.Aircraft.HpGrowth));
        }
        if (isEnabled(config.Aircraft.Speed)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("speed")), getValue(config.Aircraft.Speed));
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

int hookBUAddBuff(lua_State *L);

void modEnemies(lua_State *L) {
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("enemy_data_statistics")));

    std::vector<int> enemyShips = getTableIds(L);

    for (const int &enemyId : enemyShips) {
        lua_pushnumber(L, enemyId);
        lua_gettable(L, -2);

        if (isEnabled(config.Enemies.AntiAir)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("antiaircraft")), getValue(config.Enemies.AntiAir));
        }
        if (isEnabled(config.Enemies.AntiAirGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("antiaircraft_growth")), getValue(config.Enemies.AntiAirGrowth));
        }
        if (isEnabled(config.Enemies.AntiSubmarine)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("antisub")), getValue(config.Enemies.AntiSubmarine));
        }
        if (isEnabled(config.Enemies.Armor)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("armor")), getValue(config.Enemies.Armor));
        }
        if (isEnabled(config.Enemies.ArmorGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("armor_growth")), getValue(config.Enemies.ArmorGrowth));
        }
        if (isEnabled(config.Enemies.Cannon)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("cannon")), getValue(config.Enemies.Cannon));
        }
        if (isEnabled(config.Enemies.CannonGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("cannon_growth")), getValue(config.Enemies.CannonGrowth));
        }
        if (isEnabled(config.Enemies.Evasion)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("dodge")), getValue(config.Enemies.Evasion));
        }
        if (isEnabled(config.Enemies.EvasionGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("dodge_growth")), getValue(config.Enemies.EvasionGrowth));
        }
        if (isEnabled(config.Enemies.Hit)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("hit")), getValue(config.Enemies.Hit));
        }
        if (isEnabled(config.Enemies.HitGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("hit_growth")), getValue(config.Enemies.HitGrowth));
        }
        if (isEnabled(config.Enemies.Hp)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("durability")), getValue(config.Enemies.Hp));
        }
        if (isEnabled(config.Enemies.HpGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("durability_growth")), getValue(config.Enemies.HpGrowth));
        }
        if (isEnabled(config.Enemies.Luck)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("luck")), getValue(config.Enemies.Luck));
        }
        if (isEnabled(config.Enemies.LuckGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("luck_growth")), getValue(config.Enemies.LuckGrowth));
        }
        if (isEnabled(config.Enemies.Reload)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("reload")), getValue(config.Enemies.Reload));
        }
        if (isEnabled(config.Enemies.ReloadGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("reload_growth")), getValue(config.Enemies.ReloadGrowth));
        }
        if (config.Enemies.RemoveEquipment) {
            emptyAttributeT(L, il2cpp_string_new(const_cast<char*>("equipment_list")));
        }
        if (isEnabled(config.Enemies.Speed)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("speed")), getValue(config.Enemies.Speed));
        }
        if (isEnabled(config.Enemies.SpeedGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("speed_growth")), getValue(config.Enemies.SpeedGrowth));
        }
        if (isEnabled(config.Enemies.Torpedo)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("torpedo")), getValue(config.Enemies.Torpedo));
        }
        if (isEnabled(config.Enemies.TorpedoGrowth)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("torpedo_growth")), getValue(config.Enemies.TorpedoGrowth));
        }

        lua_pop(L, 1);
    }

    // pop enemy_data_statistics
    lua_pop(L, 1);

    if (config.Enemies.RemoveSkill) {
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("enemy_data_skill")));

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_istable(L, -1)) {
                emptyAttributeT(L, il2cpp_string_new(const_cast<char*>("skill_list")));
                replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("is_repeat")), 0);
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }
    if (config.Enemies.RemoveBuffs) {
        lua_getglobal(L, il2cpp_string_new(const_cast<char*>("ys")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("Battle")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("BattleUnit")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("AddBuff")));
        lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("oldAddBuff")));
        lua_pushcfunction(L, hookBUAddBuff);
        lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("AddBuff")));
        lua_pop(L, 3);
    }
}

int nilFunc(lua_State *L) { return 0; }

int trueFunc(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

void lc_add(lua_State *L, int idxa, int idxb) {
    if (lua_isnumber(L, idxa) && lua_isnumber(L, idxb)) {
        lua_pushnumber(L, lua_tonumber(L, idxa) + lua_tonumber(L, idxb));
    } else {
        if (luaL_getmetafield(L, idxa, il2cpp_string_new(const_cast<char*>("__add"))) || luaL_getmetafield(L, idxb, il2cpp_string_new(const_cast<char*>("__add")))) {
            lua_pushvalue(L, idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa - 1 : idxa);
            lua_pushvalue(L, idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb - 2 : idxb);
            lua_call(L, 2, 1);
        } else {
        }
    }
}

void lc_sub(lua_State *L, int idxa, int idxb) {
    if (lua_isnumber(L, idxa) && lua_isnumber(L, idxb)) {
        lua_pushnumber(L, lua_tonumber(L, idxa) - lua_tonumber(L, idxb));
    } else {
        if (luaL_getmetafield(L, idxa, il2cpp_string_new(const_cast<char*>("__sub"))) || luaL_getmetafield(L, idxb, il2cpp_string_new(const_cast<char*>("__sub")))) {
            lua_pushvalue(L, idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa - 1 : idxa);
            lua_pushvalue(L, idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb - 2 : idxb);
            lua_call(L, 2, 1);
        } else {
            // luaL_error(L, "attempt to perform arithmetic");
        }
    }
}

void lc_newclosuretable(lua_State *L, int idx) {
    lua_newtable(L);
    lua_pushvalue(L, idx);
    lua_rawseti(L, -2, 0);
}

void lc_getupvalue(lua_State *L, int tidx, int level, int varid) {
    if (level == 0) {
        lua_rawgeti(L, tidx, varid);
    } else {
        lua_pushvalue(L, tidx);
        while (--level >= 0) {
            lua_rawgeti(L, tidx, 0); /* 0 links to parent table */
            lua_remove(L, -2);
            tidx = -1;
        }
        lua_rawgeti(L, -1, varid);
        lua_remove(L, -2);
    }
}

void lc_div(lua_State *L, int idxa, int idxb) {
    if (lua_isnumber(L, idxa) && lua_isnumber(L, idxb)) {
        lua_pushnumber(L, lua_tonumber(L, idxa) / lua_tonumber(L, idxb));
    } else {
        if (luaL_getmetafield(L, idxa, il2cpp_string_new(const_cast<char*>("__div"))) || luaL_getmetafield(L, idxb, il2cpp_string_new(const_cast<char*>("__div")))) {
            lua_pushvalue(L, idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa - 1 : idxa);
            lua_pushvalue(L, idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb - 2 : idxb);
            lua_call(L, 2, 1);
        } else {
            // luaL_error(L, "attempt to perform arithmetic");
        }
    }
}

int hookSetShipSkinCommand(lua_State *L);

int hookSFVSetSkinList(lua_State *L);

int wrapShipCtor(lua_State *L);

int hookCommitCombat(lua_State *L);

int hookCommitTrybat(lua_State *L);

void modSkins(lua_State *L) {
    // replace SetShipSkinCommand.execute with its hook
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("SetShipSkinCommand")));
    lua_pushcfunction(L, hookSetShipSkinCommand);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("execute")));
    lua_pop(L, 1);

    // replace ShipFashionView.SetSkinList with its hook
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("ShipFashionView")));
    lua_pushcfunction(L, hookSFVSetSkinList);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("SetSkinList")));
    lua_pop(L, 1);

    // rename Ship's New function (its Ctor) to oldCtor
    // and set New to wrapShipCtor.
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("Ship")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("New")));
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("oldCtor")));
    lua_pushcfunction(L, wrapShipCtor);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("New")));
    lua_pop(L, 1);

    // replace ShipSkin's SKIN_TYPE_NOT_HAVE_HIDE (by default 4) to 10,
    // so all its comparisons pass through, thus making "censored" skins visible
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("ShipSkin")));
    replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("SKIN_TYPE_NOT_HAVE_HIDE")), 10);
    lua_pop(L, 1);

    if (!readSkinsFile()) {
        return;
    }
}

void luaMessageBox(lua_State *L, std::string msg) {
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("MsgboxMgr")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("GetInstance")));
    lua_pcall(L, 0, 1, 0);
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ShowMsgBox")));
    lua_pushvalue(L, -2);

    lua_newtable(L);

    lua_pushstring(L, il2cpp_string_new(const_cast<char*>("type")));
    lua_pushnumber(L, 7); // helpmsg
    lua_settable(L, -3);

    // helps is of type array of table with key `info` in each table
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>("helps")));
    lua_createtable(L, 1, 0);
    lua_newtable(L);
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>("info")));
    lua_pushstring(L, stdstr2ilstr(msg));
    lua_settable(L, -3);
    lua_rawseti(L, -2, 1);
    lua_settable(L, -3);

    lua_pcall(L, 2, 0, 0);
}

void luaToast(lua_State *L, std::string msg) {
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("TipsMgr")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("GetInstance")));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pcall(L, 0, 1, 0);

    // :ShowTips(slot0, string)
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ShowTips")));
    lua_insert(L, -2);
    lua_pushstring(L, stdstr2ilstr(msg));
    lua_pcall(L, 2, 0, 0);
}

int hookSendMsgExecute(lua_State *L) {
    lua_getfield(L, 2, il2cpp_string_new(const_cast<char*>("getBody")));
    lua_pushvalue(L, 2);
    lua_pcall(L, 1, 1, 0);

    int siz = 0;
    std::string msg(lua_tolstring(L, -1, siz));
    percyLog("chatmsg: %s", msg.c_str());

    std::string prefix(".");
    if (!msg.compare(0, prefix.size(), prefix)) {
        std::string cmd = msg.substr(1);
        handleCommand(L, cmd);
    } else {
        // TODO: maybe call the old function
    }
    return 0;
}

void luaHookFunc(lua_State *L, std::string field, lua_CFunction func, std::string backup_prefix) {
    // place name of function to be hooked, in the form x.y.z, in a stringstream
    std::istringstream luaName(field);

    std::string luaObj;
    int luaObjCount = 1;

    std::getline(luaName, luaObj, '.');
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>(luaObj.c_str())));

    while (std::getline(luaName, luaObj, '.')) {
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>(luaObj.c_str())));
        luaObjCount++;
        if (!luaName.peek())
            break;
    }

    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>((backup_prefix + luaObj).c_str())));

    lua_pushcfunction(L, func);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>(luaObj.c_str())));
    lua_pop(L, luaObjCount - 1);
}

template <typename T> void luaReplaceAttribute(lua_State *L, const char *key, T val) {
    std::vector<std::string> path = split(std::string(key), '.');

    lua_getglobal(L, stdstr2ilstr(path.front()));

    for (size_t i = 1; i < path.size() - 1; ++i)
        lua_getfield(L, -1, stdstr2ilstr(path[i]));

    if constexpr (std::is_same_v<T, int>) {
        lua_pushnumber(L, (int)val);
    } else if constexpr (std::is_same_v<T, const char *>) {
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>((char *)val)));
    }

    lua_setfield(L, -2, stdstr2ilstr(path.back()));
    lua_pop(L, path.size() - 1);
}

int hookBMWABSetActive(lua_State *L) {
    // get old SetActive function and call it with the same arguments, but with
    // arg2 set to false instead, such that battleship bullet time is not activated
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("ys")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("Battle")));
    lua_remove(L, -2);
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("BattleManualWeaponAutoBot")));
    lua_remove(L, -2);
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("oldSetActive")));
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pushboolean(L, 0);
    lua_pcall(L, 3, 0, 0);
    lua_pop(L, 3);

    return 0;
}

int ship150morale(lua_State *L) {
    lua_pop(L, 1);
    lua_pushnumber(L, 150);
    return 1;
}

// str, options
int wvHook(lua_State *L) {
    int nstack = lua_gettop(L);
    if (nstack == 1) {
        lua_pushnumber(L, 0);
        return 1;
    } else {
        lua_pushnumber(L, 0);
        lua_pushvalue(L, 1);
        return 2;
    }
}

void modMisc(lua_State *L) {
    lua_pushcfunction(L, wvHook);
    lua_setglobal(L, il2cpp_string_new(const_cast<char*>("wordVer")));
    
    if (config.Misc.ExerciseGodmode) {
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ConvertedBuff")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("buff_19")));
        lua_pushnumber(L, 0);
        lua_gettable(L, -2);
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("effect_list")));
        lua_pushnumber(L, 1);
        lua_gettable(L, -2);
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("arg_list")));
        replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("number")), -1);
        lua_pop(L, 6);
    }

    if (config.Misc.FastStageMovement) {
        luaReplaceAttribute<int>(L, "ChapterConst.ShipStepDuration", 0);
        luaReplaceAttribute<int>(L, "ChapterConst.ShipStepQuickPlayScale", 0);
    }

    if (config.Misc.Skins) {
        modSkins(L);
    }

    if (config.Misc.ChatCommands) {
        luaReplaceAttribute<const char *>(L, "pg.gametip.notice_input_desc.tip", "chat/.command");
        luaHookFunc(L, "SendMsgCommand.execute", hookSendMsgExecute, "old_");
        luaHookFunc(L, "GuildSendMsgCommand.execute", nilFunc, "old_");
    }

    if (config.Misc.RemoveHardModeStatLimit) {
        luaHookFunc(L, "WorldFleetSelectLayer.CheckValid", trueFunc, "old");
        luaHookFunc(L, "BossSingleBattleFleetSelectSubPanel.CheckValid", trueFunc, "old");
        luaHookFunc(L, "Chapter.IsEliteFleetLegal", trueFunc, "old");
    }

    if (config.Misc.RemoveNSFWArts) {
        luaHookFunc(L, "CombatLoadUI.init", CLUInitHook, "old");
    }

    if (config.Misc.AllBlueprintsConvertible) {
        luaHookFunc(L, "FragResolvePanel.GetAllBluePrintStrengthenItems", FRPGetAllBlueprintItemsHook, "old");
    }

    // thanks cmdtaves for the following patches
    if (config.Misc.RemoveBBAnimation) {
        luaHookFunc(L, "ys.Battle.BattleManualWeaponAutoBot.SetActive", hookBMWABSetActive, "old");
    }

    if (config.Misc.RemoveMoraleWarning) {
        luaHookFunc(L, "Ship.cosumeEnergy", nilFunc, "old_");
        luaHookFunc(L, "Ship.getEnergy", ship150morale, "old_");
    }

    if (isEnabled(config.Misc.AutoRepeatLimit)) {
        luaReplaceAttribute<int>(L, "pg.gameset.main_level_multiple_sorties_times.key_value", config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, "pg.gameset.hard_level_multiple_sorties_times.key_value", config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, "pg.gameset.activity_level_multiple_sorties_times.key_value", config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, "pg.gameset.archives_level_multiple_sorties_times.key_value", config.Misc.AutoRepeatLimit);
    }
}

void modWeapons(lua_State *L) {
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("weapon_property")));

    std::vector<int> weapons = getTableIds(L);

    for (int weaponId : weapons) {
        lua_pushnumber(L, weaponId);
        lua_gettable(L, -2);

        if (isEnabled(config.Weapons.Damage)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("damage")), getValue(config.Weapons.Damage));
        }
        if (isEnabled(config.Weapons.ReloadMax)) {
            replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("reload_max")), getValue(config.Weapons.ReloadMax));
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

int hookSetShipSkinCommand(lua_State *L) {
    // calls getBody on slot1, returning a table with shipId and skinId
    lua_getfield(L, 2, il2cpp_string_new(const_cast<char*>("getBody")));
    lua_insert(L, -2);
    lua_pcall(L, 1, 1, 0);

    // assigns skinId to a variable and pops it, leaving the stack intact
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("skinId")));
    int skinId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    // assigns shipId to a variable and pops it, leaving the stack intact
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("shipId")));
    int shipId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    // calls getProxy(BayProxy), which returns a table with its functions
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("getProxy")));
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("BayProxy")));
    lua_pcall(L, 1, 1, 0);

    // gets the function getShipById from the proxy, shifts it to the left
    // so that the proxy table from before is passed as its first argument,
    // pushes shipId and calls the function.
    // returns the ship table.
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("getShipById")));
    lua_insert(L, -2);
    lua_pushnumber(L, shipId);
    lua_pcall(L, 2, 1, 0);

    // if skinId is 0, which means default skin
    if (skinId == 0) {
        // gets the getConfig function from the ship table, pushes a copy
        // of the ship table onto the top of the stack, as it's needed as first argument,
        // and pushes "skin_id" as second argument, then calls the function.
        // returns the ship's default skin id, which replaces the 0 of skinId.
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("getConfig")));
        lua_pushvalue(L, -2);
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>("skin_id")));
        lua_pcall(L, 2, 1, 0);
        skinId = (int)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    // replaces the ship's skinId attribute with the one saved
    lua_pushnumber(L, skinId);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("skinId")));

    // calls getProxy(BayProxy) again
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("getProxy")));
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("BayProxy")));
    lua_pcall(L, 1, 1, 0);

    // gets the function updateShip, shifts it to the left as done before,
    // pushes a copy of the ship table onto the top of the stack, and calls
    // the function.
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("updateShip")));
    lua_insert(L, -2);
    lua_pushvalue(L, -3);
    lua_pcall(L, 2, 0, 0);

    // calls getProxy(PlayerProxy)
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("getProxy")));
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("PlayerProxy")));
    lua_pcall(L, 1, 1, 0);

    // gets the function sendNotification, shifts it to the left,
    // gets the global SetShipSkinCommand to get SKIN_UPDATED, then removes
    // said global, creates a new table, pushes a copy of the ship table onto
    // the top of the stack to assign it to the key "ship" of the table,
    // and then calls the function.
    // before calling it, the end of the stack is:
    // (ship) [sendNotification] [PlayerProxy] [SKIN_UPDATED] [table]
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("sendNotification")));
    lua_insert(L, -2);
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("SetShipSkinCommand")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("SKIN_UPDATED")));
    lua_remove(L, -2);
    lua_newtable(L);
    lua_pushvalue(L, -5);
    lua_setfield(L, -2, il2cpp_string_new(const_cast<char*>("ship")));
    lua_pcall(L, 3, 0, 0);

    // pops the ship table, as it's not needed anymore
    lua_pop(L, 1);

    // gets pg.TipsMgr's GetInstance function, removes pg.TipsMgr from the stack
    // and calls the function.
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("TipsMgr")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("GetInstance")));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pcall(L, 0, 1, 0);

    // gets function ShowTips from GetInstance's table, shifts it to the left,
    // gets the function i18n and calls it with "ship_set_skin_success", then passes it
    // as second argument to ShowTips (first being "this") and calls it.
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ShowTips")));
    lua_insert(L, -2);
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("i18n")));
    lua_pushstring(L, il2cpp_string_new(const_cast<char*>("ship_set_skin_success")));
    lua_pcall(L, 1, 1, 0);
    lua_pcall(L, 2, 0, 0);

    auto found = skins.find(shipId);
    if (skins.find(shipId) == skins.end()) {
        skins.insert(std::pair<int, int>(shipId, skinId));
    } else {
        skins.find(shipId)->second = skinId;
    }

    if (!writeSkinsFile()) {
        crash();
    }

    return 0;
}

int hookSFVSetSkinList(lua_State *L) {
    // sets ShipFashionView.skinList to pg.ship_skin_template.all,
    // where all skin indexes are stored.
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ship_skin_template")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("all")));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_setfield(L, 1, il2cpp_string_new(const_cast<char*>("skinList")));

    return 0;
}

int wrapShipCtor(lua_State *L) {
    // gets Ship.oldCtor function and calls it using
    // arg1 and arg2 (slot0, slot1) as arguments.
    // returns the Ship object.
    lua_getglobal(L, il2cpp_string_new(const_cast<char*>("Ship")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("oldCtor")));
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pcall(L, 2, 1, 0);

    // get the Ship's id (unique id, not ship's id).
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("id")));
    int shipId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    // search the id on skins, if found, replace
    // skinId with the skin.
    const auto &ship = skins.find(shipId);
    if (ship != skins.end()) {
        replaceAttributeN(L, il2cpp_string_new(const_cast<char*>("skinId")), ship->second);
    }

    return 1;
}

int hookCommitCombat(lua_State *L) {
    // get slot0.contextData.editFleet, pass to int and then pop
    // editFleet.
    lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("editFleet")));
    int editFleet = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    // get normalStageIDs from contextData, which wasn't popped earlier,
    // pass it to int then pop both.
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("normalStageIDs")));
    int normalStageLength = (int)lua_objlen(L, -1);
    lua_pop(L, 2);

    if (editFleet > normalStageLength) {
        if (config.Enemies.Enabled || config.Aircraft.Enabled || config.Weapons.Enabled) {
            // pg.TipsMgr.GetInstance()
            lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
            lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("TipsMgr")));
            lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("GetInstance")));
            lua_remove(L, -2);
            lua_remove(L, -2);
            lua_pcall(L, 0, 1, 0);

            // :ShowTips(slot0, string)
            lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ShowTips")));
            lua_insert(L, -2);
            lua_pushstring(L, il2cpp_string_new(const_cast<char*>("Don't cheat on EX ;)")));
            lua_pcall(L, 2, 0, 0);

            return 0;
        }
        // get emit from slot0, push a copy of slot0,
        // get slot0.contextData.mediatorClass.ON_EX_PRECOMBAT
        // and remove contextData & mediatorClass.
        // [emit] [slot0] [ON_EX_PRECOMBAT]
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("emit")));
        lua_pushvalue(L, 1);
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("mediatorClass")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ON_EX_PRECOMBAT")));
        lua_remove(L, -2);
        lua_remove(L, -2);

        // get contextData.editFleet, remove contextData
        // and push false to the stack.
        // [emit] [slot0] [ON_EX_PRECOMBAT] [editFleet] [false]
        // then call the function
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("editFleet")));
        lua_remove(L, -2);
        lua_pushboolean(L, 0);
        lua_pcall(L, 4, 0, 0);
    } else {
        // get emit from slot0, push a copy of slot0,
        // get slot0.contextData.mediatorClass.ON_PRECOMBAT
        // and remove contextData & mediatorClass.
        // [emit] [slot0] [ON_PRECOMBAT]
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("emit")));
        lua_pushvalue(L, 1);
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("mediatorClass")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ON_PRECOMBAT")));
        lua_remove(L, -2);
        lua_remove(L, -2);

        // get contextData.editFleet, and remove contextData,
        // [emit] [slot0] [ON_EX_PRECOMBAT] [editFleet]
        // then call the function.
        lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("editFleet")));
        lua_remove(L, -2);
        lua_pcall(L, 3, 0, 0);
    }

    return 0;
}

int hookCommitTrybat(lua_State *L) {
    if (config.Enemies.Enabled || config.Aircraft.Enabled || config.Weapons.Enabled) {
        // pg.TipsMgr.GetInstance()
        lua_getglobal(L, il2cpp_string_new(const_cast<char*>("pg")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("TipsMgr")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("GetInstance")));
        lua_remove(L, -2);
        lua_remove(L, -2);
        lua_pcall(L, 0, 1, 0);

        // :ShowTips(slot0, string)
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ShowTips")));
        lua_insert(L, -2);
        lua_pushstring(L, il2cpp_string_new(const_cast<char*>("Don't cheat on EX ;)")));
        lua_pcall(L, 2, 0, 0);

        return 0;
    }
    // get emit from slot0, push a copy of slot0,
    // get slot0.contextData.mediatorClass.ON_EX_PRECOMBAT
    // and remove contextData & mediatorClass.
    // [emit] [slot0] [ON_EX_PRECOMBAT]
    lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("emit")));
    lua_pushvalue(L, 1);
    lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("mediatorClass")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("ON_EX_PRECOMBAT")));
    lua_remove(L, -2);
    lua_remove(L, -2);

    // get contextData.editFleet, remove contextData
    // and push false to the stack.
    // [emit] [slot0] [ON_EX_PRECOMBAT] [editFleet] [false]
    // then call the function
    lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("contextData")));
    lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("editFleet")));
    lua_remove(L, -2);
    lua_pushboolean(L, 1);
    lua_pcall(L, 4, 0, 0);

    return 0;
}

int hookBUAddBuff(lua_State *L) {
    constexpr int FOE_CODE = -1;

    lua_getfield(L, 1, il2cpp_string_new(const_cast<char*>("_IFF")));
    const int IFF = static_cast<int>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    if (IFF != FOE_CODE) {
        lua_getglobal(L, il2cpp_string_new(const_cast<char*>("ys")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("Battle")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("BattleUnit")));
        lua_getfield(L, -1, il2cpp_string_new(const_cast<char*>("oldAddBuff")));

        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);

        lua_pcall(L, 3, 0, 0);
        lua_pop(L, 3);
    }

    return 0;
}

const char *(*old_lua_tolstring)(lua_State *instance, int index, int &strLen);

const char *lua_tolstring(lua_State *instance, int index, int &strLen) {
    if (instance && !exec) {
        exec = true;
        percyLog("injecting");

        lua_State *nL = lua_newthread(instance);
        lua_getglobal(nL, il2cpp_string_new(const_cast<char*>("pg")));

        lua_getglobal(nL, il2cpp_string_new(const_cast<char*>("CheaterMarkCommand")));
        lua_pushcfunction(nL, nilFunc);
        lua_setfield(nL, -2, il2cpp_string_new(const_cast<char*>("execute")));
        lua_pop(nL, 1);

        lua_getglobal(nL, il2cpp_string_new(const_cast<char*>("ActivityBossSceneTemplate")));
        lua_pushcfunction(nL, hookCommitCombat);
        lua_setfield(nL, -2, il2cpp_string_new(const_cast<char*>("commitCombat")));
        lua_pop(nL, 1);

        lua_getglobal(nL, il2cpp_string_new(const_cast<char*>("ActivityBossSceneTemplate")));
        lua_pushcfunction(nL, hookCommitTrybat);
        lua_setfield(nL, -2, il2cpp_string_new(const_cast<char*>("commitTrybat")));
        lua_pop(nL, 1);

        modSpoof(nL);
        parseLv(nL, config.Spoof.lv);

        if (config.Aircraft.Enabled) {
            modAircraft(nL);
        }
        if (config.Enemies.Enabled) {
            modEnemies(nL);
        }
        if (config.Misc.Enabled) {
            modMisc(nL);
        }
        if (config.Weapons.Enabled) {
            modWeapons(nL);
        }

        percyLog("injected");
    }
    return old_lua_tolstring(instance, index, strLen);
}

// thread where everything is ran
void *hack_thread(void *) {
    // check if target lib is loaded
    do {
        sleep(3);
    } while (!isLibraryLoaded(targetLibName));

    // load necessary functions
    loadil2cppfuncs();
    loadluafuncs();

    hook((void *)GETLUAFUNC("lua_tolstring"), (void *)lua_tolstring, (void **)&old_lua_tolstring);

    return nullptr;
}

void getConfigPath(JNIEnv *env, jobject context) {
    jclass cls_Env = env->FindClass("android/app/NativeActivity");
    jmethodID mid = env->GetMethodID(cls_Env, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    jobject obj_File = env->CallObjectMethod(context, mid, NULL);
    jclass cls_File = env->FindClass("java/io/File");
    jmethodID mid_getPath = env->GetMethodID(cls_File, "getPath", "()Ljava/lang/String;");
    auto obj_Path = (jstring)env->CallObjectMethod(obj_File, mid_getPath);
    const char *path = env->GetStringUTFChars(obj_Path, nullptr);

    std::string route(path);
    configPath = route + "/Perseus.json";
    skinPath = route + "/Skins.ini";

    env->ReleaseStringUTFChars(obj_Path, path);
}

template <typename T> T jsonGetKey(nlohmann::basic_json<nlohmann::ordered_map> obj, std::string name, T _default) {
    auto iter = obj.find(name);
    if (iter == obj.end())
        return _default;

    if constexpr (std::is_same_v<T, bool>) {
        if (!obj[name].is_boolean())
            return _default;
        return obj[name].get<bool>();
    } else if constexpr (std::is_same_v<T, int>) {
        if (!obj[name].is_number_integer())
            return _default;
        return obj[name].get<int>();
    } else if constexpr (std::is_same_v<T, double>) {
        if (!obj[name].is_number())
            return _default;
        return obj[name].get<double>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!obj[name].is_string())
            return _default;
        return obj[name].get<std::string>();
    }
}

void init(JNIEnv *env, jclass clazz, jobject context) {
    // get external path where config shall be located
    getConfigPath(env, context);

    if (access(configPath.c_str(), F_OK) != 0) {
        if (!writeConfigFile()) {
            crash();
        }
    }

    std::ifstream configFile(configPath);
    ordered_json configJson = nullptr;

    if (configFile.is_open()) {
        configJson = ordered_json::parse(configFile);
    } else {
        crash();
    }

    if (configJson == nullptr)
        crash();

    int i = 0;

    for (auto &[key, value] : configJson.items()) {
        if (i == 0 && !(key == "OriginalRepo" && value == "github.com/Egoistically/Perseus"))
            configJson = nullptr;
        if (i == 1 && !(key == "PieRepo" && value == "github.com/4pii4/PiePerseus"))
            configJson = nullptr;

        if (i > 1) {
            if (key == "Aircraft") {
                config.Aircraft.Enabled = jsonGetKey<bool>(value, "Enabled", false);
                config.Aircraft.Accuracy = jsonGetKey<int>(value, "Accuracy", -1);
                config.Aircraft.AccuracyGrowth = jsonGetKey<int>(value, "AccuracyGrowth", -1);
                config.Aircraft.AttackPower = jsonGetKey<int>(value, "AttackPower", -1);
                config.Aircraft.AttackPowerGrowth = jsonGetKey<int>(value, "AttackPowerGrowth", -1);
                config.Aircraft.CrashDamage = jsonGetKey<int>(value, "CrashDamage", -1);
                config.Aircraft.Hp = jsonGetKey<int>(value, "Hp", -1);
                config.Aircraft.HpGrowth = jsonGetKey<int>(value, "HpGrowth", -1);
                config.Aircraft.Speed = jsonGetKey<int>(value, "Speed", -1);
            } else if (key == "Enemies") {
                config.Enemies.Enabled = jsonGetKey<bool>(value, "Enabled", false);
                config.Enemies.AntiAir = jsonGetKey<int>(value, "AntiAir", -1);
                config.Enemies.AntiAirGrowth = jsonGetKey<int>(value, "AntiAirGrowth", -1);
                config.Enemies.AntiSubmarine = jsonGetKey<int>(value, "AntiSubmarine", -1);
                config.Enemies.Armor = jsonGetKey<int>(value, "Armor", -1);
                config.Enemies.ArmorGrowth = jsonGetKey<int>(value, "ArmorGrowth", -1);
                config.Enemies.Cannon = jsonGetKey<int>(value, "Cannon", -1);
                config.Enemies.CannonGrowth = jsonGetKey<int>(value, "CannonGrowth", -1);
                config.Enemies.Evasion = jsonGetKey<int>(value, "Evasion", -1);
                config.Enemies.EvasionGrowth = jsonGetKey<int>(value, "EvasionGrowth", -1);
                config.Enemies.Hit = jsonGetKey<int>(value, "Hit", -1);
                config.Enemies.HitGrowth = jsonGetKey<int>(value, "HitGrowth", -1);
                config.Enemies.Hp = jsonGetKey<int>(value, "Hp", -1);
                config.Enemies.HpGrowth = jsonGetKey<int>(value, "HpGrowth", -1);
                config.Enemies.Luck = jsonGetKey<int>(value, "Luck", -1);
                config.Enemies.LuckGrowth = jsonGetKey<int>(value, "LuckGrowth", -1);
                config.Enemies.Reload = jsonGetKey<int>(value, "Reload", -1);
                config.Enemies.ReloadGrowth = jsonGetKey<int>(value, "ReloadGrowth", -1);
                config.Enemies.RemoveBuffs = jsonGetKey<bool>(value, "RemoveBuffs", false);
                config.Enemies.RemoveEquipment = jsonGetKey<bool>(value, "RemoveEquipment", false);
                config.Enemies.RemoveSkill = jsonGetKey<bool>(value, "RemoveSkill", false);
                config.Enemies.Speed = jsonGetKey<int>(value, "Speed", -1);
                config.Enemies.SpeedGrowth = jsonGetKey<int>(value, "SpeedGrowth", -1);
                config.Enemies.Torpedo = jsonGetKey<int>(value, "Torpedo", -1);
                config.Enemies.TorpedoGrowth = jsonGetKey<int>(value, "TorpedoGrowth", -1);
            } else if (key == "Weapons") {
                config.Weapons.Enabled = jsonGetKey<bool>(value, "Enabled", false);
                config.Weapons.Damage = jsonGetKey<int>(value, "Damage", -1);
                config.Weapons.ReloadMax = jsonGetKey<int>(value, "ReloadMax", -1);
            } else if (key == "Spoof") {
                config.Spoof.Enabled = jsonGetKey<bool>(value, "Enabled", false);
                config.Spoof.name = jsonGetKey<std::string>(value, "Name", "");
                config.Spoof.id = jsonGetKey<std::string>(value, "Id", "");
                config.Spoof.lv = jsonGetKey<double>(value, "Lv", -1);
            } else if (key == "Misc") {
                config.Misc.Enabled = jsonGetKey<bool>(value, "Enabled", false);
                config.Misc.ExerciseGodmode = jsonGetKey<bool>(value, "ExerciseGodmode", false);
                config.Misc.FastStageMovement = jsonGetKey<bool>(value, "FastStageMovement", false);
                config.Misc.Skins = jsonGetKey<bool>(value, "Skins", false);
                config.Misc.AutoRepeatLimit = jsonGetKey<int>(value, "AutoRepeatLimit", -1);
                config.Misc.ChatCommands = jsonGetKey<bool>(value, "ChatCommands", false);
                config.Misc.RemoveBBAnimation = jsonGetKey<bool>(value, "RemoveBBAnimation", false);
                config.Misc.RemoveMoraleWarning = jsonGetKey<bool>(value, "RemoveMoraleWarning", false);
                config.Misc.RemoveHardModeStatLimit = jsonGetKey<bool>(value, "RemoveHardModeStatLimit", false);
                config.Misc.RemoveNSFWArts = jsonGetKey<bool>(value, "RemoveNSFWArts", false);
                config.Misc.AllBlueprintsConvertible = jsonGetKey<bool>(value, "AllBlueprintsConvertible", false);
            }
        }
        i++;
    }

    Toast(env, context, "Enjoy the feet, by @Egoistically, @pi.kt and @cmdtaves", ToastLength::LENGTH_LONG);
    percyLog("config loaded");

    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);

    if (!toastCalled) {
        crash();
    }
}

int RegisterMain(JNIEnv *env) {
    JNINativeMethod methods[] = {
        {"init", "(Landroid/content/Context;)V", reinterpret_cast<void *>(init)},
    };
    jclass clazz = env->FindClass("com/unity3d/player/UnityPlayerActivity");
    if (!clazz)
        return JNI_ERR;
    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) != 0)
        return JNI_ERR;

    return JNI_OK;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    vm->GetEnv((void **)&env, JNI_VERSION_1_6);

    if (RegisterMain(env) != 0)
        return JNI_ERR;
    return JNI_VERSION_1_6;
}
