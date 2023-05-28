#pragma once

#include "includes.hpp"
#include "Parallax.hpp"

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
private:
	std::unique_ptr<sol::environment> env;
	std::string const path;
	std::string const filename;
	std::string script;
};

void mylua_register_scripts();
bool mylua_file_script(std::string const& path);
bool mylua_run_active_scenes();
bool mylua_load_scene(std::string const& scene_name);
bool mylua_unload_scene(std::string const& scene_name);
std::map<std::string, sol::type> mylua_get_keys(sol::environment const& env);

extern sol::environment* mylua_console_env;

bool setup_lua();