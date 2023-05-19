#include "mylua.hpp"

using namespace SSS;

static void name_env_objects(sol::table const& env)
{
    for (auto&& [key, object] : env) {
        if (object.get_type() != sol::type::userdata || key.get_type() != sol::type::string)
            continue;
        std::string const key_s = key.as<std::string>();
        if (key_s.find("sol.") == 0)
            continue;
        if (object.is<Base>()) {
            object.as<Base>().setName(key_s);
            LOG_MSG(object.as<Base>().getName())
        }
    }
    for (auto const& tex : SSS::GL::Texture::getInstances()) {
        if (!tex->getName().empty())
            continue;
        switch (tex->getType()) {
        case SSS::GL::Texture::Type::Raw: {
            std::string const path = tex->getFilepath();
            size_t const slash = path.rfind('/');
            size_t const dot = path.rfind('.');
            if (slash != std::string::npos && dot != std::string::npos)
                tex->setName(path.substr(slash + 1, dot - slash - 1));
            else
                tex->setName(path);
        }   break;
        case SSS::GL::Texture::Type::Text: {
            TR::Area const* area = tex->getTextArea();
            if (area && !area->getName().empty()) {
                tex->setName(area->getName() + "");
            }
        }   break;
        }
        if (tex->getName().empty()) {
            static int count = 1;
            tex->setName("unnamed_texture_" + std::to_string(count++));
        }
    }
}

Scene::Scene(std::string const& filename_) try
    : filename(filename_), path(g->lua_folder + filename_)
{
    env = std::make_unique<sol::environment>(g->lua, sol::create, g->lua.globals());
    if (!env->valid()) {
        throw_exc("Could not initialize environment properly.");
    }
    auto result = g->lua.load_file(path);
    if (!result.valid()) {
        auto err = result.get<sol::error>();
        throw_exc(CONTEXT_MSG("Couldn't load file", err.what()));
    }
    script = readFile(path);
    (*env)["filename"] = filename;
    (*env)["is_loading"] = true;
    (*env)["is_running"] = false;
    (*env)["is_unloading"] = false;
    run();
    name_env_objects(*env);
    (*env)["is_loading"] = false;
    (*env)["is_running"] = true;
}
CATCH_AND_RETHROW_FUNC_EXC;

Scene::~Scene()
{
    (*env)["is_running"] = false;
    (*env)["is_unloading"] = true;
    run();
    env.reset();
    g->lua.collect_garbage();
}

bool Scene::run()
{
    auto result = g->lua.do_string(script, *env);
    if (!result.valid()) {
        sol::error const err = result;
        LOG_CTX_ERR(filename, err.what());
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

void lua_setup_other_libs(sol::state& lua);

bool setup_lua()
{
    mylua_register_scripts();
    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua;
    lua.new_usertype<Base>("Base", sol::no_constructor);
    lua_setup_other_libs(lua);
    lua["file_script"] = mylua_file_script;
    lua["load_scene"] = mylua_load_scene;
    lua["unload_scene"] = mylua_unload_scene;
    {
        auto window = lua["GL"].get<sol::table>()["Window"].get<sol::usertype<GL::Window>>();
        window["getHoveredPlane"] = &GL::Window::getHovered<GL::Plane>;

        auto parallax = lua.new_usertype<Parallax>("Parallax", sol::factories(
            sol::resolve<Parallax::Shared()>(Parallax::create),
            [](GL::Camera* cam) { return Parallax::create(GL::Camera::get(cam)); },
            [](GL::Camera* cam, bool clear) { return Parallax::create(GL::Camera::get(cam), clear); }
        ), sol::base_classes, sol::bases<GL::PlaneRendererBase, GL::RendererBase, Base>());
        parallax["width"] = sol::property(&Parallax::getWidth);
        parallax["speed"] = &Parallax::speed;
        parallax["pause"] = &Parallax::pause;
        parallax["play"] = &Parallax::play;
        parallax["toggle"] = &Parallax::toggle;
    }

    if (mylua_file_script("global_setup.lua"))
        return true;

    g->window = g->lua["window"];
    g->ui_window = g->lua["ui_window"];

    name_env_objects(lua.globals());

    return false;
}
