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
#include <chrono>
#include <cstdarg>

#include "Includes/obfuscate.h"
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
#define targetLibName OBFUSCATE("libil2cpp.so")
#define tostr(x) (static_cast<std::string>(x))
#define stdstr(x) (static_cast<std::string>(OBFUSCATE(x)))
#define GETLUAFUNC(method) getFunctionAddress(OBFUSCATE("LuaInterface"), OBFUSCATE("LuaDLL"), OBFUSCATE(method))
#define STR(str) il2cpp_string_new(OBFUSCATE(str))

using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

bool exec = false;
std::string configPath;
std::string skinPath;
std::map<int, int> skins;
Config config;
Il2CppImage *image = nullptr;
bool g_lua_funcs_loaded = false; // flagged when lua pointers loaded

// -------------------- forward / global function pointer declarations --------------------
// il2cpp functions (populated at runtime)
Il2CppDomain *(*il2cpp_domain_get)() = nullptr;
Il2CppAssembly *(*il2cpp_domain_assembly_open)(void *, char *) = nullptr;
Il2CppImage *(*il2cpp_assembly_get_image)(void *) = nullptr;
void *(*il2cpp_class_from_name)(void *, char *, char *) = nullptr;
MethodInfo *(*il2cpp_class_get_method_from_name)(void *, char *, int) = nullptr;
Il2CppString *(*il2cpp_string_new)(char *) = nullptr;

// Lua function pointers (populated at runtime)
lua_State *(*lua_newthread)(lua_State *) = nullptr;
void (*lua_getfield)(lua_State *, int, Il2CppString *) = nullptr;
void (*lua_gettable)(lua_State *, int) = nullptr;
void (*lua_setfield)(lua_State *, int, Il2CppString *) = nullptr;
size_t (*lua_objlen)(lua_State *, int) = nullptr;
void (*lua_pushnil)(lua_State *) = nullptr;
void (*lua_createtable)(lua_State *, int, int) = nullptr;
void (*lua_pushnumber)(lua_State *, double) = nullptr;
void (*lua_pushboolean)(lua_State *, int) = nullptr;
void (*lua_settop)(lua_State *, int) = nullptr;
int (*lua_next)(lua_State *, int) = nullptr;
double (*lua_tonumber)(lua_State *, int) = nullptr;
int (*lua_type)(lua_State *, int) = nullptr;
void (*lua_pushcclosure)(lua_State *, lua_CFunction, int) = nullptr;
int (*lua_pcall)(lua_State *, int, int, int) = nullptr;
void (*lua_call)(lua_State *, int, int) = nullptr;
void (*lua_insert)(lua_State *, int) = nullptr;
void (*lua_pushvalue)(lua_State *, int) = nullptr;
void (*lua_pushstring)(lua_State *, Il2CppString *) = nullptr;
void (*lua_remove)(lua_State *, int) = nullptr;
int (*lua_gettop)(lua_State *) = nullptr;
void (*lua_settable)(lua_State *, int) = nullptr;
void (*lua_rawseti)(lua_State *, int, int) = nullptr;
void (*lua_rawgeti)(lua_State *, int, int) = nullptr;
void (*lua_rawset)(lua_State *, int) = nullptr;
void (*lua_rawget)(lua_State *, int) = nullptr;
const char *(*lua_setupvalue)(lua_State *, int, int) = nullptr;
int (*lua_equal)(lua_State *, int, int) = nullptr;
int (*lua_toboolean)(lua_State *, int) = nullptr;
int (*lua_lessthan)(lua_State *, int, int) = nullptr;
void (*lua_replace)(lua_State *, int) = nullptr;
void (*lua_concat)(lua_State *, int) = nullptr;
int (*lua_isnumber)(lua_State *, int) = nullptr;
int (*lua_checkstack)(lua_State *, int) = nullptr;
int (*luaL_getmetafield)(lua_State *, int, Il2CppString *) = nullptr;
int (*lua_istable)(lua_State *, int) = nullptr;
void (*lua_pushcfunction)(lua_State *, lua_CFunction) = nullptr;

// old function pointer (hook target)
const char *(*old_lua_tolstring)(lua_State *instance, int index, int &strLen) = nullptr;

// forward prototypes (used in file)
const char *lua_tolstring(lua_State *instance, int index, int &strLen);
Il2CppString *stdstr2ilstr(std::string s) { return il2cpp_string_new((char *)s.c_str()); }

// -------------------- helper utilities --------------------
void crash() {
    abort();             // std posix
    // best-effort hard crash
    *((char *)-1) = 'x';
    int *p = (int *)-1;
    *p = 1;
}

void percyLog(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_VERBOSE, OBFUSCATE("Perseus"), fmt, arg);
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
        if (l.length() < 3) continue;
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

bool isEnabled(const std::string &param) { return param != stdstr("false"); }
bool isEnabled(const int param) { return param != -1; }
int getValue(const std::string &param) { return std::stoi(param); }
int getValue(const int param) { return param; }

void checkHeader(const std::string &line, const std::string &header) {
    if (line != header) config.Valid = false;
}

bool getKeyEnabled(const std::string &line, const std::string &key) {
    std::vector<std::string> splitLine = split(line, '=');
    std::string value = splitLine[1];
    if (value != stdstr("true") && value != stdstr("false") || splitLine[0] != key) {
        config.Valid = false;
        return false;
    }
    return isEnabled(value);
}

std::string getKeyValue(const std::string &line, const std::string &key) {
    std::vector<std::string> splitLine = split(line, '=');
    std::string value = splitLine[1];
    try {
        if (splitLine[0] != key) throw 1;
        if (value == stdstr("false")) return value;
        if (getValue(value) < 0) throw 1;
    } catch (...) {
        config.Valid = false;
        return stdstr("false");
    }
    return value;
}

// -------------------- il2cpp / lua function resolution --------------------

// getFunctionAddress: returns pointer to the method pointer from MethodInfo
Il2CppMethodPointer *getFunctionAddress(char *namespaze, char *klass, char *method) {
    if (!image) {
        percyLog("Perseus: getFunctionAddress: image is NULL for %s.%s::%s", namespaze ? namespaze : "?", klass ? klass : "?", method ? method : "?");
        return nullptr;
    }
    void *iklass = il2cpp_class_from_name(image, namespaze, klass);
    if (!iklass) {
        percyLog("Perseus: il2cpp_class_from_name returned NULL for %s.%s", namespaze ? namespaze : "?", klass ? klass : "?");
        return nullptr;
    }
    MethodInfo *imethod = il2cpp_class_get_method_from_name(iklass, method, -1);
    if (!imethod) {
        percyLog("Perseus: il2cpp_class_get_method_from_name returned NULL for %s.%s::%s", namespaze ? namespaze : "?", klass ? klass : "?", method ? method : "?");
        return nullptr;
    }
    return imethod->methodPointer;
}

