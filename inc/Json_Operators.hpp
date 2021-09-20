#pragma once

#include "includes.hpp"

SSS::GL::Window::Args& operator<<(
    SSS::GL::Window::Args& args, nlohmann::json const& data);

glm::vec3& operator<<(glm::vec3& vec, nlohmann::json const& data);

SSS::GL::Window::Shared const& operator<<(
    SSS::GL::Window::Shared const& window, nlohmann::json const& data);
