#include "MapOverlay.hpp"
#include "Dialog.hpp"
#include "FoldingMap.hpp"
#include "MapContent.hpp"
#include "SceneRenderer.hpp"
#include "lua_include.hpp"

#include <chrono>
#include <string_view>

namespace {
    // Map: the images' pixel size, in world units
    constexpr float map_w = 1260.f;
    constexpr float map_h = 744.f;
    constexpr char const* front_image = "map/map_front.png";
    constexpr char const* back_image = "map/map_back.png";

    // Creases on the images, in pixels: the vertical ones' x, the
    // horizontal one's y
    constexpr float crease_x[3]{ 272.f, 614.f, 965.f };
    constexpr float crease_y = 375.f;

    // Opening timeline, in seconds: hold on the folded map, open the half
    // (the cover flips up), then the accordion (all the vertical folds at
    // once)
    constexpr float hold = 0.4f;
    constexpr float half_start = hold;
    constexpr float half_time = 1.2f;
    constexpr float accordion_start = half_start + half_time;
    constexpr float accordion_time = 1.1f;
    constexpr float end_time = accordion_start + accordion_time;
    // Closing: every fold at once, from wherever they were, the map sliding
    // out of the screen toward the bottom right
    constexpr float closing_time = 0.8f;
    // Screen share around the map
    constexpr float margin = 1.05f;
    // Scale of the folded map, centered on screen, as opening starts (eased
    // to 1 as it moves to its place), and as closing ends
    constexpr float opening_scale = 1.2f;
    constexpr float closing_scale = 0.8f;
    // Accordion's depth scale (see FoldingMap::setDepthScale): the panels
    // slide flat, as against a board
    constexpr float depth_scale = 0.05f;
    // In front of the map (orthographic: the distance doesn't change its
    // size)
    constexpr float camera_z = 2000.f;
    // Faux perspective (see FoldingMap::pinch): a panel this far behind the
    // map's plane is drawn at d / (d + this) of its size. Subtle: a column
    // folded edge-on reaches ~0.7.
    constexpr float pinch_distance = 800.f;
    // Open, the map isn't flat: its vertical folds stay a little closed
    // (0 -> 1), columns 0 & 2 tilted by accordion_rest * 180 degrees, the
    // creases showing. The horizontal fold opens flat (half_rest = 0).
    constexpr float accordion_rest = 0.1f;
    constexpr float half_rest = 0.f;

    using Clock = std::chrono::steady_clock;

    struct State {
        SSS::GL::Camera::Shared camera;
        SceneRenderer::Shared renderer;
        FoldingMap::Shared map;
        // Printed on it: roads, monuments, characters' pins
        std::unique_ptr<map_content::Layers> layers;
        // Local center of the map's current shape, kept on the screen's
        // center while opening, kept as is while closing
        glm::vec2 center{ 0.f };
        // Time along the opening timeline: 0 folded, end_time unfolded
        float t{ 0.f };
        // Closing: 0 -> 1, from the folds closing started at to folded. Back
        // to 0 when reopened, then the opening timeline goes on.
        float closing_t{ 0.f };
        float from_half{ 0.f };
        float from_accordion{ 0.f };
        bool shown{ false };
        // The full opening is played once, then opening plays closing
        // backward
        bool opened_once{ false };
        bool closing{ false };
        Clock::time_point last_update;
    };
    std::unique_ptr<State> state;

    // Easings' t, 0 -> 1 (SSS::Math's don't clamp)
    float unit(float t)
    {
        return std::clamp(t, 0.f, 1.f);
    }

    // Needs the window's context (FoldingMap's & SceneRenderer's GL objects)
    void create()
    {
        state = std::make_unique<State>();
        // 2D: 1 world unit = 1 pixel at zoom 1, zoomed to fit the map
        state->camera = SSS::GL::Camera::create();
        state->camera->setProjectionType(SSS::GL::Camera::Projection::OrthoFixed);
        state->camera->setRange(1.f, 5000.f);

        state->renderer = SceneRenderer::create(state->camera);
        // Over the scene: not hidden by its depth, nor blurred with it
        state->renderer->clear_depth_buffer = true;
        state->renderer->dialog_blur = false;
        // Unlit: no light, white ambient (meshes are only drawn lit)
        state->renderer->ambient = glm::vec3(1.f);
        state->renderer->shadows = false;

        state->map = FoldingMap::create(
            SSS::GL::Texture::create(std::filesystem::path{ front_image }),
            SSS::GL::Texture::create(std::filesystem::path{ back_image }), map_w, map_h);
        state->map->setCreases({ crease_x[0] / map_w, crease_x[1] / map_w, crease_x[2] / map_w },
            crease_y / map_h);
        state->map->setDepthScale(depth_scale);
        state->map->cast_shadow = false;
        // Turned 180 degrees in its plane, centered on the origin
        state->map->setRotation(glm::vec3(0.f, 0.f, 180.f));
        state->layers = std::make_unique<map_content::Layers>(map_w, map_h);
        state->map->layers = state->layers->textures();
        state->map->overlays = state->layers->overlays();
        state->renderer->addMesh(state->map);
    }

