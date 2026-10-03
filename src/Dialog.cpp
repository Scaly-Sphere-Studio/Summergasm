// Port of the Documentation's renpy_scene example (examples/renpy_scene.cpp)
// as a dialog node any Summergasm scene can embed, see Dialog.hpp.
// This is the container: it plays the conversation and dispatches it to its
// parts (src/dialog/), which share one UIRenderer.
#include "Dialog.hpp"
#include "GameState.hpp"
#include "dialog/DialogNodes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;
using namespace dialog;

struct Dialog::Impl {
    Impl();
    ~Impl();

    DialogContext ctx;
    // The Player keeps a reference to its Script
    std::unique_ptr<renpy::Script> script;
    std::unique_ptr<renpy::Player> player;
    bool hidden = false;
    // Inputs of the frame start() / show() was called on are ignored: the
    // click that opened the dialog is not its first "next line".
    bool skip_input = false;
    std::function<void()> on_finished;
    SignalCallback on_signal;
    // Incremented by start() & stop(): a signal callback may have changed
    // the conversation
    uint64_t generation = 0;

    // State signaled so far ("background" & "music"), to signal its changes
    renpy::Background signaled_background;
    renpy::Music signaled_music;
    // Variables written to the game state so far
    renpy::Player::Vars synced_vars;

    // Conversations being played, updated by Dialog::updateAll()
    static inline std::vector<Impl*> running;

    // Built in this order, back to front (see DialogNode)
    DialogCharacters characters;
    DialogBox        box;
    DialogChoices    choices;
    DialogControls   controls;
    DialogLog        log;
    std::array<DialogNode*, 5> nodes() { return { &characters, &box, &choices, &controls, &log }; }
    DialogNode& node(Part part);

    void start(std::string const& path);
    void stop();
    void update();
    void hide();
    void show();

    void build(int w, int h);
    void destroy();
    void rebuildIfResized();
    // A new step (or the current one, once rebuilt). forward: the story
    // moved on (sends the step's one-shot signals). false when the
    // conversation was stopped or replaced: by a signal callback, or as the
    // step is the End and there's no end text
    bool present(bool animate, bool forward);
    void syncVars();
    // false if the callback stopped or replaced the conversation
    bool signal(renpy::Signal const& signal);
    void finish();
};

Dialog::Impl::Impl()
    : ctx(mainWindow())
{
}

Dialog::Impl::~Impl()
{
    stop();
}

DialogNode& Dialog::Impl::node(Part part)
{
    switch (part) {
        case Part::Characters: return characters;
        case Part::Box:        return box;
        case Part::Choices:    return choices;
        default:               return controls;
    }
}

// Extensions a script may have, by order of preference
static constexpr std::array<std::string_view, 2> script_extensions = { ".rpy", ".txt" };

static bool isScriptExtension(fs::path const& path)
{
    std::string ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return std::ranges::find(script_extensions, ext) != script_extensions.end();
}

static void warnUnsupported(fs::path const& path)
{
    LOG_CTX_WRN("Dialog", "'" + path.extension().string() + "' extension is not supported ("
        + path.string() + "), expected .rpy or .txt");
}

// The file at path if it's a script, else the scripts named path.* (each
// file of another extension being warned about). Empty if none.
static fs::path findScriptAt(fs::path const& path)
{
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) {
        if (isScriptExtension(path))
            return path;
        warnUnsupported(path);
    }
    fs::path const dir = path.has_parent_path() ? path.parent_path() : fs::path(".");
    if (!fs::is_directory(dir, ec))
        return {};
    fs::path const stem = path.filename();
    fs::path found;
    for (fs::directory_entry const& entry : fs::directory_iterator(dir, ec)) {
        fs::path const& file = entry.path();
        if (!entry.is_regular_file(ec) || file.stem() != stem || file == path)
            continue;
        if (!isScriptExtension(file))
            warnUnsupported(file);
        // .rpy preferred over .txt
        else if (found.empty() || file.extension() == script_extensions[0])
            found = file;
    }
    return found;
}

