#pragma once
// Ren'Py script parser + interpreter, standard library only.
//
// Supported subset:
//   define x = Character("Name", color="#rrggbb")
//   image tag attr = "file.png"
//   label name:            jump name            return
//   scene tag attr         scene expression "#rrggbb"
//   show tag attr [at left|center|right]         hide tag
//   "narration"            who "dialogue"       who attr "dialogue"
//   menu:  "choice": <block>
//   default var = "text"   $ var = "text"       $ who.name = "New name"
// Ignored (silently): other default / $ ..., python blocks, with, pause.
//
// On top of Ren'Py, dialogue text accepts three SSS effect markers, converted
// to SSS::TR inline formats by toTRMarkup():
//   **bold**   ~~wave~~   %%shake%%        (escape with \*, \~, \%)
// The name of every Character defined with a color is found in the text at
// runtime and drawn in that color ("Léa" -> pink "Léa"), whole words only.
// Ren'Py's `{color=#rrggbb}...{/color}` colors any text, and `[...]` is
// interpolated at runtime (see Player::interpolate):
//   [who]  Character's current name, in its color     [who.name]  same, uncolored
//   [var]  value of a `default` / `$` string variable

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace renpy {

// ── Scene state ──────────────────────────────────────────────────────────
enum class Pos { Left, Center, Right };

struct Background {
    std::optional<uint32_t> color;  // 0xRRGGBB for `scene expression "#hex"`
    std::string image;              // resolved file path otherwise (empty = black)
};

struct Sprite {
    std::string tag;                // also the Character id, e.g. "lea"
    std::string image;              // resolved file path
    Pos pos = Pos::Center;
};

struct SceneState {
    Background background;
    std::vector<Sprite> sprites;    // draw order
    Sprite const* find(std::string const& tag) const;
};

struct Character {
    std::string id, name;
    std::optional<uint32_t> color;  // 0xRRGGBB
};

// ── Compiled statements ──────────────────────────────────────────────────
struct Statement {
    enum class Kind { Say, Scene, Show, Hide, Menu, Jump, Goto, Return, Set };
    Kind kind;
    int line = 0;                   // source line, for error messages
    std::string who;                // Say: speaker id (empty = narration) / Set: variable
    std::string text;               // Say: raw text / Jump: label / Show,Hide,Scene: tag / Set: value
    std::vector<std::string> attrs; // Show/Scene/Say image attributes
    std::optional<Pos> pos;         // Show `at ...`
    std::optional<uint32_t> color;  // Scene expression "#hex"
    struct Choice { std::string text; size_t target; };
    std::vector<Choice> choices;    // Menu
    size_t target = 0;              // Jump/Goto (resolved index)
};

class Script {
public:
    // Throws std::runtime_error on I/O or syntax errors.
    // Image files are resolved relative to the script's directory.
    static Script load(std::string const& path);

    std::vector<Statement> const& statements() const { return _stmts; }
    size_t entry() const { return _entry; }
    Character const* character(std::string const& id) const;
    std::unordered_map<std::string, Character> const& characters() const { return _characters; }
    // Initial values of the `default var = "text"` variables.
    std::unordered_map<std::string, std::string> const& defaults() const { return _defaults; }
    // Looks up "tag attr..." in `image` statements, else falls back to
    // "<dir>/tag_attr.png" (Ren'Py's file naming convention).
    std::string resolveImage(std::string const& tag, std::vector<std::string> const& attrs) const;

private:
    std::string _dir;
    std::vector<Statement> _stmts;
    std::unordered_map<std::string, Character> _characters;
    std::unordered_map<std::string, std::string> _images;   // "tag attr" -> path
    std::unordered_map<std::string, size_t> _labels;
    std::unordered_map<std::string, std::string> _defaults;
    size_t _entry = 0;

    friend class Compiler;
};

// ── Markup ───────────────────────────────────────────────────────────────
struct MarkupStyle {
    std::string bold   = R"({"font":"arialbd.ttf"})";
    std::string italic = R"({"font":"ariali.ttf"})";
    std::string wave   = R"({"effect":"Waves","effect_offset":4,"effect_speed":50})";
    std::string shake  = R"({"effect":"Vibrate","effect_offset":2})";
};

// What a `[expr]` interpolation turns into.
struct Interpolated {
    std::string text;               // inserted as is, not parsed as markup
    std::optional<uint32_t> color;  // 0xRRGGBB
};
// Returns nullopt for an unknown expression, which is then kept as is.
using Resolver = std::function<std::optional<Interpolated>(std::string const& expr)>;

// A word drawn in its own color wherever it appears, e.g. a Character name.
struct Highlight {
    std::string text;
    uint32_t color;                 // 0xRRGGBB
};

// Converts **, ~~, %% (and Ren'Py's {b} {i} {color=#rrggbb}) into nested
// SSS::TR `{{json}}` blocks. Markers may overlap; unclosed markers are closed
// at the end. `[expr]` is replaced through `resolve` (`[[` is a literal '[').
// Whole-word occurrences of `highlights`, in the text and in uncolored
// interpolated values, are colored; the longest match wins.
std::string toTRMarkup(std::string_view text, MarkupStyle const& style = {},
                       Resolver const& resolve = {},
                       std::vector<Highlight> const& highlights = {});

// Wraps SSS::TR markup in a color block (colors inside it still apply).
std::string colored(std::string_view markup, uint32_t rgb);

// ── Runtime ──────────────────────────────────────────────────────────────
struct Step {
    enum class Kind { Say, Menu, End };
    Kind kind = Kind::End;
    std::string speaker;                // Character id, empty for narration
    std::string speaker_name;           // its current name (may be renamed by `$ who.name = ...`)
    std::optional<uint32_t> speaker_color;
    std::string text;                   // SSS::TR markup, ready for parseString()
    std::vector<std::string> choices;   // SSS::TR markup, Menu only
    SceneState scene;
};

// A line of the log (Player::log()): a said line, or the choice picked in a menu.
struct LogEntry {
    std::string speaker_name;           // empty for narration and choices
    std::optional<uint32_t> speaker_color;
    std::string text;                   // SSS::TR markup
    bool choice = false;
};

// Walks the script. Every produced Step stores its own SceneState and
// variables, so going back is a simple pop of the history.
class Player {
public:
    using Vars = std::unordered_map<std::string, std::string>;

    explicit Player(Script const& script, MarkupStyle style = {});

    Step const& current() const { return _history.back().step; }
    void next();                    // Say -> following step
    void choose(size_t choice);     // Menu -> branch
    bool back();                    // false when already at the first step

    // What has been shown so far, oldest first, the current line included:
    // the lines said, and for each menu passed only the choice that was picked.
    std::vector<LogEntry> log() const;

    // Converts any text to SSS::TR markup with the current variables, e.g. to
    // build UI strings: interpolate("Choose, [l]!").
    std::string interpolate(std::string_view text) const;
    Vars const& vars() const { return _history.back().vars; }

private:
    struct Frame { size_t pc; Step step; Vars vars; std::optional<size_t> chosen; };
    Script const& _script;
    MarkupStyle _style;
    std::vector<Frame> _history;

    void _run(size_t pc, SceneState state, Vars vars);
    std::string _interpolate(std::string_view text, Vars const& vars) const;
    std::string _name(Character const& c, Vars const& vars) const;
};

} // namespace renpy
