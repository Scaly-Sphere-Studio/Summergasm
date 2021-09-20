#include "Json_Operators.hpp"

SSS::GL::Window::Args& operator<<(SSS::GL::Window::Args& args,
    nlohmann::json const& data)
{
    args.title = data["title"];
    args.w = data["width"];
    args.h = data["height"];
    args.monitor_id = data["monitor_id"];
    args.fullscreen = data["fullscreen"];
    return args;
}

glm::vec3& operator<<(glm::vec3& vec, nlohmann::json const& data)
{
    vec.x = data["x"];
    vec.y = data["y"];
    vec.z = data["z"];
    return vec;
}

SSS::GL::Window::Shared const& operator<<(SSS::GL::Window::Shared const& window,
    nlohmann::json const& data)
{
    SSS::GL::Window::Objects const& objects = window->getObjects();
    glm::vec3 vec;
    // Cameras
    for (nlohmann::json const& cam_data : data["cameras"]) {
        window->createCamera(cam_data["id"]);
        SSS::GL::Camera::Ptr const& camera = objects.cameras.at(cam_data["id"]);
        camera->setProjectionType(static_cast<SSS::GL::Camera::Projection>
            (cam_data["projection"]));
        camera->setFOV(glm::radians(static_cast<float>(cam_data["fov"])));
        camera->setRange(cam_data["range"]["near"], cam_data["range"]["far"]);
        camera->setPosition(vec << cam_data["position"]);
        camera->rotate(cam_data["rotation"]["angle"], vec << cam_data["rotation"]);
    }
    // Textures
    for (nlohmann::json const& tex_data : data["textures"]) {
        window->createTexture(tex_data["id"]);
        SSS::GL::Texture::Ptr const& texture = objects.textures.at(tex_data["id"]);
        texture->setType(static_cast<SSS::GL::Texture::Type>(tex_data["type"]));
        if (tex_data.count("filepath") != 0) {
            texture->useFile(tex_data["filepath"]);
        }
    }
    // Planes
    for (nlohmann::json const& plane_data : data["planes"]) {
        window->createModel(plane_data["id"], SSS::GL::ModelType::Plane);
        SSS::GL::Plane::Ptr const& plane = objects.planes.at(plane_data["id"]);
        plane->useTexture(plane_data["texture_id"]);
        plane->setHitbox(static_cast<SSS::GL::Plane::Hitbox>(plane_data["hitbox"]));
        plane->resetTransformations(SSS::GL::Transformation::All);
        plane->scale(vec << plane_data["scaling"]);
        plane->rotate(plane_data["rotation"]["angle"], vec << plane_data["rotation"]);
        plane->translate(vec << plane_data["translation"]);
    }
    return window;
}
