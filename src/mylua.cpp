#include "mylua.hpp"

using namespace SSS;

static void name_env_objects(sol::table const& env)
{
    for (auto&& [key, object] : env) {
        if (object.get_type() != sol::type::userdata || key.get_type() != sol::type::string)
            continue;
        if (object.is<GL::Basic::Base>())
            object.as<GL::Basic::Base>().name = key.as<std::string>();
        else if (object.is<GL::PlaneRenderer::Chunk>())
            object.as<GL::PlaneRenderer::Chunk>().name = key.as<std::string>();
    }
}

Scene::Scene(std::string const& filename) try
    : path(g->lua_folder + filename)
{
    if (!env.valid()) {
        throw_exc("Could not initialize environment properly.");
        return;
    }
    env["filename"] = filename;
    env["is_loading"] = true;
    env["is_running"] = false;
    env["is_unloading"] = false;
    run();
    name_env_objects(env);
    env["is_loading"] = false;
    env["is_running"] = true;
}
CATCH_AND_RETHROW_FUNC_EXC;

Scene::~Scene()
{
    env["is_running"] = false;
    env["is_unloading"] = true;
    run();
}

bool Scene::run()
{
    auto result = g->lua.safe_script_file(path, env, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error const err = result;
        std::cout << "\n" << err.what() << "\n\n";
        return true;
    }
    return false;
}

void mylua_register_scripts()
{
    for (auto const& entry : std::filesystem::directory_iterator(g->lua_folder)) {
        std::string const path = entry.path().string();
        std::string const name = path.substr(path.rfind('/') + 1);
        // Ensure filename ends with .lua
        size_t dot = name.rfind('.');
        if (dot == std::string::npos || name.substr(dot) != ".lua")
            continue;
        if (g->lua_scenes.count(name) == 0)
            g->lua_scenes.try_emplace(name);
    }
}

static std::string complete_script_name(std::string const& name)
{
    size_t const ret = name.rfind(".lua");
    if (ret != std::string::npos && ret + 4 == name.length()) {
        return name;
    }
    return name + ".lua";
}

bool mylua_file_script(std::string const& path)
{
    std::string const real_path = g->lua_folder + complete_script_name(path);
    auto result = g->lua.safe_script_file(real_path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cout << "\n" << err.what() << "\n\n";
        return true;
    }
    return false;
}

bool mylua_run_active_scenes()
{
    bool ret = false;
    for (auto const& pair : g->lua_scenes) {
        if (pair.second)
            pair.second->run();
    }
    return ret;
}

bool mylua_load_scene(std::string const& scene_name)
{
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scenes.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    auto& scene = g->lua_scenes[script_name];
    if (scene) {
        LOG_FUNC_CTX_WRN("Given scene is already running", script_name);
        return true;
    }
    scene = std::make_unique<Scene>(script_name);
    return false;
}

bool mylua_unload_scene(std::string const& scene_name)
{
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scenes.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    auto& scene = g->lua_scenes[script_name];
    if (!scene) {
        LOG_FUNC_CTX_WRN("Given scene was not running", script_name);
        return true;
    }
    scene.reset();
    return false;
}

bool setup_lua()
{
    mylua_register_scripts();

    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua;
    lua_setup(lua);
    TR::lua_setup_TR(lua);
    GL::lua_setup_GL(lua);
    Audio::lua_setup_Audio(lua);
    lua["file_script"] = mylua_file_script;
    lua["load_scene"] = mylua_load_scene;
    lua["unload_scene"] = mylua_unload_scene;
    {
        auto plane = lua["GL"].get<sol::table>()["Plane"].get<sol::usertype<GL::Plane>>();
        plane["create"] = sol::overload(
            [](char const* str) {
                return GL::Plane::create(GL::Texture::create(g->assets_folder + str));
            },
            [](TR::Area const& area) {
                return GL::Plane::create(GL::Texture::create(area));
            }
        );
    }

    if (mylua_file_script("global_setup.lua"))
        return true;
    name_env_objects(lua.globals());

    return false;
}
