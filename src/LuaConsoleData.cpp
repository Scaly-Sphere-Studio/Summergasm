#include "mylua.hpp"

std::vector<LuaConsoleData::_AppendBaseClassFunc> LuaConsoleData::_append_fns{
    _appendBaseClass<SSS::GL::ModelBase>,
    _appendBaseClass<SSS::GL::RendererBase>,
    _appendBaseClass<SSS::GL::PlaneRendererBase>
};

bool LuaConsoleData::recursive{ false };

LuaConsoleData::LuaConsoleData(sol::state& lua, sol::table table, sol::environment env,
    std::string name)
    : LuaConsoleData(sol::type::table, name)
{
    for (auto const& [k, v] : table) {

        if (k.get_type() != sol::type::string)
            continue;
        std::string const key = k.as<std::string>();
        sol::type const type = v.get_type();

        // Skip if empty key, or private value, or risk of infinite loop (base)
        if (key.empty() || key.find("_") == 0 || key.find("sol.") == 0 ||
            key == "base" || key == "new" || type == sol::type::nil ||
            type == sol::type::lightuserdata)
            continue;

        std::string const new_name = (name.empty() ? "" : name + '.') + key;
        emplace(key, LuaConsoleData(type, new_name));

        if (recursive && (type == sol::type::table || type == sol::type::userdata))
            at(key)._append(LuaConsoleData(lua, v, env, new_name));
    }

    if (table.is<sol::userdata>()) {
        type = sol::type::userdata;
        for (_AppendBaseClassFunc const& f : _append_fns) {
            f(lua, *this, env, table);
        }
        _appendUserdata(lua, env, table);
    }
}

void LuaConsoleData::_appendUserdata(sol::state& lua, sol::environment env, sol::userdata u)
{
    _append(LuaConsoleData(lua, u[sol::metatable_key], env, name));
    for (auto& [k, v] : *this) {
        sol::protected_function_result result = lua.safe_script(
            "return " + v.name, env, sol::script_pass_on_error);
        if (result.valid() && result.get_type() != sol::type::function) {
            v.type = result.get_type();
            if (v.type == sol::type::userdata)
                v._appendUserdata(lua, env, result);
        }
        else {
            v.separator = ':';
            v.name = name + ':' + k;
        }
    }
}

template <class T>
static void LuaConsoleData::_appendBaseClass(sol::state& lua, LuaConsoleData& data,
    sol::environment env, sol::userdata const& u)
{
    if (u.is<T>()) {
        std::string tmp = "tmp";
        for (size_t i = 1; lua[tmp] != sol::nil; ++i)
            tmp = "tmp" + std::to_string(i);
        lua[tmp] = u.as<T*>();
        data._appendUserdata(lua, env, lua[tmp]);
        lua[tmp] = sol::nil;
    }
};

std::string LuaConsoleData::to_string() const
{
    static int layer = -1;
    ++layer;
    std::string ret;
    for (int i = 0; i < layer; ++i)
        ret += "  ";
    ret += '{';
    for (auto const& [k, v] : *this) {
        ret += '\n';
        for (int i = 0; i < layer + 1; ++i)
            ret += "  ";
        ret += k;
        ret += " (" + std::to_string(static_cast<int>(v.type)) + ')';
        if (!v.empty()) {
            ret += "\n";
            ret += v;
        }
    }
    ret += '\n';
    for (int i = 0; i < layer; ++i)
        ret += "  ";
    ret += '}';
    --layer;
    return ret;
}