static bool wait_for_global_table(lua_State *L, const char *name, int max_ms = 15000, int poll_ms = 200) {
    int waited = 0;
    int attempt = 0;
    while (waited < max_ms) {
        ++attempt;
        int top = lua_gettop(L);

        Il2CppString *tmpName = il2cpp_string_new((char *)name);
        lua_getglobal(L, tmpName);

        int t = lua_type(L, -1);
        bool exists = (t != 0);

        lua_settop(L, top);

        percyLog("Perseus: wait_for_global_table('%s') attempt %d top=%d exists=%d", name, attempt, top, exists ? 1 : 0);

        if (exists) return true;

        usleep(poll_ms * 1000);
        waited += poll_ms;
    }
    percyLog("Perseus: wait_for_global_table('%s') timed out after %d ms", name, max_ms);
    return false;
}

void loadil2cppfuncs() {
    // populate il2cpp functions (resolve symbols)
    il2cpp_domain_get = (Il2CppDomain * (*)()) GETSYM(targetLibName, "il2cpp_domain_get");
    if (!il2cpp_domain_get) { percyLog("Perseus: failed to find il2cpp_domain_get"); return; }

    il2cpp_domain_assembly_open = (Il2CppAssembly * (*)(void *, char *)) GETSYM(targetLibName, "il2cpp_domain_assembly_open");
    il2cpp_assembly_get_image = (Il2CppImage * (*)(void *)) GETSYM(targetLibName, "il2cpp_assembly_get_image");
    il2cpp_class_from_name = (void *(*)(void *, char *, char *)) GETSYM(targetLibName, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (MethodInfo * (*)(void *, char *, int)) GETSYM(targetLibName, "il2cpp_class_get_method_from_name");
    il2cpp_string_new = (Il2CppString * (*)(char *)) GETSYM(targetLibName, "il2cpp_string_new");

    if (!il2cpp_domain_assembly_open || !il2cpp_assembly_get_image || !il2cpp_class_from_name || !il2cpp_class_get_method_from_name || !il2cpp_string_new) {
        percyLog("Perseus: some il2cpp symbols missing; aborting il2cpp init");
        return;
    }

    Il2CppDomain *domain = il2cpp_domain_get();
    if (!domain) {
        percyLog("Perseus: il2cpp_domain_get returned NULL");
        return;
    }

    // try to enumerate assemblies to find the image that contains LuaInterface.LuaDLL
    typedef const void **(_il2cpp_domain_get_assemblies)(Il2CppDomain *, size_t *);
    _il2cpp_domain_get_assemblies *domain_get_assemblies = nullptr;
    domain_get_assemblies = (_il2cpp_domain_get_assemblies *) GETSYM(targetLibName, "il2cpp_domain_get_assemblies");

    bool found_image = false;
    if (domain_get_assemblies) {
        size_t assembly_count = 0;
        const void **assemblies = domain_get_assemblies(domain, &assembly_count);
        percyLog("Perseus: il2cpp_domain_get_assemblies returned %zu assemblies", assembly_count);
        for (size_t i = 0; i < assembly_count; ++i) {
            Il2CppAssembly *a = (Il2CppAssembly *)assemblies[i];
            if (!a) continue;
            Il2CppImage *img = il2cpp_assembly_get_image(a);
            if (!img) continue;
            void *k = il2cpp_class_from_name((void *)img, OBFUSCATE("LuaInterface"), OBFUSCATE("LuaDLL"));
            if (k) {
                image = img;
                found_image = true;
                percyLog("Perseus: Found LuaInterface.LuaDLL in assembly %zu", i);
                break;
            }
        }
    }

    if (!found_image) {
        percyLog("Perseus: Falling back to Assembly-CSharp image");
        Il2CppAssembly *assembly = il2cpp_domain_assembly_open(domain, OBFUSCATE("Assembly-CSharp"));
        if (assembly) {
            image = il2cpp_assembly_get_image(assembly);
            if (!image) percyLog("Perseus: il2cpp_assembly_get_image returned NULL for Assembly-CSharp");
        } else {
            percyLog("Perseus: il2cpp_domain_assembly_open failed for Assembly-CSharp");
            image = nullptr;
        }
    }
}

void loadluafuncs() {
    // Use GETLUAFUNC macro which uses image; ensure image exists
    if (!image) {
        percyLog("Perseus: loadluafuncs: image is NULL");
        g_lua_funcs_loaded = false;
        return;
    }

    // attempt to resolve one critical function first
    lua_newthread = (lua_State * (*)(lua_State *)) GETLUAFUNC("lua_newthread");
    if (!lua_newthread) {
        percyLog("Perseus: FATAL - Could not find 'lua_newthread' (LuaInterface.LuaDLL missing).");
        g_lua_funcs_loaded = false;
        return;
    }

    // resolve remaining lua symbols (best-effort)
    lua_getfield = (void (*)(lua_State *, int, Il2CppString *)) GETLUAFUNC("lua_getfield");
    lua_gettable = (void (*)(lua_State *, int)) GETLUAFUNC("lua_gettable");
    lua_setfield = (void (*)(lua_State *, int, Il2CppString *)) GETLUAFUNC("lua_setfield");
    lua_objlen = (size_t (*)(lua_State *, int)) GETLUAFUNC("lua_objlen");
    lua_pushnil = (void (*)(lua_State *)) GETLUAFUNC("lua_pushnil");
    lua_createtable = (void (*)(lua_State *, int, int)) GETLUAFUNC("lua_createtable");
    lua_pushnumber = (void (*)(lua_State *, double)) GETLUAFUNC("lua_pushnumber");
    lua_pushboolean = (void (*)(lua_State *, int)) GETLUAFUNC("lua_pushboolean");
    lua_settop = (void (*)(lua_State *, int)) GETLUAFUNC("lua_settop");
    lua_next = (int (*)(lua_State *, int)) GETLUAFUNC("lua_next");
    lua_tonumber = (double (*)(lua_State *, int)) GETLUAFUNC("lua_tonumber");
    lua_type = (int (*)(lua_State *, int)) GETLUAFUNC("lua_type");
    lua_pushcclosure = (void (*)(lua_State *, lua_CFunction, int)) GETLUAFUNC("lua_pushcclosure");
    lua_pcall = (int (*)(lua_State *, int, int, int)) GETLUAFUNC("lua_pcall");
    lua_call = (void (*)(lua_State *, int, int)) GETLUAFUNC("lua_call");
    lua_insert = (void (*)(lua_State *, int)) GETLUAFUNC("lua_insert");
    lua_pushvalue = (void (*)(lua_State *, int)) GETLUAFUNC("lua_pushvalue");
    lua_pushstring = (void (*)(lua_State *, Il2CppString *)) GETLUAFUNC("lua_pushstring");
    lua_remove = (void (*)(lua_State *, int)) GETLUAFUNC("lua_remove");
    lua_gettop = (int (*)(lua_State *)) GETLUAFUNC("lua_gettop");
    lua_settable = (void (*)(lua_State *, int)) GETLUAFUNC("lua_settable");
    lua_rawseti = (void (*)(lua_State *, int, int)) GETLUAFUNC("lua_rawseti");
    lua_rawgeti = (void (*)(lua_State *, int, int)) GETLUAFUNC("lua_rawgeti");
    lua_rawset = (void (*)(lua_State *, int)) GETLUAFUNC("lua_rawset");
    lua_rawget = (void (*)(lua_State *, int)) GETLUAFUNC("lua_rawget");
    lua_setupvalue = (const char * (*)(lua_State *, int, int)) GETLUAFUNC("lua_setupvalue");
    lua_equal = (int (*)(lua_State *, int, int)) GETLUAFUNC("lua_equal");
    lua_toboolean = (int (*)(lua_State *, int)) GETLUAFUNC("lua_toboolean");
    lua_lessthan = (int (*)(lua_State *, int, int)) GETLUAFUNC("lua_lessthan");
    lua_replace = (void (*)(lua_State *, int)) GETLUAFUNC("lua_replace");
    lua_concat = (void (*)(lua_State *, int)) GETLUAFUNC("lua_concat");
    lua_isnumber = (int (*)(lua_State *, int)) GETLUAFUNC("lua_isnumber");
    lua_checkstack = (int (*)(lua_State *, int)) GETLUAFUNC("lua_checkstack");
    luaL_getmetafield = (int (*)(lua_State *, int, Il2CppString *)) GETLUAFUNC("luaL_getmetafield");
    lua_istable = (int (*)(lua_State *, int)) GETLUAFUNC("lua_istable");
    lua_pushcfunction = (void (*)(lua_State *, lua_CFunction)) GETLUAFUNC("lua_pushcfunction");

    // final sanity check of a few core functions
    if (!lua_getfield || !lua_pcall || !lua_setfield) {
        percyLog("Perseus: WARNING - core lua functions missing despite LuaInterface presence.");
        g_lua_funcs_loaded = false;
        return;
    }

    percyLog("Perseus: Lua functions loaded.");
    g_lua_funcs_loaded = true;
}

// -------------------- Lua table helpers & mods (unchanged logic) --------------------
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
    lua_getfield(L, -1, STR("all"));
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        tableIds.push_back((int)lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return tableIds;
}

void modAircraft(lua_State *L) {
    lua_getfield(L, -1, STR("aircraft_template"));
    std::vector<int> aircraft = getTableIds(L);
    for (const int &aircraftId : aircraft) {
        lua_pushnumber(L, aircraftId);
        lua_gettable(L, -2);
        if (isEnabled(config.Aircraft.Accuracy)) {
            replaceAttributeN(L, STR("accuracy"), getValue(config.Aircraft.Accuracy));
        }
        if (isEnabled(config.Aircraft.AccuracyGrowth)) {
            replaceAttributeN(L, STR("ACC_growth"), getValue(config.Aircraft.AccuracyGrowth));
        }
        if (isEnabled(config.Aircraft.AttackPower)) {
            replaceAttributeN(L, STR("attack_power"), getValue(config.Aircraft.AttackPower));
        }
        if (isEnabled(config.Aircraft.AttackPowerGrowth)) {
            replaceAttributeN(L, STR("AP_growth"), getValue(config.Aircraft.AttackPowerGrowth));
        }
        if (isEnabled(config.Aircraft.CrashDamage)) {
            replaceAttributeN(L, STR("crash_DMG"), getValue(config.Aircraft.CrashDamage));
        }
        if (isEnabled(config.Aircraft.Hp)) {
            replaceAttributeN(L, STR("max_hp"), getValue(config.Aircraft.Hp));
        }
        if (isEnabled(config.Aircraft.HpGrowth)) {
            replaceAttributeN(L, STR("hp_growth"), getValue(config.Aircraft.HpGrowth));
        }
        if (isEnabled(config.Aircraft.Speed)) {
            replaceAttributeN(L, STR("speed"), getValue(config.Aircraft.Speed));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// forward declarations for hooks used below
int hookBUAddBuff(lua_State *L);
int hookSetShipSkinCommand(lua_State *L);
int hookSFVSetSkinList(lua_State *L);
int wrapShipCtor(lua_State *L);
int hookCommitCombat(lua_State *L);
int hookCommitTrybat(lua_State *L);

void modEnemies(lua_State *L) {
    lua_getfield(L, -1, STR("enemy_data_statistics"));
    std::vector<int> enemyShips = getTableIds(L);
    for (const int &enemyId : enemyShips) {
        lua_pushnumber(L, enemyId);
        lua_gettable(L, -2);
        if (isEnabled(config.Enemies.AntiAir)) {
            replaceAttributeN(L, STR("antiaircraft"), getValue(config.Enemies.AntiAir));
        }
        if (isEnabled(config.Enemies.AntiAirGrowth)) {
            replaceAttributeN(L, STR("antiaircraft_growth"), getValue(config.Enemies.AntiAirGrowth));
        }
        if (isEnabled(config.Enemies.AntiSubmarine)) {
            replaceAttributeN(L, STR("antisub"), getValue(config.Enemies.AntiSubmarine));
        }
        if (isEnabled(config.Enemies.Armor)) {
            replaceAttributeN(L, STR("armor"), getValue(config.Enemies.Armor));
        }
        if (isEnabled(config.Enemies.ArmorGrowth)) {
            replaceAttributeN(L, STR("armor_growth"), getValue(config.Enemies.ArmorGrowth));
        }
        if (isEnabled(config.Enemies.Cannon)) {
            replaceAttributeN(L, STR("cannon"), getValue(config.Enemies.Cannon));
        }
        if (isEnabled(config.Enemies.CannonGrowth)) {
            replaceAttributeN(L, STR("cannon_growth"), getValue(config.Enemies.CannonGrowth));
        }
        if (isEnabled(config.Enemies.Evasion)) {
            replaceAttributeN(L, STR("dodge"), getValue(config.Enemies.Evasion));
        }
        if (isEnabled(config.Enemies.EvasionGrowth)) {
            replaceAttributeN(L, STR("dodge_growth"), getValue(config.Enemies.EvasionGrowth));
        }
        if (isEnabled(config.Enemies.Hit)) {
            replaceAttributeN(L, STR("hit"), getValue(config.Enemies.Hit));
        }
        if (isEnabled(config.Enemies.HitGrowth)) {
            replaceAttributeN(L, STR("hit_growth"), getValue(config.Enemies.HitGrowth));
        }
        if (isEnabled(config.Enemies.Hp)) {
            replaceAttributeN(L, STR("durability"), getValue(config.Enemies.Hp));
        }
        if (isEnabled(config.Enemies.HpGrowth)) {
            replaceAttributeN(L, STR("durability_growth"), getValue(config.Enemies.HpGrowth));
        }
        if (isEnabled(config.Enemies.Luck)) {
            replaceAttributeN(L, STR("luck"), getValue(config.Enemies.Luck));
        }
        if (isEnabled(config.Enemies.LuckGrowth)) {
            replaceAttributeN(L, STR("luck_growth"), getValue(config.Enemies.LuckGrowth));
        }
        if (isEnabled(config.Enemies.Reload)) {
            replaceAttributeN(L, STR("reload"), getValue(config.Enemies.Reload));
        }
        if (isEnabled(config.Enemies.ReloadGrowth)) {
            replaceAttributeN(L, STR("reload_growth"), getValue(config.Enemies.ReloadGrowth));
        }
        if (config.Enemies.RemoveEquipment) {
            emptyAttributeT(L, STR("equipment_list"));
        }
        if (isEnabled(config.Enemies.Speed)) {
            replaceAttributeN(L, STR("speed"), getValue(config.Enemies.Speed));
        }
        if (isEnabled(config.Enemies.SpeedGrowth)) {
            replaceAttributeN(L, STR("speed_growth"), getValue(config.Enemies.SpeedGrowth));
        }
        if (isEnabled(config.Enemies.Torpedo)) {
            replaceAttributeN(L, STR("torpedo"), getValue(config.Enemies.Torpedo));
        }
        if (isEnabled(config.Enemies.TorpedoGrowth)) {
            replaceAttributeN(L, STR("torpedo_growth"), getValue(config.Enemies.TorpedoGrowth));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    if (config.Enemies.RemoveSkill) {
        lua_getfield(L, -1, STR("enemy_data_skill"));
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_istable(L, -1)) {
                emptyAttributeT(L, STR("skill_list"));
                replaceAttributeN(L, STR("is_repeat"), 0);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    if (config.Enemies.RemoveBuffs) {
        lua_getglobal(L, STR("ys"));
        lua_getfield(L, -1, STR("Battle"));
        lua_getfield(L, -1, STR("BattleUnit"));
        lua_getfield(L, -1, STR("AddBuff"));
        lua_setfield(L, -2, STR("oldAddBuff"));
        lua_pushcfunction(L, hookBUAddBuff);
        lua_setfield(L, -2, STR("AddBuff"));
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
        if (luaL_getmetafield(L, idxa, STR("__add")) || luaL_getmetafield(L, idxb, STR("__add"))) {
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
        if (luaL_getmetafield(L, idxa, STR("__sub")) || luaL_getmetafield(L, idxb, STR("__sub"))) {
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
        if (luaL_getmetafield(L, idxa, STR("__div")) || luaL_getmetafield(L, idxb, STR("__div"))) {
            lua_pushvalue(L, idxa < 0 && idxa > LUA_REGISTRYINDEX ? idxa - 1 : idxa);
            lua_pushvalue(L, idxb < 0 && idxb > LUA_REGISTRYINDEX ? idxb - 2 : idxb);
            lua_call(L, 2, 1);
        } else {
            // luaL_error(L, "attempt to perform arithmetic");
        }
    }
}

void modSkins(lua_State *L) {
    lua_getglobal(L, STR("SetShipSkinCommand"));
    lua_pushcfunction(L, hookSetShipSkinCommand);
    lua_setfield(L, -2, STR("execute"));
    lua_pop(L, 1);

    lua_getglobal(L, STR("ShipFashionView"));
    lua_pushcfunction(L, hookSFVSetSkinList);
    lua_setfield(L, -2, STR("SetSkinList"));
    lua_pop(L, 1);

    lua_getglobal(L, STR("Ship"));
    lua_getfield(L, -1, STR("New"));
    lua_setfield(L, -2, STR("oldCtor"));
    lua_pushcfunction(L, wrapShipCtor);
    lua_setfield(L, -2, STR("New"));
    lua_pop(L, 1);

    lua_getglobal(L, STR("ShipSkin"));
    replaceAttributeN(L, STR("SKIN_TYPE_NOT_HAVE_HIDE"), 10);
    lua_pop(L, 1);

    if (!readSkinsFile()) {
        return;
    }
}

void luaMessageBox(lua_State *L, std::string msg) {
    lua_getglobal(L, STR("pg"));
    lua_getfield(L, -1, STR("MsgboxMgr"));
    lua_getfield(L, -1, STR("GetInstance"));
    lua_pcall(L, 0, 1, 0);
    lua_getfield(L, -1, STR("ShowMsgBox"));
    lua_pushvalue(L, -2);

    lua_newtable(L);
    lua_pushstring(L, STR("type"));
    lua_pushnumber(L, 7);
    lua_settable(L, -3);

    lua_pushstring(L, STR("helps"));
    lua_createtable(L, 1, 0);
    lua_newtable(L);
    lua_pushstring(L, STR("info"));
    lua_pushstring(L, stdstr2ilstr(msg));
    lua_settable(L, -3);
    lua_rawseti(L, -2, 1);
    lua_settable(L, -3);

    lua_pcall(L, 2, 0, 0);
}

void luaToast(lua_State *L, std::string msg) {
    lua_getglobal(L, STR("pg"));
    lua_getfield(L, -1, STR("TipsMgr"));
    lua_getfield(L, -1, STR("GetInstance"));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pcall(L, 0, 1, 0);

    lua_getfield(L, -1, STR("ShowTips"));
    lua_insert(L, -2);
    lua_pushstring(L, stdstr2ilstr(msg));
    lua_pcall(L, 2, 0, 0);
}

int hookSendMsgExecute(lua_State *L) {
    lua_getfield(L, 2, STR("getBody"));
    lua_pushvalue(L, 2);
    lua_pcall(L, 1, 1, 0);

    int siz = 0;
    std::string msg(lua_tolstring(L, -1, siz));
    percyLog(OBFUSCATE("chatmsg: %s"), msg.c_str());

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
    std::istringstream luaName(field);
    std::string luaObj;
    int luaObjCount = 1;
    std::getline(luaName, luaObj, '.');
    lua_getglobal(L, il2cpp_string_new(luaObj.data()));
    while (std::getline(luaName, luaObj, '.')) {
        lua_getfield(L, -1, il2cpp_string_new(luaObj.data()));
        luaObjCount++;
        if (!luaName.peek())
            break;
    }
    lua_setfield(L, -2, il2cpp_string_new((char *)(backup_prefix + luaObj.data()).c_str()));
    lua_pushcfunction(L, func);
    lua_setfield(L, -2, il2cpp_string_new(luaObj.data()));
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
        lua_pushstring(L, il2cpp_string_new((char *)val));
    }
    lua_setfield(L, -2, stdstr2ilstr(path.back()));
    lua_pop(L, path.size() - 1);
}

int hookBMWABSetActive(lua_State *L) {
    lua_getglobal(L, STR("ys"));
    lua_getfield(L, -1, STR("Battle"));
    lua_remove(L, -2);
    lua_getfield(L, -1, STR("BattleManualWeaponAutoBot"));
    lua_remove(L, -2);
    lua_getfield(L, -1, STR("oldSetActive"));
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
    lua_setglobal(L, STR("wordVer"));

    if (config.Misc.ExerciseGodmode) {
        lua_getfield(L, -1, STR("ConvertedBuff"));
        lua_getfield(L, -1, STR("buff_19"));
        lua_pushnumber(L, 0);
        lua_gettable(L, -2);
        lua_getfield(L, -1, STR("effect_list"));
        lua_pushnumber(L, 1);
        lua_gettable(L, -2);
        lua_getfield(L, -1, STR("arg_list"));
        replaceAttributeN(L, STR("number"), -1);
        lua_pop(L, 6);
    }

    if (config.Misc.FastStageMovement) {
        luaReplaceAttribute<int>(L, OBFUSCATE("ChapterConst.ShipStepDuration"), 0);
        luaReplaceAttribute<int>(L, OBFUSCATE("ChapterConst.ShipStepQuickPlayScale"), 0);
    }

    if (config.Misc.Skins) {
        modSkins(L);
    }

    if (config.Misc.ChatCommands) {
        luaReplaceAttribute<const char *>(L, OBFUSCATE("pg.gametip.notice_input_desc.tip"), OBFUSCATE("chat/.command"));
        luaHookFunc(L, OBFUSCATE("SendMsgCommand.execute"), hookSendMsgExecute, OBFUSCATE("old_"));
        luaHookFunc(L, OBFUSCATE("GuildSendMsgCommand.execute"), nilFunc, OBFUSCATE("old_"));
    }

    if (config.Misc.RemoveHardModeStatLimit) {
        luaHookFunc(L, OBFUSCATE("WorldFleetSelectLayer.CheckValid"), trueFunc, OBFUSCATE("old"));
        luaHookFunc(L, OBFUSCATE("BossSingleBattleFleetSelectSubPanel.CheckValid"), trueFunc, OBFUSCATE("old"));
        luaHookFunc(L, OBFUSCATE("Chapter.IsEliteFleetLegal"), trueFunc, OBFUSCATE("old"));
    }

    if (config.Misc.RemoveNSFWArts) {
        luaHookFunc(L, OBFUSCATE("CombatLoadUI.init"), CLUInitHook, OBFUSCATE("old"));
    }

    if (config.Misc.AllBlueprintsConvertible) {
        luaHookFunc(L, OBFUSCATE("FragResolvePanel.GetAllBluePrintStrengthenItems"), FRPGetAllBlueprintItemsHook, OBFUSCATE("old"));
    }

    if (config.Misc.RemoveBBAnimation) {
        luaHookFunc(L, OBFUSCATE("ys.Battle.BattleManualWeaponAutoBot.SetActive"), hookBMWABSetActive, OBFUSCATE("old"));
    }

    if (config.Misc.RemoveMoraleWarning) {
        luaHookFunc(L, OBFUSCATE("Ship.cosumeEnergy"), nilFunc, OBFUSCATE("old_"));
        luaHookFunc(L, OBFUSCATE("Ship.getEnergy"), ship150morale, OBFUSCATE("old_"));
    }

    if (isEnabled(config.Misc.AutoRepeatLimit)) {
        luaReplaceAttribute<int>(L, OBFUSCATE("pg.gameset.main_level_multiple_sorties_times.key_value"), config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, OBFUSCATE("pg.gameset.hard_level_multiple_sorties_times.key_value"), config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, OBFUSCATE("pg.gameset.activity_level_multiple_sorties_times.key_value"), config.Misc.AutoRepeatLimit);
        luaReplaceAttribute<int>(L, OBFUSCATE("pg.gameset.archives_level_multiple_sorties_times.key_value"), config.Misc.AutoRepeatLimit);
    }
}

void modWeapons(lua_State *L) {
    lua_getfield(L, -1, STR("weapon_property"));
    std::vector<int> weapons = getTableIds(L);
    for (int weaponId : weapons) {
        lua_pushnumber(L, weaponId);
        lua_gettable(L, -2);
        if (isEnabled(config.Weapons.Damage)) {
            replaceAttributeN(L, STR("damage"), getValue(config.Weapons.Damage));
        }
        if (isEnabled(config.Weapons.ReloadMax)) {
            replaceAttributeN(L, STR("reload_max"), getValue(config.Weapons.ReloadMax));
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// -------------------- hook implementations --------------------

int hookSetShipSkinCommand(lua_State *L) {
    lua_getfield(L, 2, STR("getBody"));
    lua_insert(L, -2);
    lua_pcall(L, 1, 1, 0);

    lua_getfield(L, -1, STR("skinId"));
    int skinId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, STR("shipId"));
    int shipId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, STR("getProxy"));
    lua_getglobal(L, STR("BayProxy"));
    lua_pcall(L, 1, 1, 0);

    lua_getfield(L, -1, STR("getShipById"));
    lua_insert(L, -2);
    lua_pushnumber(L, shipId);
    lua_pcall(L, 2, 1, 0);

    if (skinId == 0) {
        lua_getfield(L, -1, STR("getConfig"));
        lua_pushvalue(L, -2);
        lua_pushstring(L, STR("skin_id"));
        lua_pcall(L, 2, 1, 0);
        skinId = (int)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pushnumber(L, skinId);
    lua_setfield(L, -2, STR("skinId"));

    lua_getglobal(L, STR("getProxy"));
    lua_getglobal(L, STR("BayProxy"));
    lua_pcall(L, 1, 1, 0);

    lua_getfield(L, -1, STR("updateShip"));
    lua_insert(L, -2);
    lua_pushvalue(L, -3);
    lua_pcall(L, 2, 0, 0);

    lua_getglobal(L, STR("getProxy"));
    lua_getglobal(L, STR("PlayerProxy"));
    lua_pcall(L, 1, 1, 0);

    lua_getfield(L, -1, STR("sendNotification"));
    lua_insert(L, -2);
    lua_getglobal(L, STR("SetShipSkinCommand"));
    lua_getfield(L, -1, STR("SKIN_UPDATED"));
    lua_remove(L, -2);
    lua_newtable(L);
    lua_pushvalue(L, -5);
    lua_setfield(L, -2, STR("ship"));
    lua_pcall(L, 3, 0, 0);

    lua_pop(L, 1);

    lua_getglobal(L, STR("pg"));
    lua_getfield(L, -1, STR("TipsMgr"));
    lua_getfield(L, -1, STR("GetInstance"));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pcall(L, 0, 1, 0);

    lua_getfield(L, -1, STR("ShowTips"));
    lua_insert(L, -2);
    lua_getglobal(L, STR("i18n"));
    lua_pushstring(L, STR("ship_set_skin_success"));
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
    lua_getglobal(L, STR("pg"));
    lua_getfield(L, -1, STR("ship_skin_template"));
    lua_getfield(L, -1, STR("all"));
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_setfield(L, 1, STR("skinList"));
    return 0;
}

int wrapShipCtor(lua_State *L) {
    lua_getglobal(L, STR("Ship"));
    lua_getfield(L, -1, STR("oldCtor"));
    lua_remove(L, -2);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_pcall(L, 2, 1, 0);

    lua_getfield(L, -1, STR("id"));
    int shipId = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    const auto &ship = skins.find(shipId);
    if (ship != skins.end()) {
        replaceAttributeN(L, STR("skinId"), ship->second);
    }

    return 1;
}

int hookCommitCombat(lua_State *L) {
    lua_getfield(L, 1, STR("contextData"));
    lua_getfield(L, -1, STR("editFleet"));
    int editFleet = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, STR("normalStageIDs"));
    int normalStageLength = (int)lua_objlen(L, -1);
    lua_pop(L, 2);

    if (editFleet > normalStageLength) {
        if (config.Enemies.Enabled || config.Aircraft.Enabled || config.Weapons.Enabled) {
            lua_getglobal(L, STR("pg"));
            lua_getfield(L, -1, STR("TipsMgr"));
            lua_getfield(L, -1, STR("GetInstance"));
            lua_remove(L, -2);
            lua_remove(L, -2);
            lua_pcall(L, 0, 1, 0);

            lua_getfield(L, -1, STR("ShowTips"));
            lua_insert(L, -2);
            lua_pushstring(L, STR("Don't cheat on EX ;)"));
            lua_pcall(L, 2, 0, 0);

            return 0;
        }
        lua_getfield(L, 1, STR("emit"));
        lua_pushvalue(L, 1);
        lua_getfield(L, 1, STR("contextData"));
        lua_getfield(L, -1, STR("mediatorClass"));
        lua_getfield(L, -1, STR("ON_EX_PRECOMBAT"));
        lua_remove(L, -2);
        lua_remove(L, -2);

        lua_getfield(L, 1, STR("contextData"));
        lua_getfield(L, -1, STR("editFleet"));
        lua_remove(L, -2);
        lua_pushboolean(L, 0);
        lua_pcall(L, 4, 0, 0);
    } else {
        lua_getfield(L, 1, STR("emit"));
        lua_pushvalue(L, 1);
        lua_getfield(L, 1, STR("contextData"));
        lua_getfield(L, -1, STR("mediatorClass"));
        lua_getfield(L, -1, STR("ON_PRECOMBAT"));
        lua_remove(L, -2);
        lua_remove(L, -2);

        lua_getfield(L, 1, STR("contextData"));
        lua_getfield(L, -1, STR("editFleet"));
        lua_remove(L, -2);
        lua_pcall(L, 3, 0, 0);
    }

    return 0;
}

int hookCommitTrybat(lua_State *L) {
    if (config.Enemies.Enabled || config.Aircraft.Enabled || config.Weapons.Enabled) {
        lua_getglobal(L, STR("pg"));
        lua_getfield(L, -1, STR("TipsMgr"));
        lua_getfield(L, -1, STR("GetInstance"));
        lua_remove(L, -2);
        lua_remove(L, -2);
        lua_pcall(L, 0, 1, 0);

        lua_getfield(L, -1, STR("ShowTips"));
        lua_insert(L, -2);
        lua_pushstring(L, STR("Don't cheat on EX ;)"));
        lua_pcall(L, 2, 0, 0);

        return 0;
    }
    lua_getfield(L, 1, STR("emit"));
    lua_pushvalue(L, 1);
    lua_getfield(L, 1, STR("contextData"));
    lua_getfield(L, -1, STR("mediatorClass"));
    lua_getfield(L, -1, STR("ON_EX_PRECOMBAT"));
    lua_remove(L, -2);
    lua_remove(L, -2);

    lua_getfield(L, 1, STR("contextData"));
    lua_getfield(L, -1, STR("editFleet"));
    lua_remove(L, -2);
    lua_pushboolean(L, 1);
    lua_pcall(L, 4, 0, 0);

    return 0;
}

int hookBUAddBuff(lua_State *L) {
    constexpr int FOE_CODE = -1;
    lua_getfield(L, 1, STR("_IFF"));
    const int IFF = static_cast<int>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    if (IFF != FOE_CODE) {
        lua_getglobal(L, STR("ys"));
        lua_getfield(L, -1, STR("Battle"));
        lua_getfield(L, -1, STR("BattleUnit"));
        lua_getfield(L, -1, STR("oldAddBuff"));

        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);

        lua_pcall(L, 3, 0, 0);
        lua_pop(L, 3);
    }

    return 0;
}

// -------------------- hooking injection entrypoint --------------------

const char *lua_tolstring_impl(lua_State *instance, int index, int &strLen) {
    // This is the real impl. We'll call this via hooking.
    // NOTE: original code used old_lua_tolstring pointer after hooking.
    return old_lua_tolstring(instance, index, strLen);
}

const char *lua_tolstring(lua_State *instance, int index, int &strLen) {
    if (instance && !exec) {
        percyLog("Perseus: injection attempt started (instance=%p)", instance);

        // create a new thread from the current lua state to do injection work
        lua_State *nL = lua_newthread(instance);
        if (!nL) {
            percyLog("Perseus: lua_newthread returned NULL, aborting injection attempt");
            return old_lua_tolstring ? old_lua_tolstring(instance, index, strLen) : nullptr;
        }

        int top_before = lua_gettop(nL);
        percyLog("Perseus: before injection, new thread L=%p top=%d", nL, top_before);

        // wait for pg
        if (!wait_for_global_table(nL, "pg", 15000, 200)) {
            percyLog("Perseus: 'pg' global not found - aborting injection attempt");
            return old_lua_tolstring ? old_lua_tolstring(instance, index, strLen) : nullptr;
        }

        // optionally wait for Net (short)
        if (!wait_for_global_table(nL, "Net", 5000, 200)) {
            percyLog("Perseus: 'Net' global not found within 5s - continuing injection cautiously");
        }

        // attempt injection
        bool injection_ok = true;
        try {
            percyLog("Perseus: injecting features into lua state (start)");
            int top_mid = lua_gettop(nL);
            percyLog("Perseus: top before modifications = %d", top_mid);

            lua_getglobal(nL, STR("pg"));

            // Hook few things
            lua_getglobal(nL, STR("CheaterMarkCommand"));
            if (lua_type(nL, -1) != 0) {
                lua_pushcfunction(nL, nilFunc);
                lua_setfield(nL, -2, STR("execute"));
            } else {
                percyLog("Perseus: CheaterMarkCommand nil - skipping");
            }
            lua_pop(nL, 1);

            lua_getglobal(nL, STR("ActivityBossSceneTemplate"));
            if (lua_type(nL, -1) != 0) {
                lua_pushcfunction(nL, hookCommitCombat);
                lua_setfield(nL, -2, STR("commitCombat"));
            } else {
                percyLog("Perseus: ActivityBossSceneTemplate nil - skipping commitCombat");
            }
            lua_pop(nL, 1);

            lua_getglobal(nL, STR("ActivityBossSceneTemplate"));
            if (lua_type(nL, -1) != 0) {
                lua_pushcfunction(nL, hookCommitTrybat);
                lua_setfield(nL, -2, STR("commitTrybat"));
            } else {
                percyLog("Perseus: ActivityBossSceneTemplate nil - skipping commitTrybat");
            }
            lua_pop(nL, 1);

            percyLog("Perseus: running modSpoof and parseLv");
            modSpoof(nL);
            parseLv(nL, config.Spoof.lv);

            if (config.Aircraft.Enabled) {
                percyLog("Perseus: applying modAircraft");
                modAircraft(nL);
            }
            if (config.Enemies.Enabled) {
                percyLog("Perseus: applying modEnemies");
                modEnemies(nL);
            }
            if (config.Misc.Enabled) {
                percyLog("Perseus: applying modMisc");
                modMisc(nL);
            }
            if (config.Weapons.Enabled) {
                percyLog("Perseus: applying modWeapons");
                modWeapons(nL);
            }

            int top_after = lua_gettop(nL);
            percyLog("Perseus: injection modifications done, top after = %d (before was %d)", top_after, top_mid);

            // success -> set exec true
            exec = true;
            percyLog("Perseus: injection finished successfully; exec flag set");
        } catch (...) {
            injection_ok = false;
            percyLog("Perseus: exception during injection (caught), aborting injection attempt");
        }

        if (!injection_ok) {
            percyLog("Perseus: injection failed - will retry on next opportunity");
            return old_lua_tolstring ? old_lua_tolstring(instance, index, strLen) : nullptr;
        }
    }
    return old_lua_tolstring ? old_lua_tolstring(instance, index, strLen) : nullptr;
}

// -------------------- thread where everything is ran --------------------
void *hack_thread(void *) {
    // wait for target lib to be loaded
    do {
        sleep(3);
    } while (!isLibraryLoaded(targetLibName));

    percyLog("Perseus: Target library loaded.");

    // load il2cpp functions and image
    loadil2cppfuncs();
    if (!image) {
        percyLog("Perseus: FATAL - il2cpp image not found; aborting hack_thread");
        return nullptr;
    }
    percyLog("Perseus: il2cpp image obtained.");

    // load lua functions
    loadluafuncs();
    if (!g_lua_funcs_loaded) {
        percyLog("Perseus: FATAL - lua functions not loaded; aborting hack_thread");
        return nullptr;
    }

    // resolve address of lua_tolstring (via GETLUAFUNC macro)
    void *lua_tolstring_addr = (void *)GETLUAFUNC("lua_tolstring");
    if (!lua_tolstring_addr) {
        percyLog("Perseus: FATAL - Could not find lua_tolstring address.");
        return nullptr;
    }

    // install hook
    hook(lua_tolstring_addr, (void *)lua_tolstring, (void **)&old_lua_tolstring);

    percyLog("Perseus: hook installed.");
    return nullptr;
}

// -------------------- JNI glue & config  --------------------
void getConfigPath(JNIEnv *env, jobject context) {
    jclass cls_Env = env->FindClass(OBFUSCATE("android/app/NativeActivity"));
    jmethodID mid = env->GetMethodID(cls_Env, OBFUSCATE("getExternalFilesDir"), OBFUSCATE("(Ljava/lang/String;)Ljava/io/File;"));
    jobject obj_File = env->CallObjectMethod(context, mid, NULL);
    jclass cls_File = env->FindClass(OBFUSCATE("java/io/File"));
    jmethodID mid_getPath = env->GetMethodID(cls_File, OBFUSCATE("getPath"), OBFUSCATE("()Ljava/lang/String;"));
    auto obj_Path = (jstring)env->CallObjectMethod(obj_File, mid_getPath);
    const char *path = env->GetStringUTFChars(obj_Path, nullptr);

    std::string route(path);
    configPath = route + tostr(OBFUSCATE("/Perseus.json"));
    skinPath = route + tostr(OBFUSCATE("/Skins.ini"));

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
    getConfigPath(env, context);

    if (access(configPath.c_str(), F_OK) != 0) {
        if (!writeConfigFile()) crash();
    }

    std::ifstream configFile(configPath);
    ordered_json configJson = nullptr;

    if (configFile.is_open()) {
        configJson = ordered_json::parse(configFile);
    } else {
        crash();
    }

    if (configJson == nullptr) crash();

    int i = 0;
    for (auto &[key, value] : configJson.items()) {
        if (i == 0 && !(key == tostr(OBFUSCATE("OriginalRepo")) && value == tostr(OBFUSCATE("github.com/Egoistically/Perseus"))))
            configJson = nullptr;
        if (i == 1 && !(key == tostr(OBFUSCATE("PieRepo")) && value == tostr(OBFUSCATE("github.com/4pii4/PiePerseus"))))
            configJson = nullptr;

        if (i > 1) {
            if (key == tostr(OBFUSCATE("Aircraft"))) {
                config.Aircraft.Enabled = jsonGetKey<bool>(value, OBFUSCATE("Enabled"), false);
                config.Aircraft.Accuracy = jsonGetKey<int>(value, OBFUSCATE("Accuracy"), -1);
                config.Aircraft.AccuracyGrowth = jsonGetKey<int>(value, OBFUSCATE("AccuracyGrowth"), -1);
                config.Aircraft.AttackPower = jsonGetKey<int>(value, OBFUSCATE("AttackPower"), -1);
                config.Aircraft.AttackPowerGrowth = jsonGetKey<int>(value, OBFUSCATE("AttackPowerGrowth"), -1);
                config.Aircraft.CrashDamage = jsonGetKey<int>(value, OBFUSCATE("CrashDamage"), -1);
                config.Aircraft.Hp = jsonGetKey<int>(value, OBFUSCATE("Hp"), -1);
                config.Aircraft.HpGrowth = jsonGetKey<int>(value, OBFUSCATE("HpGrowth"), -1);
                config.Aircraft.Speed = jsonGetKey<int>(value, OBFUSCATE("Speed"), -1);
            } else if (key == tostr(OBFUSCATE("Enemies"))) {
                config.Enemies.Enabled = jsonGetKey<bool>(value, OBFUSCATE("Enabled"), false);
                config.Enemies.AntiAir = jsonGetKey<int>(value, OBFUSCATE("AntiAir"), -1);
                config.Enemies.AntiAirGrowth = jsonGetKey<int>(value, OBFUSCATE("AntiAirGrowth"), -1);
                config.Enemies.AntiSubmarine = jsonGetKey<int>(value, OBFUSCATE("AntiSubmarine"), -1);
                config.Enemies.Armor = jsonGetKey<int>(value, OBFUSCATE("Armor"), -1);
                config.Enemies.ArmorGrowth = jsonGetKey<int>(value, OBFUSCATE("ArmorGrowth"), -1);
                config.Enemies.Cannon = jsonGetKey<int>(value, OBFUSCATE("Cannon"), -1);
                config.Enemies.CannonGrowth = jsonGetKey<int>(value, OBFUSCATE("CannonGrowth"), -1);
                config.Enemies.Evasion = jsonGetKey<int>(value, OBFUSCATE("Evasion"), -1);
                config.Enemies.EvasionGrowth = jsonGetKey<int>(value, OBFUSCATE("EvasionGrowth"), -1);
                config.Enemies.Hit = jsonGetKey<int>(value, OBFUSCATE("Hit"), -1);
                config.Enemies.HitGrowth = jsonGetKey<int>(value, OBFUSCATE("HitGrowth"), -1);
                config.Enemies.Hp = jsonGetKey<int>(value, OBFUSCATE("Hp"), -1);
                config.Enemies.HpGrowth = jsonGetKey<int>(value, OBFUSCATE("HpGrowth"), -1);
                config.Enemies.Luck = jsonGetKey<int>(value, OBFUSCATE("Luck"), -1);
                config.Enemies.LuckGrowth = jsonGetKey<int>(value, OBFUSCATE("LuckGrowth"), -1);
                config.Enemies.Reload = jsonGetKey<int>(value, OBFUSCATE("Reload"), -1);
                config.Enemies.ReloadGrowth = jsonGetKey<int>(value, OBFUSCATE("ReloadGrowth"), -1);
                config.Enemies.RemoveBuffs = jsonGetKey<bool>(value, OBFUSCATE("RemoveBuffs"), false);
                config.Enemies.RemoveEquipment = jsonGetKey<bool>(value, OBFUSCATE("RemoveEquipment"), false);
                config.Enemies.RemoveSkill = jsonGetKey<bool>(value, OBFUSCATE("RemoveSkill"), false);
                config.Enemies.Speed = jsonGetKey<int>(value, OBFUSCATE("Speed"), -1);
                config.Enemies.SpeedGrowth = jsonGetKey<int>(value, OBFUSCATE("SpeedGrowth"), -1);
                config.Enemies.Torpedo = jsonGetKey<int>(value, OBFUSCATE("Torpedo"), -1);
                config.Enemies.TorpedoGrowth = jsonGetKey<int>(value, OBFUSCATE("TorpedoGrowth"), -1);
            } else if (key == tostr(OBFUSCATE("Weapons"))) {
                config.Weapons.Enabled = jsonGetKey<bool>(value, OBFUSCATE("Enabled"), false);
                config.Weapons.Damage = jsonGetKey<int>(value, OBFUSCATE("Damage"), -1);
                config.Weapons.ReloadMax = jsonGetKey<int>(value, OBFUSCATE("ReloadMax"), -1);
            } else if (key == tostr(OBFUSCATE("Spoof"))) {
                config.Spoof.Enabled = jsonGetKey<bool>(value, OBFUSCATE("Enabled"), false);
                config.Spoof.name = jsonGetKey<std::string>(value, OBFUSCATE("Name"), stdstr(""));
                config.Spoof.id = jsonGetKey<std::string>(value, OBFUSCATE("Id"), stdstr(""));
                config.Spoof.lv = jsonGetKey<double>(value, OBFUSCATE("Lv"), -1);
            } else if (key == tostr(OBFUSCATE("Misc"))) {
                config.Misc.Enabled = jsonGetKey<bool>(value, OBFUSCATE("Enabled"), false);
                config.Misc.ExerciseGodmode = jsonGetKey<bool>(value, OBFUSCATE("ExerciseGodmode"), false);
                config.Misc.FastStageMovement = jsonGetKey<bool>(value, OBFUSCATE("FastStageMovement"), false);
                config.Misc.Skins = jsonGetKey<bool>(value, OBFUSCATE("Skins"), false);
                config.Misc.AutoRepeatLimit = jsonGetKey<int>(value, OBFUSCATE("AutoRepeatLimit"), -1);
                config.Misc.ChatCommands = jsonGetKey<bool>(value, OBFUSCATE("ChatCommands"), false);
                config.Misc.RemoveBBAnimation = jsonGetKey<bool>(value, OBFUSCATE("RemoveBBAnimation"), false);
                config.Misc.RemoveMoraleWarning = jsonGetKey<bool>(value, OBFUSCATE("RemoveMoraleWarning"), false);
                config.Misc.RemoveHardModeStatLimit = jsonGetKey<bool>(value, OBFUSCATE("RemoveHardModeStatLimit"), false);
                config.Misc.RemoveNSFWArts = jsonGetKey<bool>(value, OBFUSCATE("RemoveNSFWArts"), false);
                config.Misc.AllBlueprintsConvertible = jsonGetKey<bool>(value, OBFUSCATE("AllBlueprintsConvertible"), false);
            }
        }
        i++;
    }

    Toast(env, context, OBFUSCATE("Enjoy the feet, by @Egoistically, @pi.kt and @cmdtaves"), ToastLength::LENGTH_LONG);
    percyLog(OBFUSCATE("config loaded"));

    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);

    if (!toastCalled) {
        crash();
    }
}

int RegisterMain(JNIEnv *env) {
    JNINativeMethod methods[] = {
        {OBFUSCATE("init"), OBFUSCATE("(Landroid/content/Context;)V"), reinterpret_cast<void *>(init)},
    };
    jclass clazz = env->FindClass(OBFUSCATE("com/unity3d/player/UnityPlayerActivity"));
    if (!clazz)
        return JNI_ERR;
    if (env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0])) != 0)
        return JNI_ERR;

    return JNI_OK;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    if (RegisterMain(env) != 0)
        return JNI_ERR;
    return JNI_VERSION_1_6;
}
