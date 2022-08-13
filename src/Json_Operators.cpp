#include "Summergasm.hpp"

using namespace SSS::GL;

static std::vector<Camera::Shared> cameras;
static std::vector<Plane::Shared> planes;

static nlohmann::json relativePathToJson(std::string const& path)
{
    return nlohmann::json::parse(SSS::readFile(SSS::pathWhich(path)));
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

Window::Shared createWindow(std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    Window::CreateArgs args;
    args.title      = data["title"];
    args.w          = data["width"];
    args.h          = data["height"];
    args.monitor_id = data["monitor_id"];
    args.fullscreen = data["fullscreen"];
    args.maximized = data["maximized"];
    args.iconified = data["iconified"];
    args.hidden = data["hidden"];
    return Window::create(args);
}

static void loadRenderer(Window::Shared const& window, nlohmann::json const& data)
{
    Window::Objects const& objects = window->getObjects();
    
    int const id = data["id"];
    int const type_id = data["type"];

    // Ensure that if a renderer already exists, it is the corresponding type
    if (objects.renderers.count(id) != 0) {
        try {
            switch (type_id) {
            case 0:
                objects.renderers.at(id)->castAs<PlaneRenderer>();
                break;
            case 1:
                objects.renderers.at(id)->castAs<LineRenderer>();
                break;
            }
        }
        catch (...) {
            window->removeRenderer(id);
        }
    }

    // Create the renderer if needed
    if (objects.renderers.count(id) == 0) {
        switch (type_id) {
        case 0:
            window->createRenderer<PlaneRenderer>(id);
            break;
        case 1:
            window->createRenderer<LineRenderer>(id);
            break;
        default:
            SSS::throw_exc("Unkown type_id of renderer : " + std::to_string(type_id));
        }
    }

    // Fill renderer infos
    Renderer::Ptr const& renderer_ptr = objects.renderers.at(id);
    renderer_ptr->title = data["title"];
    switch (type_id) {
    case 0: {
        auto& renderer = renderer_ptr->castAs<PlaneRenderer>();
        renderer.chunks.clear();
        for (nlohmann::json const& chunk_data : data["chunks"]) {
            PlaneRenderer::Chunk& chunk = renderer.chunks.emplace_back();
            chunk.title = chunk_data["title"];
            int const cam_id = chunk_data["camera"];
            if (cameras.size() > cam_id)
                chunk.camera = cameras.at(cam_id);
            chunk.reset_depth_before = chunk_data["reset_depth_before"];
            nlohmann::json const& plane_ids = chunk_data["planes"];
            for (uint32_t i = 0; i < plane_ids.size(); ++i) {
                chunk.planes.emplace_back(planes.at(plane_ids[i]));
            }
        }

    }   break;
    case 1: {
    }   break;
    default:
        SSS::throw_exc("Unkown type_id of renderer : " + std::to_string(type_id));
    }
}

void loadStaticObjects(Window::Shared const& window, std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    Window::Objects const& objects = window->getObjects();

    // Textures
    for (nlohmann::json const& tex_data : data["textures"]) {
        window->createTexture(tex_data["id"]);
        Texture::Ptr const& texture = objects.textures.at(tex_data["id"]);
        if (tex_data.count("filepath") != 0) {
            texture->loadImage(tex_data["filepath"]);
        }
        if (tex_data.count("text_area_id") != 0) {
            texture->setTextAreaID(tex_data["text_area_id"]);
        }
        texture->setType(static_cast<Texture::Type>(tex_data["type"]));
    }
    // Renderers
    for (nlohmann::json const& renderer_data : data["renderers"]) {
        loadRenderer(window, renderer_data);
    }
}

void loadScene(Window::Shared const& window, std::string const& json_path)
{
    nlohmann::json const data = relativePathToJson(json_path);
    Window::Objects const& objects = window->getObjects();
    glm::vec3 vec3;
    glm::vec2 vec2;

    // Cameras
    for (nlohmann::json const& cam_data : data["cameras"]) {
        Camera::Shared const& camera = cameras.emplace_back(Camera::create());
        camera->setProjectionType(static_cast<Camera::Projection>(cam_data["projection"]));
        camera->setFOV(cam_data["fov"]);
        camera->setRange(cam_data["range"]["near"], cam_data["range"]["far"]);
        camera->setPosition(vec3 << cam_data["position"]);
        camera->setRotation(vec2 << cam_data["rotation"]);
    }
    // Planes
    for (nlohmann::json const& plane_data : data["planes"]) {
        Plane::Shared const& plane = planes.emplace_back(Plane::create());
        plane->setTextureID(plane_data["texture_id"]);
        plane->setHitbox(static_cast<Plane::Hitbox>(plane_data["hitbox"]));
        plane->setScaling(vec3 << plane_data["scaling"]);
        plane->setRotation(vec3 << plane_data["rotation"]);
        plane->setTranslation(vec3 << plane_data["translation"]);
        plane->setOnClickFuncID(plane_data["on_click_function_id"]);
        plane->setPassiveFuncID(plane_data["passive_function_id"]);
    }

    // Renderers
    for (nlohmann::json const& renderer_data : data["renderers"]) {
        loadRenderer(window, renderer_data);
    }

    // Clear static vectors
    cameras.clear();
    planes.clear();
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
    using namespace SSS::TR;

    nlohmann::json const data = relativePathToJson(json_path);
    Area::Map const& areas = Area::getMap();
    for (nlohmann::json const& area_data : data) {
        uint32_t id = area_data["id"];
        Area::create(id, area_data["width"], area_data["height"]);
        Area::Ptr const& area = areas.at(id);
        area->setPrintMode(static_cast<Area::PrintMode>(area_data["print_mode"]));
        for (nlohmann::json const& format_data : area_data["text_opt"]) {
            // Font
            Format format;
            format.font = format_data["font"];
            loadFont(format.font);
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