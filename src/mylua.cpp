#include "mylua.hpp"
#include "Dialog.hpp"

using namespace SSS;

sol::environment* mylua_console_env{ nullptr };

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
            TR::Area::Shared area = tex->getTextArea();
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

// Running scenes, in loading order: the last one is the current scene
static std::vector<std::string> scenes_order;

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
    size_t const n = filename.find('.');
    std::string const name = n < filename.size() ? filename.substr(0, n) : filename;
    g->lua["scenes"][name] = *env;
    (*env)["filename"] = filename;
    (*env)["is_loading"] = true;
    (*env)["is_running"] = false;
    (*env)["is_unloading"] = false;
    run();
    name_env_objects(*env);
    (*env)["is_loading"] = false;
    (*env)["is_running"] = true;
    scenes_order.push_back(filename);
}
CATCH_AND_RETHROW_FUNC_EXC;

static void empty_table(sol::table table)
{
    for (auto& [key, obj] : table) {
        // Window may not be set yet if a scene is unloaded during global_setup.lua
        if (g->window && obj.is<SSS::GL::RendererBase*>())
            g->window->removeRenderer(obj.as<SSS::GL::RendererBase*>()->getSharedBase());
        if (obj.get_type() == sol::type::table) {
            empty_table(obj);
            if (!obj.as<sol::table>().empty())
                LOG_MSG("THIS SHOULD BE EMPTY")
        }
        table[key] = sol::nil;
    }
}