    // Open, flat & still: what's printed on it can be clicked, & animated
    bool fullyOpen()
    {
        return state->shown && !state->closing && state->closing_t <= 0.f && state->t >= end_time;
    }

    // The cursor on the flat map, in its pixels as seen on screen (from its
    // top left), if over it
    std::optional<glm::vec2> cursorOnMap()
    {
        auto const [w, h] = g->window->getDimensions();
        if (w <= 0 || h <= 0)
            return std::nullopt;
        auto const [cx, cy] = g->window->getCursorPos();
        // Orthographic, 1 world unit = zoom pixels, straight on (y up)
        float const zoom = state->camera->getZoom();
        glm::vec3 const cam = state->camera->getPosition();
        glm::vec2 const world = glm::vec2(cam)
            + glm::vec2(cx - w / 2.f, h / 2.f - cy) / zoom;
        // Local: the paper, turned 180 degrees on screen (the model turns
        // it), seen straight on: the folded shape's texel under the cursor
        glm::vec4 const local = glm::inverse(state->map->getModelMat4()) * glm::vec4(world, 0.f, 1.f);
        auto const uv = state->map->uvAt(glm::vec2(local));
        if (!uv)
            return std::nullopt;
        // Image pixels from its top left, then turned as seen on screen
        return glm::vec2(map_w, map_h) - *uv * glm::vec2(map_w, map_h);
    }

    // map_overlay.on_location_clicked(id, scene), see global_setup.lua
    void callLocationHook(std::string const& id)
    {
        map_content::Location const* loc = map_content::data().location(id);
        sol::object const hook = g->lua()["map_overlay"]["on_location_clicked"];
        if (!loc || hook.get_type() != sol::type::function)
            return;
        sol::protected_function const f = hook;
        sol::protected_function_result const result = f(id, loc->scene);
        if (!result.valid()) {
            sol::error const err = result;
            LOG_CTX_ERR("map_overlay.on_location_clicked", err.what());
        }
    }

    // Folds & frames the map at the current time. The map is turned 180
    // degrees in its plane: its blue cover (the front's bottom left panel)
    // is at the top right once unfolded. Folded, it shows the cover, the
    // crease on top, scaled up. Opening the first time, its shape is kept
    // centered on screen: the cover flips up (going behind), showing column
    // 3, then the accordion slides open to the left, flat as against a
    // board, the map scaled back to its size, ending in its place. Closing,
    // everything folds back at once, the map shrinking & sliding out of the
    // screen toward the bottom right. Opening again plays that backward.
    // The camera doesn't move: the whole map fits the screen.
    void pose()
    {
        FoldingMap& map = *state->map;
        float const t = state->t;
        // Folded (1) to open, a little folded still (rest)
        // Slow start & end, sharper than a cubic: the cover's flip
        float half = glm::mix(half_rest, 1.f, 1.f - SSS::Math::easeInOutQuart(unit((t - half_start) / half_time)));
        // Fast start, quick stop: the accordion pulled open
        float const opened = SSS::Math::easeOutCubic(unit((t - accordion_start) / accordion_time));
        float accordion = glm::mix(accordion_rest, 1.f, 1.f - opened);
        // Opening: scaled up, back to its size with the accordion
        float scale = glm::mix(opening_scale, 1.f, opened);
        float slide = 0.f;
        if (state->closing_t > 0.f) {
            float const c = SSS::Math::easeInOutQuart(unit(state->closing_t));
            half = glm::mix(state->from_half, 1.f, c);
            accordion = glm::mix(state->from_accordion, 1.f, c);
            scale *= glm::mix(1.f, closing_scale, c);
            // Slow start, fast end: the map thrown out of the screen
            slide = SSS::Math::easeInCubic(unit(state->closing_t));
        }
        map.setHalfFold(half);
        map.setFlap(accordion);
        map.setAccordion(accordion);
        map.update();
        if (state->closing_t <= 0.f)
            state->center = (glm::vec2(map.getBoundsMin()) + glm::vec2(map.getBoundsMax())) / 2.f;

        state->camera->setPosition(glm::vec3(0.f, 0.f, camera_z));
        map.pinch = glm::vec4(0.f, 0.f, 0.f, pinch_distance);
        auto const [w, h] = g->window->getDimensions();
        if (w > 0 && h > 0) {
            float const fw = static_cast<float>(w), fh = static_cast<float>(h);
            float const zoom = std::min(fw / map_w, fh / map_h) / margin;
            state->camera->setZoom(zoom);
            // The shape's center (turned 180 degrees, scaled) on the
            // screen's: unfolded, the map's center. Closing: toward
            // the bottom right, by the screen's size (world units): the
            // folded map, at the bottom right, ends out of it.
            glm::vec2 const screen(fw / zoom, fh / zoom);
            glm::vec2 const centering = state->center * scale;
            map.setScaling(glm::vec3(scale));
            map.setTranslation(glm::vec3(centering + slide * glm::vec2(screen.x, -screen.y), 0.f));
        }
    }

