#pragma once

#include <SSS/GL.hpp>

void button_func_1(GLFWwindow* ptr, uint32_t id, int button, int action, int mods);
void button_func_2(GLFWwindow* ptr, uint32_t id, int button, int action, int mods);

struct GlobalData {
    std::map<uint32_t, SSS::TR::TextArea::Shared> text_areas;
    std::map<uint32_t, SSS::TR::Buffer::Shared> buffers;
    static constexpr std::array<SSS::GL::Plane::ButtonFunction, 3> button_functions = {
        nullptr,
        button_func_1,
        button_func_2
    };
};
extern std::unique_ptr<GlobalData> g_data;
