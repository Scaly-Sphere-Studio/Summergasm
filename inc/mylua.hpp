#pragma once

#include "includes.hpp"

class Scene {
public:
	Scene() = delete;
	Scene(std::string const& filename);
	~Scene();

	Scene(const Scene&) = delete;	// Copy constructor
	Scene(Scene&&) = delete;	// Move constructor
	Scene& operator=(const Scene&) = delete;	// Copy assignment
	Scene& operator=(Scene&&) = delete;	// Move assignment

	bool run();

private:
	sol::environment env{ g->lua, sol::create, g->lua.globals() };
	std::string path;
};

void mylua_register_scripts();
bool mylua_file_script(std::string const& path);
bool mylua_run_active_scenes();
bool mylua_load_scene(std::string const& scene_name);
bool mylua_unload_scene(std::string const& scene_name);

bool setup_lua();