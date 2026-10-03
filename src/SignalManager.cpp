#include "SignalManager.hpp"
#include "renpy/RenpyParser.h"

#include <algorithm>

namespace signals {

namespace {

struct Entry {
    uint32_t id;
    std::string name;
    Handler handler;
};

std::vector<Entry> entries;
uint32_t next_id = 1;

std::string const* fileArg(renpy::Signal const& signal)
{
    return signal.args.empty() ? nullptr : std::get_if<std::string>(&signal.args[0]);
}

} // namespace

uint32_t on(std::string name, Handler handler)
{
    entries.push_back({ next_id, std::move(name), std::move(handler) });
    return next_id++;
}

void off(uint32_t id)
{
    std::erase_if(entries, [id](Entry const& e) { return e.id == id; });
}

void clear()
{
    entries.clear();
}

bool dispatch(renpy::Signal const& signal)
{
    // Copied: a handler may register or remove handlers, or stop the dialog
    std::vector<Entry> const snapshot = entries;
    for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it) {
        if (it->name != signal.name && it->name != "*")
            continue;
        // Removed by an earlier handler of this very dispatch
        if (std::ranges::none_of(entries, [&](Entry const& e) { return e.id == it->id; }))
            continue;
        bool handled = false;
        try {
            handled = it->handler(signal);
        }
        catch (std::exception const& e) {
            LOG_CTX_ERR("signals: handler of '" + signal.name + "'", e.what());
        }
        if (handled)
            return true;
    }
    // The ones nothing else is expected to handle are not worth a warning
    if (signal.name != "background")
        LOG_CTX_WRN("signals", "unhandled signal '" + signal.name + "'");
    return false;
}

void installDefaults()
{
    // `play music "file" [loop|noloop]`, `stop music`
    on("music", [](renpy::Signal const& signal) {
        std::string const* const file = fileArg(signal);
        if (!file) {
            SSS::Audio::Mixer::stopMusic();
            return true;
        }
        bool const* const loop = signal.args.size() > 1 ? std::get_if<bool>(&signal.args[1]) : nullptr;
        SSS::Audio::Mixer::playMusic(*file, !loop || *loop);
        return true;
    });
    // `play sound "file"`: one-shots can't be stopped, `stop sound` does nothing
    on("sound", [](renpy::Signal const& signal) {
        if (std::string const* const file = fileArg(signal))
            SSS::Audio::Mixer::playSound(*file);
        return true;
    });
}

} // namespace signals