// Script of the given name: looked up in the dialogs folder first
// ("dial1" -> dialogs/dial1.*), else as a direct path (absolute, or relative
// to the working directory). Only .rpy & .txt files are accepted.
static fs::path findScript(std::string const& name)
{
    fs::path const in_folder = fs::path(g->dialogs_folder) / name;
    if (fs::path found = findScriptAt(in_folder); !found.empty())
        return fs::absolute(found);
    // An absolute name was already looked up as is
    if (fs::path(name) != in_folder) {
        if (fs::path found = findScriptAt(name); !found.empty())
            return fs::absolute(found);
    }
    SSS::throw_exc("Dialog: no " + name + "(.rpy|.txt) in " + g->dialogs_folder + " nor as a direct path");
}

// The UIRenderer & SDF textures make raw GL calls: the window's context must
// be current (Lua scenes run without any), hence the setContext() calls.
void Dialog::Impl::start(std::string const& name)
{
    SSS::GL::Context const context = ctx.window.setContext();
    // Loaded first: if it throws, the current conversation goes on.
    // Images are looked up in the assets folder, music & sounds in the sounds one.
    auto new_script = std::make_unique<renpy::Script>(renpy::Script::load(findScript(name).string(),
        fs::absolute(g->assets_folder).string(), fs::absolute(g->sounds_folder).string()));

    stop();
    ++generation;
    script = std::move(new_script);
    // Starts with the game's values, the script's `default`s set the missing ones
    synced_vars = renpy::Player::Vars(game_state::vars().begin(), game_state::vars().end());
    player = std::make_unique<renpy::Player>(*script, renpy::MarkupStyle{}, synced_vars);
    ctx.player = player.get();
    skip_input = true;
    running.push_back(this);

    // Not built while minimized (0 x 0): a 1 x 1 UI is rebuilt at the first update
    auto const [w, h] = ctx.window.getDimensions();
    build(std::max(w, 1), std::max(h, 1));
    present(true, true);
}

void Dialog::Impl::stop()
{
    if (!player) return;
    SSS::GL::Context const context = ctx.window.setContext();
    destroy();
    std::erase(running, this);
    player.reset();
    script.reset();
    ++generation;
    ctx.player = nullptr;
    ctx.typing = false;
    ctx.settings.log_open = false;
    hidden = false;
    signaled_background = {};
    signaled_music = {};
    synced_vars.clear();
}

void Dialog::Impl::finish()
{
    stop();
    // May start another conversation
    if (on_finished) on_finished();
}

void Dialog::Impl::build(int w, int h)
{
    ctx.L.emplace(float(w), float(h));
    ctx.ui = SSS::GL::UIRenderer::create();
    ctx.ui->updateResolution(ctx.L->W, ctx.L->H);
    if (!hidden) ctx.window.addRenderer(ctx.ui);
    for (DialogNode* n : nodes()) n->build(ctx);
}

// Releases every GL object of the UI (while the GL context is current).
void Dialog::Impl::destroy()
{
    auto const all = nodes();
    for (auto it = all.rbegin(); it != all.rend(); ++it) (*it)->destroy();
    // Deletes the text nodes popped by the parts (nothing else calls it)
    SSS::SceneGraph::update();
    if (ctx.ui) {
        if (!hidden) ctx.window.removeRenderer(ctx.ui);
        ctx.ui.reset();
    }
    ctx.L.reset();
}

// Window resized: rebuild the UI for the new size, the story goes on where
// it was (the current line is shown at once). Skipped while minimized (0 x 0).
void Dialog::Impl::rebuildIfResized()
{
    auto const [w, h] = ctx.window.getDimensions();
    if (w <= 0 || h <= 0 || (w == int(ctx.L->W) && h == int(ctx.L->H)))
        return;
    destroy();
    build(w, h);
    if (!present(false, false)) return;
    if (ctx.settings.log_open) log.setOpen(ctx, true);
}

