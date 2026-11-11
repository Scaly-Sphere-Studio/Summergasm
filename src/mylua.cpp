#include "mylua.hpp"
#include "Dialog.hpp"
#include "MapOverlay.hpp"
#include "MapContent.hpp"
#include "GameState.hpp"
#include "Settings.hpp"
#include "SignalManager.hpp"
#include "Transition.hpp"
#include "renpy/RenpyParser.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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
    env = std::make_unique<sol::environment>(g->lua(), sol::create, g->lua().globals());
    if (!env->valid()) {
        throw_exc("Could not initialize environment properly.");
    }
    auto result = g->lua().load_file(path);
    if (!result.valid()) {
        auto err = result.get<sol::error>();
        throw_exc(CONTEXT_MSG("Couldn't load file", err.what()));
    }
    script = readFile(path);
    std::error_code ec;
    watched_time = std::filesystem::last_write_time(path, ec);
    size_t const n = filename.find('.');
    std::string const name = n < filename.size() ? filename.substr(0, n) : filename;
    g->lua()["scenes"][name] = *env;
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
    g->lua()["scenes"][name] = sol::nil;
    // Don't leave the console pointing to a deleted env
    if (mylua_console_env == env.get())
        g->lua()["console_reset_env"]();
    empty_table(*env);
    env.reset();
    g->lua().collect_garbage();
}

