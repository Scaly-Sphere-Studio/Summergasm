#include "includes.hpp"
#include "Json_Operators.hpp"

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(ptr, GLFW_TRUE);
    }
}

int main(void) try
{
    // Create window & set callbacks
    SSS::GL::Window::Args args;
    args << nlohmann::json::parse(SSS::readFile(SSS::PWD + "resources/json/Window.json"));
    SSS::GL::Window::Shared window = SSS::GL::Window::create(args);
    SSS::GL::Window::Objects const& objects = window->getObjects();
    window->setCallback(glfwSetKeyCallback, key_callback);
    // Load objects
    window << nlohmann::json::parse(SSS::readFile(SSS::PWD + "resources/json/WindowObjects.json"));
    // Manually set renderers, for now
    window->createRenderer<SSS::GL::Plane::Renderer>(0);
    objects.renderers.at(0)->try_emplace(0);
    objects.renderers.at(0)->at(0).camera_ID = 0;
    objects.renderers.at(0)->at(0).objects.emplace(0, 0);
    objects.renderers.at(0)->at(0).objects.emplace(1, 1);
    objects.renderers.at(0)->try_emplace(1);
    objects.renderers.at(0)->at(1).camera_ID = 1;
    objects.renderers.at(0)->at(1).objects.emplace(0, 2);
    // Main loop
    while (window && !window->shouldClose()) {
        SSS::GL::Window::pollTextureThreads();
        window->render();
    }
}
__CATCH_AND_LOG_FUNC_EXC