bool Dialog::Impl::present(bool animate, bool forward)
{
    // Signal callbacks may read the game's values
    syncVars();

    // Copied: a callback may destroy the player
    renpy::SceneState const scene = player->current().scene;
    if (scene.background != signaled_background) {
        signaled_background = scene.background;
        renpy::Signal sig{ "background", {} };
        if (scene.background.color)               sig.args.push_back(int64_t(*scene.background.color));
        else if (!scene.background.image.empty()) sig.args.push_back(scene.background.image);
        if (!signal(sig)) return false;
    }
    if (scene.music != signaled_music) {
        signaled_music = scene.music;
        renpy::Signal sig{ "music", {} };
        if (!scene.music.file.empty()) sig.args = { scene.music.file, scene.music.loop };
        if (!signal(sig)) return false;
    }
    if (forward) {
        auto const signals = player->current().signals;
        for (auto const& sig : signals)
            if (!signal(sig)) return false;
    }

    auto const& step = player->current();
    if (step.kind == renpy::Step::Kind::End && ctx.end_text.empty()) {
        finish();
        return false;
    }
    for (DialogNode* n : nodes()) n->present(ctx, step, animate);
    return true;
}

// Writes the variables changed since the last step (going back too) to the
// game state. Renamed characters (`$ who.name = ...`) stay in the conversation.
void Dialog::Impl::syncVars()
{
    auto const& vars = player->vars();
    for (auto const& [name, value] : vars) {
        if (name.ends_with(".name")) continue;
        auto const it = synced_vars.find(name);
        if (it == synced_vars.end() || it->second != value)
            game_state::set(name, value);
    }
    synced_vars = vars;
}

bool Dialog::Impl::signal(renpy::Signal const& sig)
{
    uint64_t const gen = generation;
    if (on_signal) {
        try {
            on_signal(sig);
        }
        catch (std::exception const& e) {
            LOG_CTX_ERR("Dialog.on_signal", e.what());
        }
        if (gen != generation) return false;
    }

    // The dialog only reports what the script says: the host interprets it
    // (see SignalManager.hpp), unhandled signals are ignored
    return true;
}

void Dialog::Impl::hide()
{
    if (!player || hidden) return;
    SSS::GL::Context const context = ctx.window.setContext();
    // Not typed unseen
    if (ctx.typing) box.finishLine(ctx);
    ctx.window.removeRenderer(ctx.ui);
    hidden = true;
}

void Dialog::Impl::show()
{
    if (!player || !hidden) return;
    SSS::GL::Context const context = ctx.window.setContext();
    hidden = false;
    ctx.window.addRenderer(ctx.ui);
    // Auto mode & choices delays start over
    ctx.shown_at = Clock::now();
    skip_input = true;
    rebuildIfResized();
}

