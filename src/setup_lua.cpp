#include <SSS/Commons/Lua.hpp>
#include <SSS/Text-Rendering/Lua.hpp>
#include <SSS/GL/Lua.hpp>
#include <SSS/Audio/Lua.hpp>

void lua_setup_other_libs(sol::state& lua)
{
    SSS::lua_setup(lua);
    SSS::TR::lua_setup_TR(lua);
    SSS::GL::lua_setup_GL(lua);
    SSS::Audio::lua_setup_Audio(lua);
}