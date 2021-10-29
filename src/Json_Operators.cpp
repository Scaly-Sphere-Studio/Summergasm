#include "Summergasm.hpp"

static nlohmann::json relativePathToJson(std::string const& path)
{
    if (SSS::isReg(path)) {
        return nlohmann::json::parse(SSS::readFile(path));
    }
    return nlohmann::json::parse(SSS::readFile(SSS::PWD + path));
}

static glm::vec3& operator<<(glm::vec3& vec, nlohmann::json const& data)
{
    vec.x = data["x"];
    vec.y = data["y"];
    vec.z = data["z"];
    return vec;
}

static glm::vec2& operator<<(glm::vec2& vec, nlohmann::json const& data)
{
    vec.x = data["x"];
    vec.y = data["y"];
    return vec;
}

SSS::GL::Window::Shared createWindow(std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    SSS::GL::Window::Args args;
    args.title      = data["title"];
    args.w          = data["width"];
    args.h          = data["height"];
    args.monitor_id = data["monitor_id"];
    args.fullscreen = data["fullscreen"];
    return SSS::GL::Window::create(args);
}

void loadWindowObjects(SSS::GL::Window::Shared const& window,
    std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    SSS::GL::Window::Objects const& objects = window->getObjects();
    glm::vec3 vec3;
    glm::vec2 vec2;
    // Cameras
    for (nlohmann::json const& cam_data : data["cameras"]) {
        window->createCamera(cam_data["id"]);
        SSS::GL::Camera::Ptr const& camera = objects.cameras.at(cam_data["id"]);
        camera->setProjectionType(static_cast<SSS::GL::Camera::Projection>
            (cam_data["projection"]));
        camera->setFOV(cam_data["fov"]);
        camera->setRange(cam_data["range"]["near"], cam_data["range"]["far"]);
        camera->setPosition(vec3 << cam_data["position"]);
        camera->setRotation(vec2 << cam_data["rotation"]);
    }
    // Textures
    for (nlohmann::json const& tex_data : data["textures"]) {
        window->createTexture(tex_data["id"]);
        SSS::GL::Texture::Ptr const& texture = objects.textures.at(tex_data["id"]);
        if (tex_data.count("filepath") != 0) {
            texture->useFile(SSS::PWD + std::string(tex_data["filepath"]));
        }
        if (tex_data.count("text_area_id") != 0) {
            uint32_t const text_area_id = tex_data["text_area_id"];
            if (g_data->text_areas.count(text_area_id) != 0) {
                texture->setTextArea(g_data->text_areas.at(text_area_id));
            }
        }
        texture->setType(static_cast<SSS::GL::Texture::Type>(tex_data["type"]));
    }
    // Planes
    for (nlohmann::json const& plane_data : data["planes"]) {
        window->createModel(plane_data["id"], SSS::GL::ModelType::Plane);
        SSS::GL::Plane::Ptr const& plane = objects.planes.at(plane_data["id"]);
        plane->useTexture(plane_data["texture_id"]);
        plane->setHitbox(static_cast<SSS::GL::Plane::Hitbox>(plane_data["hitbox"]));
        plane->resetTransformations(SSS::GL::Transformation::All);
        plane->scale(vec3 << plane_data["scaling"]);
        plane->rotate(plane_data["rotation"]["angle"], vec3 << plane_data["rotation"]);
        plane->translate(vec3 << plane_data["translation"]);
        uint32_t const func_id = plane_data["button_function_id"];
        if (func_id < g_data->button_functions.size()) {
            plane->setFunction(g_data->button_functions.at(func_id));
        }
    }
}

void organizeRenderers(SSS::GL::Window::Shared const& window,
    std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    SSS::GL::Window::Objects const& objects = window->getObjects();
    for (nlohmann::json const& renderer_data : data) {
        int const id    = renderer_data["id"];
        int const type  = renderer_data["type"];
        switch (type) {
        case 0:
            window->createRenderer<SSS::GL::PlaneRenderer>(id);
            break;
        default:
            SSS::throw_exc("Unkown type of renderer : " + std::to_string(type));
        }
        SSS::GL::Renderer::Ptr const& renderer = objects.renderers.at(id);
        for (nlohmann::json const& chunk_data : renderer_data["chunks"]) {
            int const id = chunk_data["id"];
            renderer->try_emplace(id);
            SSS::GL::RenderChunk& chunk = renderer->at(id);
            chunk.camera_ID             = chunk_data["camera_id"];
            chunk.use_camera            = chunk_data["use_camera"];
            chunk.reset_depth_before    = chunk_data["reset_depth_before"];
            nlohmann::json const& objects_data = chunk_data["objects"];
            for (int i = 0; i < objects_data.size(); ++i) {
                chunk.objects.try_emplace(i, objects_data[i]);
            }
        }
    }
}

static SSS::RGB24::s jsonToRGB24s(nlohmann::json const& color)
{
    if (color.is_array()) {
        return SSS::RGB24(color[0], color[1], color[2]);
    }
    else if (color.is_number_unsigned()) {
        return SSS::RGB24(color);
    }
    return SSS::RGB24(0xFFFFFF);
}

void loadTextAreas(std::string const& json_path) try
{
    using namespace SSS::TR;
    nlohmann::json const data = relativePathToJson(json_path);
    for (nlohmann::json const& text_area_data : data) {
        TextArea::Shared& text_area = g_data->text_areas[text_area_data["id"]];
        text_area = TextArea::create(text_area_data["width"], text_area_data["height"]);
        text_area->TWset(text_area_data["typewriter"]);
        for (nlohmann::json const& opt_data : text_area_data["text_opt"]) {
            Font::Shared font = Font::getShared(opt_data["font"]);
            nlohmann::json const& style_data = opt_data["style"];
            TextStyle style(
                style_data["charsize"],
                style_data["outline_size"],
                style_data["has_outline"],
                style_data["has_shadow"],
                style_data["line_spacing"]
            );
            nlohmann::json const& color_data = opt_data["color"];
            TextColors color(
                jsonToRGB24s(color_data["text"]),
                jsonToRGB24s(color_data["outline"]),
                jsonToRGB24s(color_data["shadow"]),
                color_data["alpha"]
            );
            nlohmann::json const& lng_data = opt_data["lng"];
            TextLanguage lng(
                lng_data["language"],
                lng_data["script"],
                lng_data["direction"],
                SSS::strToU32(opt_data["lng"]["word_dividers"])
            );
            TextOpt opt(font, style, color, lng);
            text_area->setTextOpt(opt_data["id"], opt);
        }
        g_data->texts.clear();
        g_data->texts.reserve(text_area_data["string_array"].size());
        for (std::string const& str : text_area_data["string_array"]) {
            g_data->texts.push_back(str);
        }
        text_area->parseString(g_data->texts[0]);
    }
}
__CATCH_AND_LOG_FUNC_EXC