void Dialog::Impl::update()
{
    if (!player || hidden) return;
    SSS::GL::Context const context = ctx.window.setContext();

    rebuildIfResized();
    if (!player) return;

    auto const& keys   = ctx.window.getKeyInputs();
    auto const& clicks = ctx.window.getClickInputs();
    auto const [cx, cy] = ctx.window.getCursorPos();

    DialogInput input;
    input.cursor = glm::vec2(cx, cy);
    if (!std::exchange(skip_input, false)) {
        input.confirm = keys[GLFW_KEY_SPACE].is_pressed() || keys[GLFW_KEY_ENTER].is_pressed()
                     || keys[GLFW_KEY_KP_ENTER].is_pressed();
        input.click   = clicks[GLFW_MOUSE_BUTTON_LEFT].is_pressed();
        input.back    = keys[GLFW_KEY_BACKSPACE].is_pressed() || keys[GLFW_KEY_LEFT].is_pressed();
        input.cursor_moved = ctx.window.getCursorDiff() != std::make_tuple(0, 0);
    }

    // Settings: a click on them is not a "next line" input.
    auto const actions = controls.handleInput(ctx, input);
    if (actions.speed_changed) box.applyTextSpeed(ctx);
    if (actions.toggle_log) {
        log.setOpen(ctx, !ctx.settings.log_open);
        controls.refresh(ctx);
    }

    if (ctx.typing && box.typewriterDone())
        box.finishLine(ctx);

    using Kind = renpy::Step::Kind;
    auto const kind = player->current().kind;
    choices.reveal(ctx);
    bool const auto_next = ctx.settings.auto_mode && !ctx.settings.log_open && kind == Kind::Say
        && !ctx.typing && ctx.shownFor() >= AUTO_DELAY;
    bool changed = false;
    bool animate = true;

    if (ctx.settings.log_open) {
        // The log takes every input but the settings: the story waits.
        log.scroll(ctx);
    }
    else if (input.back) {
        changed = player->back();
        animate = false;
    }
    else if (ctx.typing && (input.confirm || input.click)) {
        box.finishLine(ctx);                // first input shows the whole line
    }
    else if (kind == Kind::Say && (input.confirm || input.click || auto_next)) {
        player->next();
        changed = true;
        box.pressed();
    }
    else if (kind == Kind::Menu && choices.shown()) {
        if (auto const pick = choices.handleInput(ctx, input)) {
            player->choose(*pick);
            changed = true;
        }
    }
    else if (kind == Kind::End && (input.confirm || input.click)) {
        finish();
        return;
    }

    // Only going back doesn't animate
    if (changed && !present(animate, animate))
        return;
    for (DialogNode* n : nodes()) n->update(ctx, input);
}

// ─────────────────────────────────────────────────────────────────────────
Dialog::Dialog()
    : _impl(std::make_unique<Impl>())
{
}

Dialog::~Dialog() = default;

void Dialog::start(std::string const& path) { _impl->start(path); }
void Dialog::stop()   { _impl->stop(); }
void Dialog::hide()   { _impl->hide(); }
void Dialog::show()   { _impl->show(); }

void Dialog::updateAll()
{
    // Copy: the finished callbacks may start, stop or destroy dialogs
    auto const dialogs = Impl::running;
    for (Impl* d : dialogs) {
        if (std::find(Impl::running.begin(), Impl::running.end(), d) == Impl::running.end())
            continue;
        try {
            d->update();
        }
        catch (std::exception const& e) {
            LOG_CTX_ERR("Dialog::updateAll", e.what());
        }
    }
}

bool Dialog::isActive() const noexcept  { return _impl->player != nullptr; }
bool Dialog::isHidden() const noexcept  { return _impl->hidden; }
bool Dialog::isLogOpen() const noexcept { return _impl->ctx.settings.log_open; }

void Dialog::setPartShown(Part part, bool shown)
{
    // Parts change the alpha of their planes
    SSS::GL::Context const context = _impl->ctx.window.setContext();
    _impl->node(part).setEnabled(shown);
}

bool Dialog::isPartShown(Part part) const noexcept
{
    return _impl->node(part).isEnabled();
}

void Dialog::setEndText(std::string text)              { _impl->ctx.end_text = std::move(text); }
std::string const& Dialog::getEndText() const noexcept { return _impl->ctx.end_text; }

void Dialog::setOnFinished(std::function<void()> callback)
{
    _impl->on_finished = std::move(callback);
}

void Dialog::setOnSignal(SignalCallback callback)
{
    _impl->on_signal = std::move(callback);
}

std::string Dialog::backgroundImage() const
{
    if (!_impl->player) return {};
    auto const& bg = _impl->player->current().scene.background;
    return bg.color ? std::string() : bg.image;
}

std::optional<uint32_t> Dialog::backgroundColor() const
{
    if (!_impl->player) return std::nullopt;
    return _impl->player->current().scene.background.color;
}
