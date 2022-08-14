#include "Summergasm.hpp"

using namespace SSS::GL;

static std::vector<Camera::Shared> cameras;
static std::vector<Plane::Shared> planes;

static nlohmann::json filepathToJson(std::string const& path)
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
    nlohmann::json const data = filepathToJson(json_path);
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

static void loadTexture(Window::Shared const& window, nlohmann::json const& data) noexcept try
{
    if (!data.is_object())
        return;

    Texture::Ptr const& texture = window->createTexture(data["id"]);
    if (data.count("filepath") != 0) {
        texture->loadImage(data["filepath"]);
    }
    if (data.count("text_area_id") != 0) {
        texture->setTextAreaID(data["text_area_id"]);
    }
    texture->setType(static_cast<Texture::Type>(data["type"]));
}
CATCH_AND_LOG_FUNC_EXC;

static void loadCamera(Window::Shared const& window, nlohmann::json const& data) noexcept try
{
    if (!data.is_object())
        return;

    glm::vec2 vec2;
    glm::vec3 vec3;

    Camera::Shared const& camera = cameras.emplace_back(Camera::create(window));
    camera->setProjectionType(static_cast<Camera::Projection>(data["projection"]));
    camera->setFOV(data["fov"]);
    camera->setRange(data["range"]["near"], data["range"]["far"]);
    camera->setPosition(vec3 << data["position"]);
    camera->setRotation(vec2 << data["rotation"]);
}
CATCH_AND_LOG_FUNC_EXC;

static void loadPlane(Window::Shared const& window, nlohmann::json const& data) noexcept try
{
    if (!data.is_object())
        return;

    glm::vec3 vec3;

    Plane::Shared const& plane = planes.emplace_back(Plane::create(window));
    plane->setTextureID(data["texture_id"]);
    plane->setHitbox(static_cast<Plane::Hitbox>(data["hitbox"]));
    plane->setScaling(vec3 << data["scaling"]);
    plane->setRotation(vec3 << data["rotation"]);
    plane->setTranslation(vec3 << data["translation"]);
    plane->setOnClickFuncID(data["on_click_function_id"]);
    plane->setPassiveFuncID(data["passive_function_id"]);
}
CATCH_AND_LOG_FUNC_EXC;

static void loadRenderer(Window::Shared const& window, nlohmann::json const& data) noexcept try
{
    if (!data.is_object())
        return;

    int const id = data["id"];
    int const type_id = data["type"];

    // Create the renderer if needed
    switch (type_id) {
    case 0: {
        auto& renderer = window->createRenderer<PlaneRenderer>(id)->castAs<PlaneRenderer>();
        renderer.title = data["title"];
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
        window->createRenderer<LineRenderer>(id);
    }   break;
    default:
        SSS::throw_exc("Unkown type_id of renderer : " + std::to_string(type_id));
    }
}
CATCH_AND_LOG_FUNC_EXC;

void loadScene(Window::Shared const& window, std::string const& json_path) try
{
    nlohmann::json const data = filepathToJson(json_path);
    Window::Objects const& objects = window->getObjects();

    // Textures
    for (nlohmann::json const& tex_data : data["textures"]) {
        loadTexture(window, tex_data);
    }
    // Cameras
    for (nlohmann::json const& cam_data : data["cameras"]) {
        loadCamera(window, cam_data);
    }
    // Planes
    for (nlohmann::json const& plane_data : data["planes"]) {
        loadPlane(window, plane_data);
    }
    // Renderers
    for (nlohmann::json const& renderer_data : data["renderers"]) {
        loadRenderer(window, renderer_data);
    }

    // Clear static vectors
    cameras.clear();
    planes.clear();
}
CATCH_AND_LOG_FUNC_EXC;

static SSS::RGB24 jsonToRGB24(nlohmann::json const& color) noexcept
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

    nlohmann::json const data = filepathToJson(json_path);
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
CATCH_AND_LOG_FUNC_EXC;