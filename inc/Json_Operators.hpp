#pragma once

#include "includes.hpp"

SSS::GL::Window::Shared createWindow(std::string const& json_path);

void loadWindowObjects(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void organizeRenderers(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void loadTextAreas(std::string const& json_path);