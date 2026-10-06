#pragma once

#include "lua_include.hpp"
#include "SceneRenderer.hpp"

class Scene {
public:
	Scene() = delete;
	Scene(std::string const& filename);
	~Scene();

	Scene(const Scene&) = delete;				// Copy constructor
	Scene(Scene&&) = delete;					// Move constructor
	Scene& operator=(const Scene&) = delete;	// Copy assignment
	Scene& operator=(Scene&&) = delete;			// Move assignment

	bool run();
	auto& getEnv() const noexcept { return *env; };
	auto const& getPath() const noexcept { return path; };

	// Last write time of the file seen by mylua_watch_scenes()
	std::filesystem::file_time_type watched_time;
private:
	std::unique_ptr<sol::environment> env;
	std::string const path;
	std::string const filename;
	std::string script;
};

class LuaConsoleData : public std::map<std::string, LuaConsoleData> {
private:
    LuaConsoleData(sol::type type_, std::string name_) : type(type_), name(name_) {};
public:
    LuaConsoleData(sol::state& lua, sol::table table, sol::environment env,
        std::string name = "");

private:
    inline void _append(LuaConsoleData const& data) { insert(data.cbegin(), data.cend()); }
    void _appendUserdata(sol::state& lua, sol::environment env, sol::userdata u);
    
    template <class T>
    static void _appendBaseClass(sol::state& lua, LuaConsoleData& data,
        sol::environment env, sol::userdata const& u);
    using _AppendBaseClassFunc = std::function <
        void(sol::state&, LuaConsoleData&, sol::environment, sol::userdata const&) >;
    static std::vector<_AppendBaseClassFunc> _append_fns;

public:
    template <class T>
    static inline void addBaseClass() { _append_fns.emplace_back(_appendBaseClass<T>); }

    static bool recursive;

    sol::type type;
    std::string name;
    char separator{ '.' };

    std::string to_string() const;
    inline operator std::string() const { return to_string(); }
};

void mylua_register_scripts();
bool mylua_file_script(std::string const& path);
bool mylua_run_active_scenes();
bool mylua_load_scene(std::string const& scene_name);
bool mylua_unload_scene(std::string const& scene_name);
bool mylua_reload_scene(std::string const& scene_name);
// Reloads the current scene when its file changed (checked once per second)
void mylua_watch_scenes();
bool mylua_return_to_menu();
bool mylua_on_escape();
// Destroys the node of dialog(name), before the window is closed
void mylua_free_dialog();

extern sol::environment* mylua_console_env;

bool setup_lua();