Scene::~Scene()
{
    std::erase(scenes_order, filename);
    (*env)["is_running"] = false;
    (*env)["is_unloading"] = true;
    run();
    size_t const n = filename.find('.');
    std::string const name = n < filename.size() ? filename.substr(0, n) : filename;
    g->lua["scenes"][name] = sol::nil;
    // Don't leave the console pointing to a deleted env
    if (mylua_console_env == env.get())
        g->lua["console_reset_env"]();
    empty_table(*env);
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

// Main menu, loaded by default (see global_setup.lua)
static std::string const menu_scene = "menu.lua";

// Scenes can't be loaded or unloaded while they're running (a scene could
// destroy its own environment mid-script, e.g. a menu button loading a scene
// which unloads the menu): those requests are deferred until every scene ran.
static bool scenes_running = false;

// Dialog node of dialog(name), played over the current scene. Created on
// first use, it lives until the game exits (Lua may keep references to it).
static std::unique_ptr<Dialog> shared_dialog;

void mylua_free_dialog()
{
    shared_dialog.reset();
}

// Stops the previous conversation, resets the node's options and plays the
// given one, see Dialog::start()
static Dialog* mylua_dialog(std::string const& name)
{
    if (!shared_dialog)
        shared_dialog = std::make_unique<Dialog>();
    Dialog& dialog = *shared_dialog;
    dialog.stop();
    dialog.setOnFinished(nullptr);
    dialog.setEndText({});
    for (auto const part : { Dialog::Part::Characters, Dialog::Part::Box, Dialog::Part::Choices, Dialog::Part::Controls })
        dialog.setPartShown(part, true);
    dialog.start(name);
    return &dialog;
}
static std::vector<std::function<void()>> pending_requests;

static bool defer_request(std::function<void()> request)
{
    if (!scenes_running)
        return false;
    pending_requests.emplace_back(std::move(request));
    return true;
}

bool mylua_run_active_scenes()
{
    bool ret = false;
    scenes_running = true;
    for (auto const& pair : g->lua_scenes) {
        if (pair.second)
            pair.second->run();
    }
    scenes_running = false;
    // Requests may queue other ones (e.g. loading a scene unloads the menu)
    while (!pending_requests.empty()) {
        auto const requests = std::move(pending_requests);
        pending_requests.clear();
        for (auto const& request : requests) {
            try {
                request();
            }
            catch (std::exception const& e) {
                LOG_FUNC_ERR(e.what());
                ret = true;
            }
        }
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
    if (defer_request([script_name]() { mylua_load_scene(script_name); }))
        return false;
    auto& scene = g->lua_scenes[script_name];
    if (scene) {
        LOG_FUNC_CTX_WRN("Given scene is already running", script_name);
        return true;
    }
    scene = std::make_unique<Scene>(script_name);
    // Unload the menu once another scene is loaded, see mylua_return_to_menu()
    if (script_name != menu_scene) {
        auto const it = g->lua_scenes.find(menu_scene);
        if (it != g->lua_scenes.end() && it->second)
            it->second.reset();
    }
    return false;
}

bool mylua_unload_scene(std::string const& scene_name)
{
    // Empty name: unload the current (last loaded) scene
    if (scene_name.empty()) {
        if (scenes_order.empty()) {
            LOG_FUNC_WRN("No scene is running");
            return true;
        }
        return mylua_unload_scene(scenes_order.back());
    }
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scenes.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    if (defer_request([script_name]() { mylua_unload_scene(script_name); }))
        return false;
    auto& scene = g->lua_scenes[script_name];
    if (!scene) {
        LOG_FUNC_CTX_WRN("Given scene was not running", script_name);
        return true;
    }
    scene.reset();
    return false;
}

// Unloads every scene but the menu, and loads the menu if needed
bool mylua_return_to_menu()
{
    if (g->lua_scenes.count(menu_scene) == 0) {
        LOG_FUNC_CTX_WRN("Menu script wasn't registered", menu_scene);
        return true;
    }
    if (defer_request([]() { mylua_return_to_menu(); }))
        return false;
    for (auto& [name, scene] : g->lua_scenes) {
        if (name != menu_scene && scene)
            scene.reset();
    }
    if (shared_dialog)
        shared_dialog->stop();
    if (!g->lua_scenes[menu_scene])
        return mylua_load_scene(menu_scene);
    return false;
}

// Escape key of the main window. The current scene may handle it with an
// on_escape() function returning true (e.g. the dialog closing its log).
// Otherwise, returns to the menu. Returns false if nothing was done (the
// menu is the current scene): the caller then closes the game.
bool mylua_on_escape()
{
    // The dialog closes its log itself
    if (shared_dialog && shared_dialog->isVisible() && shared_dialog->isLogOpen())
        return true;
    if (!scenes_order.empty()) {
        std::string const current = scenes_order.back();
        sol::object const handler = g->lua_scenes.at(current)->getEnv()["on_escape"];
        if (handler.is<sol::protected_function>()) {
            sol::protected_function_result const result = handler.as<sol::protected_function>()();
            if (!result.valid()) {
                sol::error const err = result;
                LOG_CTX_ERR(current, err.what());
            }
            else if (result.get_type() == sol::type::boolean && result.get<bool>()) {
                return true;
            }
        }
        if (current == menu_scene)
            return false;
    }
    return !mylua_return_to_menu();
}

static void mylua_list_scenes()
{
    std::string scenes;
    for (auto const& [name, scene] : g->lua_scenes) {
        scenes += (scenes.empty() ? "" : ", ") + name.substr(0, name.rfind(".lua"));
        if (!scenes_order.empty() && name == scenes_order.back())
            scenes += " (current)";
        else if (scene)
            scenes += " (running)";
    }
    g->lua["print"]("Scenes: " + scenes);
}

void lua_setup_other_libs(sol::state& lua);

static std::map<std::string, sol::type> get_keys_from_table(sol::table const& table)
{
    std::map<std::string, sol::type> ret;
    for (auto const& [raw_key, obj] : table) {
        if (raw_key.get_type() != sol::type::string)
            continue;
        std::string const key = raw_key.as<std::string>();
        sol::type const obj_type = obj.get_type();
        // Skip if empty key, or private value, or risk of stack overflow (base)
        if (key.empty() || key.find("_") == 0 || key.find("sol.") == 0 ||
            key == "base" || key == "new" || obj_type == sol::type::nil ||
            obj_type == sol::type::lightuserdata)
            continue;
        ret[key] = obj_type;
        if (obj_type == sol::type::table) {
            auto append = get_keys_from_table(obj);
            for (auto const& [str, type] : append)
                ret[key + "." + str] = type;
        }
    }
    return ret;
}

std::map<std::string, sol::type> mylua_get_keys(sol::environment const& env) try
{
    auto ret = get_keys_from_table(env);
    for (auto& [key, type] : ret) {
        if (type == sol::type::userdata) {
            sol::userdata data = g->lua.safe_script("return " + key, env);
            auto const append = get_keys_from_table(data[sol::metatable_key]);
            for (auto const& [subkey, subtype] : append) {
                sol::protected_function_result res = g->lua.safe_script(
                    "return " + key + "." + subkey, env, sol::script_pass_on_error);
                if (res.valid() && res.get_type() != sol::type::function) {
                    ret[key + "." + subkey] = subtype;
                }
                else {
                    ret[key + ":" + subkey] = subtype;
                }
            }
        }
    }
    return ret;
}
catch (...) {
    return std::map<std::string, sol::type>();
}

// C++ bound functions carry no argument info in Lua, so they're documented here
struct LuaCommandHelp {
    std::string args;
    std::string description;
};
static std::map<std::string, LuaCommandHelp> const lua_commands_help{
    { "help",               { "[filter]",   "List commands & functions (aliases: h, ? [filter])" } },
    { "file_script",        { "path",       "Run resources/lua/<path>.lua once" } },
    { "load_scene",         { "scene_name",   "Load & run a scene, unloads the menu (alias: ls <scene>)" } },
    { "unload_scene",       { "[scene_name]", "Unload a scene, default: current one (alias: us [scene])" } },
    { "list_scenes",        { "",             "List scenes & which are running (alias: ls)" } },
    { "menu",               { "",             "Unload every scene & return to the menu (alias: m, key: Escape)" } },
    { "dialog",             { "name",         "Play resources/dialogs/<name>[.rpy|.txt] (or a direct path) over the current scene, replacing the previous one. Returns the Dialog" } },
    { "console_set_env",    { "scene_name", "Run console commands in a running scene's env" } },
    { "console_reset_env",  { "",           "Run console commands in the global env" } },
};

// Returns "name(arg1, arg2, ...)" for Lua functions, or an empty string otherwise
static std::string lua_function_signature(std::string const& name, sol::function const& f)
{
    sol::table const debug = g->lua["debug"];
    sol::table const info = debug["getinfo"](f, "Su");
    if (info["what"].get_or<std::string>("") != "Lua")
        return std::string();
    sol::function const getlocal = debug["getlocal"];
    int const nparams = info["nparams"].get_or(0);
    std::string args;
    for (int i = 1; i <= nparams; ++i) {
        if (!args.empty())
            args += ", ";
        args += getlocal(f, i).get<std::string>();
    }
    if (info["isvararg"].get_or(false))
        args += args.empty() ? "..." : ", ...";
    return name + '(' + args + ')';
}

static void mylua_help(sol::optional<std::string> filter)
{
    sol::function const print = g->lua["print"];
    auto const matches = [&](std::string const& name) {
        return !filter || name.find(*filter) != std::string::npos;
    };
    auto const print_aligned = [&](std::string const& signature, std::string const& description) {
        constexpr size_t width = 32;
        std::string line = "  " + signature;
        if (!description.empty())
            line += std::string(line.size() < width ? width - line.size() : 1, ' ') + description;
        print(line);
    };

    print("Commands:");
    for (auto const& [name, help] : lua_commands_help) {
        if (matches(name))
            print_aligned(name + '(' + help.args + ')', help.description);
    }

    // Lua functions of the console env (scene env first, then globals)
    std::map<std::string, std::string> functions;
    auto const add_functions = [&](sol::table const& table) {
        for (auto const& [key, value] : table) {
            if (key.get_type() != sol::type::string || value.get_type() != sol::type::function)
                continue;
            std::string const name = key.as<std::string>();
            if (name.empty() || name.find("_") == 0 || lua_commands_help.count(name) != 0
                || functions.count(name) != 0 || !matches(name))
                continue;
            std::string const signature = lua_function_signature(name, value.as<sol::function>());
            if (!signature.empty())
                functions.emplace(name, signature);
        }
    };
    add_functions(*mylua_console_env);
    add_functions(g->lua.globals());
    if (!functions.empty()) {
        print("Lua functions:");
        for (auto const& [name, signature] : functions)
            print_aligned(signature, "");
    }

    if (filter)
        return;
    mylua_list_scenes();
    print("Libraries: GL, TR, Audio, Parallax, vec3... (Tab to autocomplete, e.g. GL.Plane.)");
}

bool setup_lua()
{
    mylua_register_scripts();
    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua;
    lua_setup_other_libs(lua);

    lua["file_script"] = mylua_file_script;
    // C++ functions return true on error, Lua ones return true on success
    lua["load_scene"] = [](std::string const& scene_name) {
        return !mylua_load_scene(scene_name);
    };
    lua["unload_scene"] = [](sol::optional<std::string> scene_name) {
        return !mylua_unload_scene(scene_name.value_or(""));
    };
    lua["list_scenes"] = mylua_list_scenes;
    lua["menu"] = []() {
        return !mylua_return_to_menu();
    };
    lua["dialog"] = mylua_dialog;
    lua["help"] = mylua_help;
    lua["scenes"].get_or_create<sol::table>();

    lua["console_set_env"] = [](char const* scene_name) {
        std::string const key = complete_script_name(scene_name);
        if (g->lua_scenes.count(key) == 0)
            SSS::throw_exc("No scene with this given name.");
        auto& scene = g->lua_scenes.at(key);
        if (!scene)
            SSS::throw_exc("Scene isn't running");
        mylua_console_env = &scene->getEnv();
    };
    lua["console_reset_env"] = []() {
        mylua_console_env = reinterpret_cast<sol::environment*>(&g->lua.globals());
    };
    lua["console_reset_env"]();

    auto parallax = lua.new_usertype<Parallax>("Parallax", sol::factories(
        []() { return Parallax::create(); },
        [](GL::Camera* cam) { return Parallax::create(GL::Camera::get(cam)); },
        [](GL::Camera* cam, bool clear) { return Parallax::create(GL::Camera::get(cam), clear); }
    ), sol::base_classes, sol::bases<GL::PlaneRenderer, GL::RendererBase, Base>());
    parallax["width"] = sol::property(&Parallax::getWidth);
    parallax["speed"] = &Parallax::speed;
    parallax["pause"] = &Parallax::pause;
    parallax["play"] = &Parallax::play;
    parallax["toggle"] = &Parallax::toggle;

    // Ren'Py dialog node, see Dialog.hpp & dialog.lua
    // Dialog.new() is idle, Dialog.new(name) starts the conversation at once
    // (see also dialog(name), playing on a shared node).
    // Started dialogs are updated by the main loop (Dialog::updateAll).
    auto dialog = lua.new_usertype<Dialog>("Dialog", sol::factories(
        []() { return std::make_unique<Dialog>(); },
        [](std::string const& name) {
            auto d = std::make_unique<Dialog>();
            d->start(name);
            return d;
        }
    ));
    dialog["start"] = &Dialog::start;
    dialog["stop"] = &Dialog::stop;
    dialog["hide"] = &Dialog::hide;
    dialog["show"] = &Dialog::show;
    dialog["active"] = sol::readonly_property(&Dialog::isActive);
    dialog["hidden"] = sol::readonly_property(&Dialog::isHidden);
    dialog["visible"] = sol::readonly_property(&Dialog::isVisible);
    dialog["log_open"] = sol::readonly_property(&Dialog::isLogOpen);
    dialog["end_text"] = sol::property(&Dialog::getEndText, &Dialog::setEndText);
    auto const part_property = [](Dialog::Part part) {
        return sol::property(
            [part](Dialog const& d) { return d.isPartShown(part); },
            [part](Dialog& d, bool shown) { d.setPartShown(part, shown); });
    };
    dialog["show_characters"] = part_property(Dialog::Part::Characters);
    dialog["show_box"] = part_property(Dialog::Part::Box);
    dialog["show_choices"] = part_property(Dialog::Part::Choices);
    dialog["show_controls"] = part_property(Dialog::Part::Controls);
    // Image path, or nil
    dialog["background_image"] = sol::readonly_property([](Dialog const& d) -> sol::optional<std::string> {
        std::string image = d.backgroundImage();
        if (image.empty()) return sol::nullopt;
        return image;
    });
    // 0xRRGGBB, or nil
    dialog["background_color"] = sol::readonly_property([](Dialog const& d) -> sol::optional<uint32_t> {
        if (auto const color = d.backgroundColor()) return *color;
        return sol::nullopt;
    });
    // function () or nil, called once a conversation is over
    dialog["on_finished"] = sol::writeonly_property([](Dialog& d, sol::object callback) {
        if (!callback.is<sol::protected_function>()) {
            d.setOnFinished(nullptr);
            return;
        }
        d.setOnFinished([f = callback.as<sol::protected_function>()]() {
            sol::protected_function_result const result = f();
            if (!result.valid()) {
                sol::error const err = result;
                LOG_CTX_ERR("Dialog.on_finished", err.what());
            }
        });
    });

    if (mylua_file_script("global_setup.lua"))
        return true;

    g->window = g->lua["window"];
    g->ui_window = g->lua["ui_window"];

    name_env_objects(lua.globals());

    return false;
}
