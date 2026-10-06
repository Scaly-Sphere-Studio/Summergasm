#include "Settings.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace settings {

static Values values;

Values const& get() noexcept
{
    return values;
}

static fs::path filePath()
{
    return fs::path(g->home_folder) / "settings.ini";
}

void setDialogTails(bool on)
{
    if (values.dialog_tails == on) return;
    values.dialog_tails = on;
    save();
}

void setGreyCharacters(bool on)
{
    if (values.grey_characters == on) return;
    values.grey_characters = on;
    save();
}

namespace {

std::string trim(std::string const& s)
{
    auto const first = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto const last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return first < last ? std::string(first, last) : std::string();
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::optional<bool> parseBool(std::string const& value)
{
    std::string const v = lower(value);
    if (v == "true" || v == "1" || v == "yes" || v == "on")   return true;
    if (v == "false" || v == "0" || v == "no" || v == "off")  return false;
    return std::nullopt;
}

// "section.key" -> setting
bool* find(std::string const& name)
{
    if (name == "dialog.tails")            return &values.dialog_tails;
    if (name == "dialog.grey_characters")  return &values.grey_characters;
    return nullptr;
}

char const* str(bool b) { return b ? "true" : "false"; }

} // namespace

bool load() try
{
    fs::path const path = filePath();
    if (!fs::exists(path))
        return save();
    std::ifstream file(path);
    if (!file)
        SSS::throw_exc("cannot read " + path.string());

    std::string line, section;
    for (int n = 1; std::getline(file, line); ++n) {
        if (auto const c = line.find_first_of(";#"); c != std::string::npos)
            line.erase(c);
        line = trim(line);
        if (line.empty()) continue;
        std::string const where = path.filename().string() + ":" + std::to_string(n);
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        auto const eq = line.find('=');
        if (eq == std::string::npos) {
            LOG_CTX_WRN("settings::load", where + ": expected key = value");
            continue;
        }
        std::string const key = lower(trim(line.substr(0, eq)));
        std::string const name = section.empty() ? key : section + "." + key;
        bool* const setting = find(name);
        if (!setting) {
            LOG_CTX_WRN("settings::load", where + ": unknown setting '" + name + "'");
            continue;
        }
        auto const value = parseBool(trim(line.substr(eq + 1)));
        if (!value) {
            LOG_CTX_WRN("settings::load", where + ": '" + name + "' expects true or false");
            continue;
        }
        *setting = *value;
    }
    return true;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("settings::load", e.what());
    return false;
}

bool save() try
{
    fs::path const path = filePath();
    std::ofstream file(path);
    if (!file)
        SSS::throw_exc("cannot write " + path.string());
    file << "; Summergasm settings\n"
         << "\n"
         << "[dialog]\n"
         << "; Speech bubble tails from the speakers' mouths\n"
         << "tails = " << str(values.dialog_tails) << "\n"
         << "; Characters not talking drawn in black & white\n"
         << "grey_characters = " << str(values.grey_characters) << "\n";
    return true;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("settings::save", e.what());
    return false;
}

} // namespace settings