    // GLFW key codes are the keys' US layout positions: the key typing "m"
    // depends on the layout (AZERTY: GLFW_KEY_SEMICOLON). Looked up among
    // the printable keys, checked again every second (layout switches).
    int toggleKey()
    {
        static int key = GLFW_KEY_M;
        static Clock::time_point checked;
        auto const now = Clock::now();
        if (now - checked < std::chrono::seconds(1))
            return key;
        checked = now;
        key = GLFW_KEY_M;
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_WORLD_2; ++k) {
            char const* name = glfwGetKeyName(k, 0);
            if (name && (std::string_view(name) == "m" || std::string_view(name) == "M")) {
                key = k;
                break;
            }
        }
        return key;
    }

    void hideRenderer()
    {
        g->window->removeRenderer(state->renderer);
        state->shown = false;
        state->closing = false;
        state->t = 0.f;
        state->closing_t = 0.f;
    }
}

void MapOverlay::open()
{
    if (Dialog::anyVisible())
        return;
    SSS::GL::Context const context = g->window->setContext();
    if (!state)
        create();
    state->closing = false;
    if (state->shown)
        return;
    state->shown = true;
    state->t = 0.f;
    if (state->opened_once) {
        // Closing backward, from closed to open
        state->t = end_time;
        state->closing_t = 1.f;
        state->from_half = half_rest;
        state->from_accordion = accordion_rest;
        state->center = glm::vec2(0.f);
    }
    state->opened_once = true;
    state->last_update = Clock::now();
    pose();
    state->layers->setHovered(std::nullopt);
    state->layers->update(0.f, false);
    state->map->overlays = state->layers->overlays();
    state->map->overlay_rects = state->layers->overlayRects();
    g->window->addRenderer(state->renderer);
}

void MapOverlay::close()
{
    if (!state || !state->shown || state->closing)
        return;
    state->closing = true;
    // Already closing (reopened midway): goes on from there
    if (state->closing_t > 0.f)
        return;
    state->from_half = state->map->getHalfFold();
    state->from_accordion = state->map->getAccordion();
}

void MapOverlay::hide()
{
    if (!isShown())
        return;
    SSS::GL::Context const context = g->window->setContext();
    hideRenderer();
}

void MapOverlay::toggle()
{
    if (isOpen())
        close();
    else
        open();
}

bool MapOverlay::isShown() noexcept
{
    return state && state->shown;
}

bool MapOverlay::isOpen() noexcept
{
    return isShown() && !state->closing;
}

void MapOverlay::update()
{
    // M (whatever the layout): Escape is handled by the main loop (see
    // mylua_on_escape)
    if (g->window->getKeyInputs()[toggleKey()].is_pressed())
        toggle();
    if (!isShown())
        return;
    if (Dialog::anyVisible())
        close();

    auto const now = Clock::now();
    // Capped, as the scenes' dt
    float const dt = std::min(std::chrono::duration<float>(now - state->last_update).count(), 0.1f);
    state->last_update = now;
    if (state->closing) {
        state->closing_t += dt / closing_time;
        if (state->closing_t >= 1.f) {
            SSS::GL::Context const context = g->window->setContext();
            hideRenderer();
            return;
        }
    }
    // Reopened while closing: unfolds back to where closing started first
    else if (state->closing_t > 0.f)
        state->closing_t = std::max(state->closing_t - dt / closing_time, 0.f);
    else
        state->t = std::min(state->t + dt, end_time);
    pose();

    // What's printed on it: hovered & clicked when open, flat & still
    bool const open = fullyOpen() && !Dialog::anyVisible();
    std::optional<std::string> hovered;
    if (open) {
        if (auto const pos = cursorOnMap())
            hovered = state->layers->locationAt(*pos);
    }
    state->layers->setHovered(hovered);
    {
        SSS::GL::Context const context = g->window->setContext();
        state->layers->update(dt, open);
    }
    // A sprite's texture when one thing pops: changes with what animates
    state->map->overlays = state->layers->overlays();
    state->map->overlay_rects = state->layers->overlayRects();
    if (hovered && g->window->getClickInputs()[GLFW_MOUSE_BUTTON_LEFT].is_pressed()) {
        close();
        callLocationHook(*hovered);
    }
}

void MapOverlay::free()
{
    if (!state)
        return;
    SSS::GL::Context const context = g->window->setContext();
    if (state->shown)
        hideRenderer();
    state.reset();
}