bool Scene::run()
{
    auto result = g->lua().do_string(script, *env);
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
    auto result = g->lua().safe_script_file(real_path, sol::script_pass_on_error);
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
// A dialog node reporting its signals to the signal manager
static std::unique_ptr<Dialog> new_dialog()
{
    auto d = std::make_unique<Dialog>();
    d->setOnSignal(signals::dispatch);
    return d;
}

static Dialog* mylua_dialog(std::string const& name)
{
    if (!shared_dialog)
        shared_dialog = std::make_unique<Dialog>();
    Dialog& dialog = *shared_dialog;
    dialog.stop();
    dialog.setOnFinished(nullptr);
    dialog.setOnSignal(signals::dispatch);
    dialog.setEndText({});
    for (auto const part : { Dialog::Part::Characters, Dialog::Part::Box,
                             Dialog::Part::Choices, Dialog::Part::Controls })
        dialog.setPartShown(part, true);
    dialog.start(name);
    return &dialog;
}
// Game values (see GameState.hpp) in Lua: strings, integers & booleans
static sol::object to_lua(sol::state_view lua, game_state::Value const& value)
{
    return std::visit([&](auto const& v) { return sol::make_object(lua, v); }, value);
}

// Calls f(name, args...) for a signal, true if it returned true
static bool call_lua_signal_handler(sol::protected_function const& f, renpy::Signal const& signal, char const* context)
{
    sol::state_view const lua = f.lua_state();
    std::vector<sol::object> args;
    for (auto const& arg : signal.args)
        args.push_back(to_lua(lua, arg));
    sol::protected_function_result const result = f(signal.name, sol::as_args(args));
    if (!result.valid()) {
        sol::error const err = result;
        LOG_CTX_ERR(context, err.what());
        return false;
    }
    return result.get_type() == sol::type::boolean && result.get<bool>();
}

static game_state::Value from_lua(sol::object const& object)
{
    switch (object.get_type()) {
    case sol::type::string:
        return object.as<std::string>();
    case sol::type::boolean:
        return object.as<bool>();
    case sol::type::number: {
        double const number = object.as<double>();
        if (number == std::floor(number))
            return static_cast<int64_t>(number);
        break;
    }
    default:
        break;
    }
    SSS::throw_exc("game values are strings, integers or booleans");
}

// game.vars: index it to read & write the game values, nil erases one
struct GameVars {};

static std::vector<std::function<void()>> pending_requests;

static bool defer_request(std::function<void()> request)
{
    if (!scenes_running)
        return false;
    pending_requests.emplace_back(std::move(request));
    return true;
}

// Absolute paths are kept, others are looked up in the given folder
static std::string resolve_audio(std::string const& folder, std::string const& file)
{
    std::filesystem::path const path(file);
    return path.is_absolute() ? file : (std::filesystem::path(folder) / path).string();
}

// Frame clock, as Lua globals: dt (seconds since the previous frame, capped
// so a stall (loading, window drag) doesn't make animations jump) & time
// (seconds since the first frame, the sum of every dt)
static void update_lua_clock()
{
    using clock = std::chrono::steady_clock;
    static clock::time_point last = clock::now();
    static double time = 0.0;
    clock::time_point const now = clock::now();
    double const dt = std::min(std::chrono::duration<double>(now - last).count(), 0.1);
    last = now;
    time += dt;
    g->lua()["dt"] = dt;
    g->lua()["time"] = time;
}

bool mylua_run_active_scenes()
{
    bool ret = false;
    update_lua_clock();
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

// The file is compiled first: a script that doesn't compile leaves the running
// scene untouched. The scene keeps its place in the loading order (so the
// current scene stays current), and the console stays in its env.
bool mylua_reload_scene(std::string const& scene_name)
{
    // Empty name: reload the current (last loaded) scene
    if (scene_name.empty()) {
        if (scenes_order.empty()) {
            LOG_FUNC_WRN("No scene is running");
            return true;
        }
        return mylua_reload_scene(scenes_order.back());
    }
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scenes.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    if (defer_request([script_name]() { mylua_reload_scene(script_name); }))
        return false;
    auto& scene = g->lua_scenes[script_name];
    if (!scene) {
        LOG_FUNC_CTX_WRN("Given scene was not running", script_name);
        return true;
    }
    sol::load_result const compiled = g->lua().load_file(scene->getPath());
    if (!compiled.valid()) {
        sol::error const err = compiled;
        LOG_CTX_ERR(script_name, std::string("Not reloaded, the script doesn't compile:\n") + err.what());
        return true;
    }
    bool const console_in_scene = mylua_console_env == &scene->getEnv();
    size_t const order = std::find(scenes_order.begin(), scenes_order.end(), script_name) - scenes_order.begin();
    scene.reset();
    scene = std::make_unique<Scene>(script_name);
    std::erase(scenes_order, script_name);
    scenes_order.insert(scenes_order.begin() + std::min(order, scenes_order.size()), script_name);
    if (console_in_scene)
        mylua_console_env = &scene->getEnv();
    LOG_CTX_MSG("Scene reloaded", script_name);
    return false;
}

// Toggled by auto_reload([on])
static bool watch_scenes = true;

void mylua_watch_scenes()
{
    static auto next_check = std::chrono::steady_clock::now();
    auto const now = std::chrono::steady_clock::now();
    if (!watch_scenes || scenes_order.empty() || now < next_check)
        return;
    next_check = now + std::chrono::seconds(1);
    std::string const current = scenes_order.back();
    Scene& scene = *g->lua_scenes.at(current);
    std::error_code ec;
    auto const time = std::filesystem::last_write_time(scene.getPath(), ec);
    if (ec || time == scene.watched_time)
        return;
    // Once per change: a script that doesn't compile waits for the next save
    scene.watched_time = time;
    try {
        mylua_reload_scene(current);
    }
    catch (std::exception const& e) {
        LOG_FUNC_ERR(e.what());
    }
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
    // Closes the map first
    if (MapOverlay::isOpen()) {
        MapOverlay::close();
        return true;
    }
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
    g->lua()["print"]("Scenes: " + scenes);
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
            sol::userdata data = g->lua().safe_script("return " + key, env);
            auto const append = get_keys_from_table(data[sol::metatable_key]);
            for (auto const& [subkey, subtype] : append) {
                sol::protected_function_result res = g->lua().safe_script(
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
    { "load_scene",         { "scene_name, [options]", "Load & run a scene, unloads the menu (alias: ls <scene>). options.transition: see transition()" } },
    { "unload_scene",       { "[scene_name]", "Unload a scene, default: current one (alias: us [scene])" } },
    { "reload_scene",       { "[scene_name]", "Reload a scene if its script compiles, default: current one (alias: rs [scene])" } },
    { "auto_reload",        { "[on]",         "Reload the current scene when its file changes (if it compiles), toggles without argument. On by default" } },
    { "list_scenes",       { "",             "List scenes & which are running (alias: ls)" } },
    { "menu",               { "[options]",    "Unload every scene & return to the menu (alias: m, key: Escape). options: see load_scene()" } },
    { "transition",         { "name, [duration, options]", "Screen transition, see list_transitions(). options: color, on_midpoint, on_done; shader transitions also take palette, strips, softness, angle, mask. Single: covers the screen then reveals it, on_midpoint at the midpoint. Dual: fades the previous frame into the new scene" } },
    { "transition_stop",    { "",             "Stop the running transition, its callbacks are not called" } },
    { "list_transitions",   { "",             "List the transitions, with their kind" } },
    { "transition_cover",   { "x",            "Cover-then-reveal curve of a single transition layer, 0 to 1 then 0 (for layer functions of x)" } },
    { "transition_smooth",  { "x",            "Smoothstep of x clamped to [0, 1], the easing of the dual transitions" } },
    { "corner_radius",      { "aspect",       "Radius reaching the farthest corner of the screen (screen height 1), for transition layers" } },
    { "dialog",             { "name",         "Play resources/dialogs/<name>[.rpy|.txt] (or a direct path) over the current scene, replacing the previous one. Returns the Dialog" } },
    { "debug_mouths",       { "[on]",         "Show each character's mouth (circle) & direction (line) used by the speech bubble tails, toggles without argument. Reloads resources/characters/*.json" } },
    { "dialog_tails",       { "[on]",         "Draw the speech bubble tails, toggles without argument. Saved in settings.ini" } },
    { "grey_characters",    { "[on]",         "Draw the characters not talking in black & white, toggles without argument. Saved in settings.ini" } },
    { "game.save",          { "[name]",       "Save the game values (game.vars) in saves/<name>.json, default: save" } },
    { "game.load",          { "[name]",       "Load the game values of saves/<name>.json, default: save" } },
    { "game.reset",         { "",             "Erase every game value (game.vars)" } },
    { "list_assets",        { "folder",       "Sorted file names in resources/assets/<folder>, empty if missing" } },
    { "console_set_env",    { "scene_name", "Run console commands in a running scene's env" } },
    { "console_reset_env",  { "",           "Run console commands in the global env" } },
    // Depth of field of every SceneRenderer of the window, distances from the camera
    { "dof",                { "[on]",                   "Depth of field, toggles without argument" } },
    { "focus",              { "[distance, band_width]", "Sharp band centered on distance (width kept if omitted), prints it without argument. focus(0, 0): all blurred" } },
    { "focus_near",         { "distance",               "Near end of the sharp band (pushes the far one if needed)" } },
    { "focus_far",          { "distance",               "Far end of the sharp band (pushes the near one if needed). focus_near(0) focus_far(math.huge): all sharp" } },
    { "focus_move",         { "delta",                  "Move the sharp band, away from the camera if positive" } },
};

// Sorted names of the files in assets/<folder>, empty if it doesn't exist
static std::vector<std::string> mylua_list_assets(std::string const& folder)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(g->assets_folder + folder, ec)) {
        if (entry.is_regular_file(ec))
            names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Gradient of stops { t, 0xRRGGBB | vec3 (sRGB), [alpha = 1], [ease = "Linear"] },
// the ease applying from that stop to the next
static SSS::Math::Gradient<glm::vec4> lua_to_gradient(sol::table const& stops)
{
    using SSS::Math::EaseType;
    static std::map<std::string, EaseType> const eases{
        { "Linear", EaseType::Linear },
        { "SineEaseIn", EaseType::SineEaseIn },
        { "SineEaseOut", EaseType::SineEaseOut },
        { "SineEaseInOut", EaseType::SineEaseInOut },
        { "QuadEaseIn", EaseType::QuadEaseIn },
        { "QuadEaseOut", EaseType::QuadEaseOut },
        { "QuadEaseInOut", EaseType::QuadEaseInOut },
        { "CubicEaseIn", EaseType::CubicEaseIn },
        { "CubicEaseOut", EaseType::CubicEaseOut },
        { "CubicEaseInOut", EaseType::CubicEaseInOut },
    };
    SSS::Math::Gradient<glm::vec4> gradient;
    for (size_t i = 1; i <= stops.size(); ++i) {
        sol::optional<sol::table> const stop = stops[i];
        if (!stop)
            continue;
        float const t = stop->get_or(1, 0.f);
        glm::vec3 rgb(1.f);
        sol::object const color = (*stop)[2];
        if (color.is<glm::vec3>())
            rgb = color.as<glm::vec3>();
        else if (color.get_type() == sol::type::number) {
            uint32_t const hex = color.as<uint32_t>();
            rgb = glm::vec3((hex >> 16) & 0xff, (hex >> 8) & 0xff, hex & 0xff) / 255.f;
        }
        float const alpha = stop->get_or(3, 1.f);
        auto const ease = eases.find(stop->get_or<std::string>(4, "Linear"));
        gradient.push({ t, glm::vec4(rgb, alpha) },
            ease == eases.cend() ? EaseType::Linear : ease->second);
    }
    return gradient;
}

// Applies func to every SceneRenderer of the window, then prints their focus band
template <typename Func>
static void mylua_focus(Func&& func)
{
    sol::function const print = g->lua()["print"];
    std::vector<std::shared_ptr<SceneRenderer>> renderers;
    if (g->window) {
        for (auto const& renderer : g->window->getRenderers()) {
            if (auto scene = std::dynamic_pointer_cast<SceneRenderer>(renderer))
                renderers.emplace_back(std::move(scene));
        }
    }
    if (renderers.empty()) {
        print("No SceneRenderer in the window");
        return;
    }
    for (auto const& renderer : renderers) {
        func(*renderer);
        print(std::format("Depth of field {}, sharp from {:.0f} to {:.0f} (distance {:.0f}, band width {:.0f})",
            renderer->depth_of_field ? "on" : "off", renderer->getFocusNear(), renderer->getFocusFar(),
            renderer->getFocusDistance(), renderer->getFocusBandWidth()));
    }
}

// Returns "name(arg1, arg2, ...)" for Lua functions, or an empty string otherwise
static std::string lua_function_signature(std::string const& name, sol::function const& f)
{
    sol::table const debug = g->lua()["debug"];
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
    sol::function const print = g->lua()["print"];
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
    add_functions(g->lua().globals());
    if (!functions.empty()) {
        print("Lua functions:");
        for (auto const& [name, signature] : functions)
            print_aligned(signature, "");
    }

    if (filter)
        return;
    mylua_list_scenes();
    print("Libraries: GL, TR, Audio, SceneRenderer, vec3... (Tab to autocomplete, e.g. GL.Plane.)");
}

// 0xRRGGBB to sRGB
static glm::vec3 lua_rgb(int hex)
{
    return glm::vec3((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF) / 255.f;
}

// Lua function as a callback: its errors are logged, not thrown
static std::function<void()> lua_callback(sol::protected_function fn, std::string const& what)
{
    return [fn, what]() {
        sol::protected_function_result const result = fn();
        if (!result.valid()) {
            sol::error const err = result;
            LOG_CTX_ERR(what, err.what());
        }
    };
}

// Options of transition() and of the scene switches, see lua_commands_help
static transition::Options lua_transition_options(sol::table const& t)
{
    transition::Options options;
    if (auto const hex = t.get<sol::optional<int>>("color"))
        options.color = lua_rgb(*hex);
    if (auto const palette = t.get<sol::optional<sol::table>>("palette")) {
        for (auto const& [unused_key, value] : *palette) {
            if (value.is<int>())
                options.palette.push_back(lua_rgb(value.as<int>()));
        }
    }
    if (auto const mask = t.get<sol::optional<std::string>>("mask"))
        options.mask = *mask;
    if (auto const strips = t.get<sol::optional<int>>("strips"))
        options.strips = *strips;
    if (auto const softness = t.get<sol::optional<float>>("softness"))
        options.softness = *softness;
    if (auto const angle = t.get<sol::optional<float>>("angle"))
        options.angle = *angle;
    if (auto const fn = t.get<sol::optional<sol::protected_function>>("on_midpoint"))
        options.on_midpoint = lua_callback(*fn, "transition on_midpoint");
    if (auto const fn = t.get<sol::optional<sol::protected_function>>("on_done"))
        options.on_done = lua_callback(*fn, "transition on_done");
    return options;
}

// Scene switch of load_scene() & menu(): with options.transition, it runs at the
// midpoint of that transition (the screen is covered, or the frame is kept for
// dual ones). Returns true on error
static bool lua_switch_scene(sol::optional<sol::table> const& opts, std::function<bool()> const& switch_scene)
{
    if (!opts)
        return switch_scene();
    std::string const name = opts->get_or<std::string>("transition", "");
    if (name.empty())
        return switch_scene();
    transition::Options options = lua_transition_options(*opts);
    options.on_midpoint = [switch_scene, user = std::move(options.on_midpoint)]() {
        switch_scene();
        if (user)
            user();
    };
    // Unknown transition: the scene is switched without it
    sol::optional<float> const seconds = opts->get<sol::optional<float>>("duration");
    std::optional<float> const duration = seconds ? std::optional<float>(*seconds) : std::nullopt;
    if (transition::start(name, duration, std::move(options)))
        return switch_scene();
    return false;
}

// A number of a layer: a constant, or a function of the progress (0 to 1).
// Errors are thrown, see mylua_load_transition()
static transition::Curve lua_curve(sol::object const& value, std::string const& what)
{
    if (value.get_type() == sol::type::number) {
        float const v = value.as<float>();
        return [v](float, float) { return v; };
    }
    if (value.get_type() == sol::type::function) {
        sol::protected_function const fn = value;
        // Logged once, then the parameter stays at 0
        auto reported = std::make_shared<bool>(false);
        return [fn, what, reported](float x, float aspect) -> float {
            sol::protected_function_result const result = fn(x, aspect);
            if (!result.valid()) {
                if (!*reported) {
                    *reported = true;
                    sol::error const err = result;
                    LOG_CTX_ERR(what, err.what());
                }
                return 0.f;
            }
            return result.get_type() == sol::type::number ? result.get<float>() : 0.f;
        };
    }
    throw std::runtime_error(what + ": expected a number or a function");
}

// A color of a layer: 0xRRGGBB, or a function of the progress returning it
static transition::ColorCurve lua_color(sol::object const& value, std::string const& what)
{
    if (value.get_type() == sol::type::number) {
        glm::vec3 const rgb = lua_rgb(value.as<int>());
        return [rgb](float, float) { return rgb; };
    }
    if (value.get_type() == sol::type::function) {
        sol::protected_function const fn = value;
        auto reported = std::make_shared<bool>(false);
        return [fn, what, reported](float x, float aspect) -> glm::vec3 {
            sol::protected_function_result const result = fn(x, aspect);
            if (!result.valid()) {
                if (!*reported) {
                    *reported = true;
                    sol::error const err = result;
                    LOG_CTX_ERR(what, err.what());
                }
                return glm::vec3(0.f);
            }
            return result.get_type() == sol::type::number ? lua_rgb(result.get<int>()) : glm::vec3(0.f);
        };
    }
    throw std::runtime_error(what + ": expected a color or a function");
}

// Index of a texture file in the transition's list, added if new
static int lua_texture_slot(std::string const& file, std::vector<std::string>& textures)
{
    auto const it = std::find(textures.begin(), textures.end(), file);
    if (it != textures.end())
        return static_cast<int>(it - textures.begin());
    if (textures.size() >= transition::max_textures)
        throw std::runtime_error("at most " + std::to_string(transition::max_textures) + " textures per transition");
    textures.push_back(file);
    return static_cast<int>(textures.size() - 1);
}

// A field: shape, its parameters, invert and texture. Used by layers and masks
static transition::Field lua_field(sol::table const& t, std::string const& what, std::vector<std::string>& textures)
{
    static std::map<std::string, transition::Shape> const shapes{
        { "full", transition::Shape::Full },
        { "circle", transition::Shape::Circle },
        { "box", transition::Shape::Box },
        { "ring", transition::Shape::Ring },
        { "diamond", transition::Shape::Diamond },
        { "heart", transition::Shape::Heart },
        { "ramp", transition::Shape::Ramp },
        { "noise", transition::Shape::Noise },
        { "texture", transition::Shape::Texture },
    };
    std::string const name = t.get<sol::optional<std::string>>("shape").value_or("full");
    auto const it = shapes.find(name);
    if (it == shapes.end())
        throw std::runtime_error(what + ": unknown shape '" + name + "'");

    transition::Field field;
    field.shape = it->second;
    field.invert = t.get<sol::optional<bool>>("invert").value_or(false);
    if (auto const file = t.get<sol::optional<std::string>>("texture"))
        field.texture = lua_texture_slot(*file, textures);
    if (field.shape == transition::Shape::Texture && field.texture < 0)
        throw std::runtime_error(what + ": the texture shape needs a texture");

    static constexpr std::array<char const*, transition::param_count> names{
        "x", "y", "radius", "width", "height", "rotation", "round", "thickness", "angle", "offset", "threshold", "softness",
    };
    for (size_t k = 0; k != names.size(); ++k) {
        sol::object const value = t.get<sol::object>(names[k]);
        if (value.get_type() != sol::type::nil)
            field.params[k] = lua_curve(value, what + "." + names[k]);
    }
    return field;
}

// The fill of a layer: a texture, a gradient, or a solid color (the cover color if unset)
static transition::Fill lua_fill(sol::optional<sol::table> const& t, std::string const& what, std::vector<std::string>& textures)
{
    transition::Fill fill;
    if (!t)
        return fill;
    if (auto const file = t->get<sol::optional<std::string>>("texture")) {
        fill.kind = transition::Fill::Kind::Texture;
        fill.texture = lua_texture_slot(*file, textures);
        return fill;
    }
    if (t->get<sol::optional<bool>>("previous").value_or(false)) {
        fill.kind = transition::Fill::Kind::Previous;
        sol::object const shift_x = t->get<sol::object>("shift_x");
        if (shift_x.get_type() != sol::type::nil)
            fill.shift_x = lua_curve(shift_x, what + ".fill.shift_x");
        sol::object const shift_y = t->get<sol::object>("shift_y");
        if (shift_y.get_type() != sol::type::nil)
            fill.shift_y = lua_curve(shift_y, what + ".fill.shift_y");
        return fill;
    }
    if (auto const gradient = t->get<sol::optional<sol::table>>("gradient")) {
        for (size_t i = 1; i <= gradient->size(); ++i)
            fill.stops.push_back(lua_rgb(gradient->get<int>(static_cast<int>(i))));
        if (fill.stops.empty() || fill.stops.size() > 4)
            throw std::runtime_error(what + ": a gradient needs 1 to 4 colors");
        sol::object const angle = t->get<sol::object>("angle");
        if (angle.get_type() != sol::type::nil)
            fill.angle = lua_curve(angle, what + ".fill.angle");
        if (fill.stops.size() == 1) {
            glm::vec3 const rgb = fill.stops.front();
            fill.stops.clear();
            fill.color = [rgb](float, float) { return rgb; };
        }
        else {
            fill.kind = transition::Fill::Kind::Gradient;
        }
        return fill;
    }
    sol::object const color = t->get<sol::object>("color");
    if (color.get_type() != sol::type::nil)
        fill.color = lua_color(color, what + ".fill.color");
    return fill;
}

// The layers of a single transition, drawn in order (the first is the lowest)
static std::vector<transition::Layer> lua_layers(sol::table const& layers, std::vector<std::string>& textures)
{
    std::vector<transition::Layer> out;
    for (size_t i = 1; i <= layers.size(); ++i) {
        std::string const what = "layers[" + std::to_string(i) + "]";
        sol::optional<sol::table> const layer = layers.get<sol::optional<sol::table>>(static_cast<int>(i));
        if (!layer)
            throw std::runtime_error(what + ": expected a table");
        transition::Layer item;
        item.field = lua_field(*layer, what, textures);
        if (auto const mask = layer->get<sol::optional<sol::table>>("mask"))
            item.mask = lua_field(*mask, what + ".mask", textures);
        item.fill = lua_fill(layer->get<sol::optional<sol::table>>("fill"), what, textures);
        sol::object const opacity = layer->get<sol::object>("opacity");
        if (opacity.get_type() != sol::type::nil)
            item.opacity = lua_curve(opacity, what + ".opacity");
        out.push_back(std::move(item));
    }
    if (out.empty())
        throw std::runtime_error("layers is empty");
    if (out.size() > transition::max_layers)
        throw std::runtime_error("at most " + std::to_string(transition::max_layers) + " layers per transition");
    return out;
}

// resources/lua/transitions/<name>.lua: returns a table, see the README. Returns true on error
static bool mylua_load_transition(std::filesystem::path const& file)
{
    std::string const name = file.stem().string();
    try {
        auto result = g->lua().safe_script_file(file.string(), sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error const err = result;
            LOG_CTX_ERR(name, err.what());
            return true;
        }
        if (result.get_type() != sol::type::table)
            throw std::runtime_error("the script must return a table");
        sol::table const t = result.get<sol::table>();
        std::string const kind = t.get<sol::optional<std::string>>("kind").value_or("single");
        if (kind != "single" && kind != "dual")
            throw std::runtime_error("kind must be 'single' or 'dual'");

        transition::Script script;
        script.name = name;
        script.label = t.get<sol::optional<std::string>>("label").value_or(name);
        script.kind = kind == "dual" ? transition::Kind::Dual : transition::Kind::Single;
        script.duration = t.get<sol::optional<float>>("duration").value_or(1.f);
        script.midpoint = std::clamp(t.get<sol::optional<float>>("midpoint").value_or(0.5f), 0.01f, 0.99f);
        script.defaults = lua_transition_options(t);
        script.path = file;
        // Motion of the scene behind the layers, optional
        auto const scene_curve = [&](char const* key) {
            sol::object const value = t.get<sol::object>(key);
            return value.get_type() == sol::type::nil ? transition::Curve{} : lua_curve(value, name + "." + key);
        };
        script.scene_x = scene_curve("scene_x");
        script.scene_y = scene_curve("scene_y");
        script.wave = scene_curve("wave");

        auto const shader = t.get<sol::optional<std::string>>("shader");
        auto const layers = t.get<sol::optional<sol::table>>("layers");
        if (shader && layers)
            throw std::runtime_error("use shader or layers, not both");
        if (layers)
            script.layers = lua_layers(*layers, script.textures);
        else if (shader)
            script.shader = *shader;
        else
            throw std::runtime_error("missing shader or layers");

        if (transition::define(std::move(script)))
            return true;
        LOG_CTX_MSG("Transition loaded", name);
        return false;
    }
    catch (std::exception const& e) {
        LOG_CTX_ERR(name, e.what());
        return true;
    }
}

// Loads the transitions when their file is new or saved, checked twice per second
void mylua_watch_transitions()
{
    using clock = std::chrono::steady_clock;
    static auto next_check = clock::now();
    static std::map<std::string, std::filesystem::file_time_type> times;
    auto const now = clock::now();
    if (now < next_check)
        return;
    next_check = now + std::chrono::milliseconds(500);

    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(g->lua_folder + "transitions/", ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".lua")
            continue;
        auto const time = std::filesystem::last_write_time(entry.path(), ec);
        auto const [it, is_new] = times.try_emplace(entry.path().stem().string());
        if (!is_new && it->second == time)
            continue;
        it->second = time;
        mylua_load_transition(entry.path());
    }
}

bool setup_lua()
{
    mylua_register_scripts();
    signals::installDefaults();
    g->lua().open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua();
    lua_setup_other_libs(lua);

    // Frame clock, updated before the scenes run, see update_lua_clock()
    lua["dt"] = 0.0;
    lua["time"] = 0.0;
    lua["file_script"] = mylua_file_script;
    // C++ functions return true on error, Lua ones return true on success
    lua["load_scene"] = [](std::string const& scene_name, sol::optional<sol::table> opts) {
        return !lua_switch_scene(opts, [scene_name]() { return mylua_load_scene(scene_name); });
    };
    lua["unload_scene"] = [](sol::optional<std::string> scene_name) {
        return !mylua_unload_scene(scene_name.value_or(""));
    };
    lua["reload_scene"] = [](sol::optional<std::string> scene_name) {
        return !mylua_reload_scene(scene_name.value_or(""));
    };
    lua["auto_reload"] = [](sol::optional<bool> on) {
        watch_scenes = on.value_or(!watch_scenes);
        return watch_scenes;
    };
    lua["list_scenes"] = mylua_list_scenes;
    lua["list_assets"] = [](std::string const& folder) {
        return sol::as_table(mylua_list_assets(folder));
    };
    lua["menu"] = [](sol::optional<sol::table> opts) {
        return !lua_switch_scene(opts, []() { return mylua_return_to_menu(); });
    };
    // Screen transitions, see transition.hpp. Returns true on success
    lua["transition"] = [](std::string const& name, sol::optional<float> duration, sol::optional<sol::table> opts) {
        transition::Options options = opts ? lua_transition_options(*opts) : transition::Options{};
        std::optional<float> const seconds = duration ? std::optional<float>(*duration) : std::nullopt;
        return !transition::start(name, seconds, std::move(options));
    };
    lua["transition_stop"] = []() {
        transition::stop();
    };
    // Easing of the transitions (see the README): smoothstep of x, clamped to [0, 1]
    auto const smooth = [](float x) {
        float const v = std::clamp(x, 0.f, 1.f);
        return v * v * (3.f - 2.f * v);
    };
    lua["transition_smooth"] = smooth;
    // Radius that reaches the farthest corner of the screen (screen height 1, see the README)
    lua["corner_radius"] = [](float aspect) {
        return 0.5f * std::sqrt(aspect * aspect + 1.f) * 1.02f;
    };
    // Cover then reveal of a single transition (see the README): 0 at the start, 1 at the midpoint, 0 at the end
    lua["transition_cover"] = [smooth](float x) {
        float const t = std::clamp(x, 0.f, 1.f);
        return t < 0.5f ? smooth(2.f * t) : 1.f - smooth(2.f * t - 1.f);
    };
    lua["list_transitions"] = []() {
        std::string names;
        for (auto const& def : transition::list()) {
            names += (names.empty() ? "" : ", ") + def.name
                + (def.kind == transition::Kind::Dual ? " (dual)" : "");
        }
        g->lua()["print"]("Transitions: " + names);
    };
    // Sounds overlap freely, only one music plays at a time (crossfaded).
    // Like scene requests, they are run by the main loop (see defer_request()).
    // Names are relative to the sounds / musics folders.
    lua["play_sound"] = [](std::string const& file, sol::optional<int> volume) {
        std::string const path = resolve_audio(g->sounds_folder, file);
        int const vol = volume.value_or(100);
        if (!defer_request([path, vol]() { SSS::Audio::Mixer::playSound(path, vol); }))
            SSS::Audio::Mixer::playSound(path, vol);
    };
    lua["play_music"] = [](std::string const& file, sol::optional<bool> loop, sol::optional<float> fade) {
        std::string const path = resolve_audio(g->musics_folder, file);
        bool const lp = loop.value_or(true);
        float const fd = fade.value_or(SSS::Audio::Mixer::default_fade);
        if (!defer_request([path, lp, fd]() { SSS::Audio::Mixer::playMusic(path, lp, fd); }))
            SSS::Audio::Mixer::playMusic(path, lp, fd);
    };
    lua["stop_music"] = [](sol::optional<float> fade) {
        float const fd = fade.value_or(SSS::Audio::Mixer::default_fade);
        if (!defer_request([fd]() { SSS::Audio::Mixer::stopMusic(fd); }))
            SSS::Audio::Mixer::stopMusic(fd);
    };
    lua["dialog"] = mylua_dialog;
    lua["help"] = mylua_help;
    lua["dof"] = [](sol::optional<bool> on) {
        mylua_focus([&](SceneRenderer& ren) {
            ren.depth_of_field = on.value_or(!ren.depth_of_field);
        });
    };
    lua["focus"] = [](sol::optional<float> distance, sol::optional<float> band_width) {
        mylua_focus([&](SceneRenderer& ren) {
            if (distance)
                ren.setFocus(*distance, band_width.value_or(ren.getFocusBandWidth()));
        });
    };
    lua["focus_near"] = [](float distance) {
        mylua_focus([&](SceneRenderer& ren) { ren.setFocusNear(distance); });
    };
    lua["focus_far"] = [](float distance) {
        mylua_focus([&](SceneRenderer& ren) { ren.setFocusFar(distance); });
    };
    lua["focus_move"] = [](float delta) {
        mylua_focus([&](SceneRenderer& ren) { ren.moveFocus(delta); });
    };
    lua["debug_mouths"] = [](sol::optional<bool> on) {
        Dialog::setDebugMouths(on.value_or(!Dialog::debugMouths()));
        return Dialog::debugMouths();
    };
    lua["dialog_tails"] = [](sol::optional<bool> on) {
        settings::setDialogTails(on.value_or(!settings::get().dialog_tails));
        return settings::get().dialog_tails;
    };
    lua["grey_characters"] = [](sol::optional<bool> on) {
        settings::setGreyCharacters(on.value_or(!settings::get().grey_characters));
        return settings::get().grey_characters;
    };
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
        mylua_console_env = reinterpret_cast<sol::environment*>(&g->lua().globals());
    };
    lua["console_reset_env"]();

    // Not bound by SSS::GL, needed by Camera:rotate (pitch, yaw in degrees)
    auto vec2 = lua.new_usertype<glm::vec2>("vec2", sol::constructors<
        glm::vec2(),
        glm::vec2(float),
        glm::vec2(float, float)
    >());
    vec2["x"] = &glm::vec2::x;
    vec2["y"] = &glm::vec2::y;

    // Whole 3D scene of planes, see SceneRenderer.hpp
    auto scene_renderer = lua.new_usertype<SceneRenderer>("SceneRenderer", sol::factories(
        []() { return SceneRenderer::create(); },
        [](GL::Camera* cam) { return SceneRenderer::create(GL::Camera::get(cam)); }
    ), sol::base_classes, sol::bases<GL::PlaneRenderer, GL::RendererBase, Base>());
    scene_renderer["addPlane"] = [](SceneRenderer& ren, GL::Plane* plane) {
        ren.addPlane(GL::Plane::get(plane));
    };
    scene_renderer["removePlane"] = [](SceneRenderer& ren, GL::Plane* plane) {
        ren.removePlane(GL::Plane::get(plane));
    };
    scene_renderer["addScrollLayer"] = [](SceneRenderer& ren, std::vector<GL::Plane*> planes) {
        std::vector<GL::Plane::Shared> vec;
        vec.reserve(planes.size());
        for (GL::Plane* plane : planes)
            vec.push_back(GL::Plane::get(plane));
        ren.addScrollLayer(vec);
    };
    scene_renderer["clearScrollLayers"] = &SceneRenderer::clearScrollLayers;
    scene_renderer["scroll_layer_count"] = sol::property(&SceneRenderer::getScrollLayerCount);
    scene_renderer["scroll_speed"] = &SceneRenderer::scroll_speed;
    scene_renderer["depth_sort"] = &SceneRenderer::depth_sort;
    scene_renderer["is_playing"] = sol::property(&SceneRenderer::isPlaying);
    scene_renderer["pause"] = &SceneRenderer::pause;
    scene_renderer["play"] = &SceneRenderer::play;
    scene_renderer["toggle"] = &SceneRenderer::toggle;
    scene_renderer["lighting"] = &SceneRenderer::lighting;
    scene_renderer["opaque_cutoff"] = &SceneRenderer::opaque_cutoff;
    scene_renderer["ambient"] = &SceneRenderer::ambient;
    scene_renderer["lights"] = &SceneRenderer::lights;
    scene_renderer["addLight"] = &SceneRenderer::addLight;
    scene_renderer["removeLight"] = &SceneRenderer::removeLight;
    // Depth of field: focus band from near to far, distances from the camera
    scene_renderer["depth_of_field"] = &SceneRenderer::depth_of_field;
    scene_renderer["focus_blur"] = &SceneRenderer::focus_blur;
    scene_renderer["focus_max_blur"] = &SceneRenderer::focus_max_blur;
    scene_renderer["setFocus"] = &SceneRenderer::setFocus;
    scene_renderer["moveFocus"] = &SceneRenderer::moveFocus;
    scene_renderer["focus_transition"] = &SceneRenderer::focus_transition;
    scene_renderer["dialog_blur"] = &SceneRenderer::dialog_blur;
    scene_renderer["setBlurClamp"] = [](SceneRenderer& ren, GL::Plane* plane, bool clamp) {
        ren.setBlurClamp(GL::Plane::get(plane), clamp);
    };
    scene_renderer["getBlurClamp"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getBlurClamp(GL::Plane::get(plane));
    };
    scene_renderer["setFocusNear"] = &SceneRenderer::setFocusNear;
    scene_renderer["setFocusFar"] = &SceneRenderer::setFocusFar;
    scene_renderer["focus_near"] = sol::property(&SceneRenderer::getFocusNear);
    scene_renderer["focus_far"] = sol::property(&SceneRenderer::getFocusFar);
    scene_renderer["focus_distance"] = sol::property(&SceneRenderer::getFocusDistance);
    scene_renderer["focus_band_width"] = sol::property(&SceneRenderer::getFocusBandWidth);
    scene_renderer["normal_map_y_down"] = &SceneRenderer::normal_map_y_down;
    // setNormalMap(plane, texture | path in the assets folder | nil)
    scene_renderer["setNormalMap"] = sol::overload(
        [](SceneRenderer& ren, GL::Plane* plane, GL::Texture* texture) {
            ren.setNormalMap(GL::Plane::get(plane), texture ? GL::Texture::get(texture) : nullptr);
        },
        [](SceneRenderer& ren, GL::Plane* plane, std::string const& path) {
            ren.setNormalMap(GL::Plane::get(plane), GL::Texture::create(std::filesystem::path{ path }));
        },
        [](SceneRenderer& ren, GL::Plane* plane, sol::lua_nil_t) {
            ren.setNormalMap(GL::Plane::get(plane), nullptr);
        }
    );
    scene_renderer["getNormalMap"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getNormalMap(GL::Plane::get(plane));
    };
    scene_renderer["setLightingFactor"] = [](SceneRenderer& ren, GL::Plane* plane, float factor) {
        ren.setLightingFactor(GL::Plane::get(plane), factor);
    };
    scene_renderer["getLightingFactor"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getLightingFactor(GL::Plane::get(plane));
    };
    scene_renderer["shadows"] = &SceneRenderer::shadows;
    scene_renderer["shadow_map_size"] = &SceneRenderer::shadow_map_size;
    scene_renderer["shadow_fit_margin"] = &SceneRenderer::shadow_fit_margin;
    scene_renderer["setCastShadow"] = [](SceneRenderer& ren, GL::Plane* plane, bool cast) {
        ren.setCastShadow(GL::Plane::get(plane), cast);
    };
    scene_renderer["getCastShadow"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getCastShadow(GL::Plane::get(plane));
    };
    scene_renderer["setShadowTransmission"] = [](SceneRenderer& ren, GL::Plane* plane, float transmission) {
        ren.setShadowTransmission(GL::Plane::get(plane), transmission);
    };
    scene_renderer["getShadowTransmission"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getShadowTransmission(GL::Plane::get(plane));
    };
    // setEmissiveMap(plane, texture | path in the assets folder | nil)
    scene_renderer["setEmissiveMap"] = sol::overload(
        [](SceneRenderer& ren, GL::Plane* plane, GL::Texture* texture) {
            ren.setEmissiveMap(GL::Plane::get(plane), texture ? GL::Texture::get(texture) : nullptr);
        },
        [](SceneRenderer& ren, GL::Plane* plane, std::string const& path) {
            ren.setEmissiveMap(GL::Plane::get(plane), GL::Texture::create(std::filesystem::path{ path }));
        },
        [](SceneRenderer& ren, GL::Plane* plane, sol::lua_nil_t) {
            ren.setEmissiveMap(GL::Plane::get(plane), nullptr);
        }
    );
    scene_renderer["getEmissiveMap"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getEmissiveMap(GL::Plane::get(plane));
    };
    scene_renderer["setEmissiveColor"] = [](SceneRenderer& ren, GL::Plane* plane, glm::vec3 const& color) {
        ren.setEmissiveColor(GL::Plane::get(plane), color);
    };
    scene_renderer["getEmissiveColor"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getEmissiveColor(GL::Plane::get(plane));
    };
    scene_renderer["setEmissiveIntensity"] = [](SceneRenderer& ren, GL::Plane* plane, float intensity) {
        ren.setEmissiveIntensity(GL::Plane::get(plane), intensity);
    };
    scene_renderer["getEmissiveIntensity"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getEmissiveIntensity(GL::Plane::get(plane));
    };
    // Gradient maps: addGradient({ { t, 0xRRGGBB | vec3, [alpha], [ease] }, ... })
    // returns the gradient's index (-1: atlas full), given to setGradient
    scene_renderer["addGradient"] = [](SceneRenderer& ren, sol::table const& stops) {
        return ren.addGradient(lua_to_gradient(stops));
    };
    scene_renderer["setGradientStops"] = [](SceneRenderer& ren, int index, sol::table const& stops) {
        ren.setGradientStops(index, lua_to_gradient(stops));
    };
    scene_renderer["clearGradients"] = &SceneRenderer::clearGradients;
    scene_renderer["gradient_count"] = sol::property(&SceneRenderer::getGradientCount);
    // setGradient(plane, index | nil)
    scene_renderer["setGradient"] = sol::overload(
        [](SceneRenderer& ren, GL::Plane* plane, int index) {
            ren.setGradient(GL::Plane::get(plane), index);
        },
        [](SceneRenderer& ren, GL::Plane* plane, sol::lua_nil_t) {
            ren.setGradient(GL::Plane::get(plane), -1);
        }
    );
    scene_renderer["getGradient"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getGradient(GL::Plane::get(plane));
    };
    // setCausticsMap(plane, texture | path in the assets folder | nil)
    scene_renderer["setCausticsMap"] = sol::overload(
        [](SceneRenderer& ren, GL::Plane* plane, GL::Texture* texture) {
            ren.setCausticsMap(GL::Plane::get(plane), texture ? GL::Texture::get(texture) : nullptr);
        },
        [](SceneRenderer& ren, GL::Plane* plane, std::string const& path) {
            ren.setCausticsMap(GL::Plane::get(plane), GL::Texture::create(std::filesystem::path{ path }));
        },
        [](SceneRenderer& ren, GL::Plane* plane, sol::lua_nil_t) {
            ren.setCausticsMap(GL::Plane::get(plane), nullptr);
        }
    );
    scene_renderer["getCausticsMap"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getCausticsMap(GL::Plane::get(plane));
    };
    scene_renderer["setCausticsColor"] = [](SceneRenderer& ren, GL::Plane* plane, glm::vec3 const& color) {
        ren.setCausticsColor(GL::Plane::get(plane), color);
    };
    scene_renderer["getCausticsColor"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getCausticsColor(GL::Plane::get(plane));
    };
    scene_renderer["setCausticsIntensity"] = [](SceneRenderer& ren, GL::Plane* plane, float intensity) {
        ren.setCausticsIntensity(GL::Plane::get(plane), intensity);
    };
    scene_renderer["getCausticsIntensity"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getCausticsIntensity(GL::Plane::get(plane));
    };
    scene_renderer["setCausticsTile"] = [](SceneRenderer& ren, GL::Plane* plane, float tile) {
        ren.setCausticsTile(GL::Plane::get(plane), tile);
    };
    scene_renderer["getCausticsTile"] = [](SceneRenderer& ren, GL::Plane* plane) {
        return ren.getCausticsTile(GL::Plane::get(plane));
    };
    scene_renderer["setCausticsWaterline"] = [](SceneRenderer& ren, GL::Plane* plane, float water_y, float fade) {
        ren.setCausticsWaterline(GL::Plane::get(plane), water_y, fade);
    };
    // Returns water_y, fade
    scene_renderer["getCausticsWaterline"] = [](SceneRenderer& ren, GL::Plane* plane) {
        glm::vec2 const waterline = ren.getCausticsWaterline(GL::Plane::get(plane));
        return std::make_tuple(waterline.x, waterline.y);
    };
    scene_renderer["caustics_speed"] = &SceneRenderer::caustics_speed;

    // See PointLight in SceneRenderer.hpp
    auto point_light = lua.new_usertype<PointLight>("PointLight",
        sol::factories([]() { return PointLight::create(); }));
    point_light["position"] = &PointLight::position;
    point_light["color"] = &PointLight::color;
    point_light["intensity"] = &PointLight::intensity;
    point_light["radius"] = &PointLight::radius;
    point_light["falloff"] = &PointLight::falloff;
    point_light["terminator_color"] = &PointLight::terminator_color;
    point_light["terminator_width"] = &PointLight::terminator_width;
    point_light["shadow_color"] = &PointLight::shadow_color;
    point_light["enabled"] = &PointLight::enabled;
    // follow = plane | nil, see PointLight::follow
    point_light["follow"] = sol::property(
        [](PointLight& light) { return std::dynamic_pointer_cast<GL::Plane>(light.follow.lock()); },
        [](PointLight& light, sol::object plane) {
            light.follow = plane.is<GL::Plane*>() ? GL::Plane::get(plane.as<GL::Plane*>()) : nullptr;
        }
    );
    point_light["follow_offset"] = &PointLight::follow_offset;
    point_light["cast_shadows"] = &PointLight::cast_shadows;
    point_light["shadow_direction"] = &PointLight::shadow_direction;
    point_light["shadow_fov"] = &PointLight::shadow_fov;
    point_light["shadow_bias"] = &PointLight::shadow_bias;
    point_light["shadow_softness"] = &PointLight::shadow_softness;
    point_light["shadow_ambient"] = &PointLight::shadow_ambient;
    point_light["size"] = &PointLight::size;

    // Folding paper map mesh, see FoldingMap.hpp
    // FoldingMap.new(front, back, width, height): textures (back can be nil:
    // the front one mirrored) or image paths in the assets folder
    auto folding_map = lua.new_usertype<FoldingMap>("FoldingMap", sol::factories(
        [](GL::Texture* front, GL::Texture* back, float width, float height) {
            return FoldingMap::create(front ? GL::Texture::get(front) : nullptr,
                back ? GL::Texture::get(back) : nullptr, width, height);
        },
        [](std::string const& front, std::string const& back, float width, float height) {
            return FoldingMap::create(GL::Texture::create(std::filesystem::path{ front }),
                GL::Texture::create(std::filesystem::path{ back }), width, height);
        }
    ), sol::base_classes, sol::bases<GL::ModelBase>());
    folding_map["half_fold"] = sol::property(&FoldingMap::getHalfFold, &FoldingMap::setHalfFold);
    folding_map["flap"] = sol::property(&FoldingMap::getFlap, &FoldingMap::setFlap);
    folding_map["accordion"] = sol::property(&FoldingMap::getAccordion, &FoldingMap::setAccordion);
    folding_map["bend"] = sol::property(&FoldingMap::getBend, &FoldingMap::setBend);    folding_map["thickness"] = sol::property(&FoldingMap::getThickness, &FoldingMap::setThickness);
    folding_map["crease_radius"] = sol::property(&FoldingMap::getCreaseRadius, &FoldingMap::setCreaseRadius);
    folding_map["width"] = sol::readonly_property(&FoldingMap::getWidth);
    folding_map["height"] = sol::readonly_property(&FoldingMap::getHeight);
    folding_map["front"] = &FoldingMap::front;
    folding_map["back"] = &FoldingMap::back;
    folding_map["shade"] = &FoldingMap::shade;
    folding_map["mask"] = &FoldingMap::mask;
    folding_map["hidden"] = &FoldingMap::hidden;
    folding_map["cast_shadow"] = &FoldingMap::cast_shadow;
    // Returns min, max (vec3): local bounding box of the current shape
    folding_map["getBounds"] = [](FoldingMap& map) {
        map.update();
        return std::make_tuple(map.getBoundsMin(), map.getBoundsMax());
    };
    scene_renderer["addMesh"] = [](SceneRenderer& ren, std::shared_ptr<FoldingMap> mesh) {
        ren.addMesh(std::move(mesh));
    };
    scene_renderer["removeMesh"] = [](SceneRenderer& ren, std::shared_ptr<FoldingMap> const& mesh) {
        ren.removeMesh(mesh);
    };

    // City map over the scene (M key), see MapOverlay.hpp
    sol::table map_overlay = lua.create_named_table("map_overlay");
    map_overlay["open"] = &MapOverlay::open;
    map_overlay["close"] = &MapOverlay::close;
    map_overlay["toggle"] = &MapOverlay::toggle;
    map_overlay["hide"] = &MapOverlay::hide;
    map_overlay["is_open"] = &MapOverlay::isOpen;
    map_overlay["is_shown"] = &MapOverlay::isShown;
    // What's printed on it (see MapContent.hpp): the state is kept in the
    // game's values, so saved with it
    map_overlay["unlock_location"] = [](std::string const& id) { map_content::setLocationUnlocked(id, true); };
    map_overlay["lock_location"] = [](std::string const& id) { map_content::setLocationUnlocked(id, false); };
    map_overlay["is_location_unlocked"] = &map_content::isLocationUnlocked;
    map_overlay["unlock_road"] = [](std::string const& id) { map_content::setRoadUnlocked(id, true); };
    map_overlay["lock_road"] = [](std::string const& id) { map_content::setRoadUnlocked(id, false); };
    map_overlay["is_road_unlocked"] = &map_content::isRoadUnlocked;
    map_overlay["show_character"] = [](std::string const& id) { map_content::setCharacterShown(id, true); };
    map_overlay["hide_character"] = [](std::string const& id) { map_content::setCharacterShown(id, false); };
    map_overlay["is_character_shown"] = &map_content::isCharacterShown;
    map_overlay["move_character"] = &map_content::moveCharacter;
    map_overlay["character_location"] = &map_content::characterLocation;
    // { id = { name = ..., scene = ... }, ... }
    map_overlay["locations"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table out = lua.create_table();
        for (auto const& loc : map_content::data().locations)
            out[loc.id] = lua.create_table_with("name", loc.name, "scene", loc.scene);
        return out;
    };
    map_overlay["clear"] = &map_content::clear;
    // Ordered as in the JSON: { { id = ..., ... }, ... }
    map_overlay["all_locations"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table out = lua.create_table();
        for (auto const& loc : map_content::data().locations)
            out.add(lua.create_table_with("id", loc.id, "name", loc.name, "scene", loc.scene, "transition", loc.transition));
        return out;
    };
    map_overlay["all_roads"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table out = lua.create_table();
        for (auto const& road : map_content::data().roads)
            out.add(lua.create_table_with("id", road.id, "from", road.from, "to", road.to));
        return out;
    };
    map_overlay["all_characters"] = [](sol::this_state ts) {
        sol::state_view lua(ts);
        sol::table out = lua.create_table();
        for (auto const& ch : map_content::data().characters)
            out.add(lua.create_table_with("id", ch.id, "location", ch.location));
        return out;
    };

    // Ren'Py dialog node, see Dialog.hpp & dialog.lua
    // Dialog.new() is idle, Dialog.new(name) starts the conversation at once
    // (see also dialog(name), playing on a shared node).
    // Started dialogs are updated by the main loop (Dialog::updateAll).
    auto dialog = lua.new_usertype<Dialog>("Dialog", sol::factories(
        []() { return new_dialog(); },
        [](std::string const& name) {
            auto d = new_dialog();
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
    // function (name, args...) or nil: this dialog's own handler of the
    // script's signals, called before the signal manager's (see signals).
    // Returns true when it handled the signal.
    dialog["on_signal"] = sol::writeonly_property([](Dialog& d, sol::object callback) {
        if (!callback.is<sol::protected_function>()) {
            d.setOnSignal(signals::dispatch);
            return;
        }
        d.setOnSignal([f = callback.as<sol::protected_function>()](renpy::Signal const& signal) {
            if (call_lua_signal_handler(f, signal, "Dialog.on_signal"))
                return true;
            return signals::dispatch(signal);
        });
    });

    // Signal manager: handlers of the script's signals, see SignalManager.hpp.
    // function (name, args...) returning true when it handled the signal.
    // Handlers of a scene must be removed with off() when it unloads.
    auto signals_table = lua["signals"].get_or_create<sol::table>();
    signals_table["on"] = [](std::string const& name, sol::protected_function f) {
        return signals::on(name, [f](renpy::Signal const& signal) {
            return call_lua_signal_handler(f, signal, "signals");
        });
    };
    signals_table["off"] = &signals::off;

    // Game values, see GameState.hpp
    auto game_vars = lua.new_usertype<GameVars>("GameVars", sol::no_constructor);
    game_vars[sol::meta_function::index] = [](GameVars const&, std::string const& name, sol::this_state s) -> sol::object {
        if (auto const value = game_state::get(name)) return to_lua(s, *value);
        return sol::lua_nil;
    };
    game_vars[sol::meta_function::new_index] = [](GameVars&, std::string const& name, sol::object value) {
        if (value.is<sol::lua_nil_t>()) game_state::erase(name);
        else game_state::set(name, from_lua(value));
    };
    // pairs(game.vars), on a copy
    game_vars[sol::meta_function::pairs] = [](GameVars const&, sol::this_state s) {
        sol::state_view lua(s);
        sol::table copy = lua.create_table();
        for (auto const& [name, value] : game_state::vars())
            copy[name] = to_lua(lua, value);
        return std::make_tuple(lua["next"].get<sol::object>(), copy, sol::lua_nil);
    };
    sol::table game = lua.create_named_table("game");
    game["vars"] = GameVars{};
    game["save"] = [](sol::optional<std::string> name) { return game_state::save(name.value_or("save")); };
    game["load"] = [](sol::optional<std::string> name) { return game_state::load(name.value_or("save")); };
    game["reset"] = game_state::reset;

    if (mylua_file_script("global_setup.lua"))
        return true;

    g->window = g->lua()["window"];
    g->ui_window = g->lua()["ui_window"];

    name_env_objects(lua.globals());

    return false;
}
