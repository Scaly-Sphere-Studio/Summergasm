#pragma once

#include "mylua.hpp"

// ImGui
void print_console();
void print_imgui();

// Callbacks

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods);
void close_callback(GLFWwindow* ptr);
