#include "Summergasm.hpp"

static nlohmann::json relativePathToJson(std::string const& path)
{
    if (SSS::pathIsFile(path)) {
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
    SSS::GL::Window::CreateArgs args;
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
            texture->setTextAreaID(tex_data["text_area_id"]);
        }
        texture->setType(static_cast<SSS::GL::Texture::Type>(tex_data["type"]));
    }
    // Planes
    for (nlohmann::json const& plane_data : data["planes"]) {
        window->createModel(plane_data["id"], SSS::GL::Model::Type::Plane);
        SSS::GL::Plane::Ptr const& plane = objects.planes.at(plane_data["id"]);
        plane->setTextureID(plane_data["texture_id"]);
        plane->setHitbox(static_cast<SSS::GL::Plane::Hitbox>(plane_data["hitbox"]));
        plane->setScaling(vec3 << plane_data["scaling"]);
        plane->setRotation(vec3 << plane_data["rotation"]);
        plane->setTranslation(vec3 << plane_data["translation"]);
        plane->setOnClickFuncID(plane_data["on_click_function_id"]);
        plane->setPassiveFuncID(plane_data["passive_function_id"]);
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
            window->createRenderer<SSS::GL::Plane::Renderer>(id);
            break;
        default:
            SSS::throw_exc("Unkown type of renderer : " + std::to_string(type));
        }
        SSS::GL::Renderer::Ptr const& renderer = objects.renderers.at(id);
        renderer->title = renderer_data["title"];
        for (nlohmann::json const& chunk_data : renderer_data["chunks"]) {
            SSS::GL::Renderer::Chunk& chunk = renderer->chunks.emplace_back();
            chunk.title                         = chunk_data["title"];
            chunk.camera_ID                     = chunk_data["camera_id"];
            chunk.use_camera                    = chunk_data["use_camera"];
            chunk.reset_depth_before            = chunk_data["reset_depth_before"];
            nlohmann::json const& objects_data  = chunk_data["objects"];
            for (uint32_t i = 0; i < objects_data.size(); ++i) {
                chunk.objects.emplace_back(objects_data[i]);
            }
        }
    }
}

static SSS::RGB24 jsonToRGB24(nlohmann::json const& color)
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
    nlohmann::json const data = relativePathToJson(json_path);
    SSS::TR::Area::Map const& areas = SSS::TR::Area::getMap();
    for (nlohmann::json const& area_data : data) {
        uint32_t id = area_data["id"];
        SSS::TR::Area::create(id, area_data["width"], area_data["height"]);
        SSS::TR::Area::Ptr const& area = areas.at(id);
        area->twSet(area_data["typewriter"]);
        for (nlohmann::json const& format_data : area_data["text_opt"]) {
            // Font
            SSS::TR::Format format;
            format.font = format_data["font"];
            SSS::TR::loadFont(format.font);
            // Style
            nlohmann::json const& style_data = format_data["style"];
            format.style.charsize      = style_data["charsize"];
            format.style.has_outline   = style_data["has_outline"];
            format.style.outline_size  = style_data["outline_size"];
            format.style.has_shadow    = style_data["has_shadow"];
            format.style.line_spacing  = style_data["line_spacing"];
            // Color
            nlohmann::json const& color_data = format_data["color"];
            format.color.text.plain     = jsonToRGB24(color_data["text"]);
            format.color.outline.plain  = jsonToRGB24(color_data["outline"]);
            format.color.shadow.plain   = jsonToRGB24(color_data["shadow"]);
            format.color.alpha          = color_data["alpha"];
            // Language
            nlohmann::json const& lng_data = format_data["lng"];
            format.lng.language         = lng_data["language"];
            format.lng.script           = lng_data["script"];
            format.lng.direction        = lng_data["direction"];
            format.lng.word_dividers    = SSS::strToStr32(format_data["lng"]["word_dividers"]);
            // Set format
            area->setFormat(format, format_data["id"]);
        }
        if (area_data.count("string_array") != 0) {
            g_data->texts.clear();
            g_data->texts.reserve(area_data["string_array"].size());
            for (std::string const& str : area_data["string_array"]) {
                g_data->texts.push_back(str);
            }
            area->parseString(g_data->texts[0]);
        }
        else {
            area->parseString("This is the second text area");
        }
    }
}
CATCH_AND_LOG_FUNC_EXC