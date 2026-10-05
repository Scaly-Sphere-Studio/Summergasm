#pragma once
// Ren'Py script parser + interpreter, standard library only.
//
// Supported subset:
//   define x = Character("Name", color="#rrggbb")
//   image tag attr = "file.png"
//   label name:            jump name            return
//   scene tag attr         scene expression "#rrggbb"
//   show tag attr [at left|center|right]         hide tag
//   swap tag1 tag2         (the two characters exchange their places in line)
//   flip tag left|center|right   (turns around to the back of that side's line)
//   "narration"            who "dialogue"       who attr "dialogue"
//   who1 & who2 "dialogue" (SSS: several characters speaking at once)
//   menu:  "choice": <block>
//   default var = value    $ var = value        $ who.name = "New name"
//   $ var += 1             $ var -= 1           (values: "text", 12, True, False)
//   play music "file" [loop|noloop]   play sound "file"   stop music|sound
//   signal name [args...]  (SSS: any event for the host, see Step::signals)
// Ignored (silently): other default / $ ..., python blocks, with, pause.
// Audio files are resolved relative to the script's audio folder.
//
// On top of Ren'Py, dialogue text accepts three SSS effect markers, converted
// to SSS::TR inline formats by toTRMarkup():
//   **bold**   ~~wave~~   %%shake%%        (escape with \*, \~, \%)
// The name of every Character defined with a color is found in the text at
// runtime and drawn in that color ("Léa" -> pink "Léa"), whole words only.
// Ren'Py's `{color=#rrggbb}...{/color}` colors any text, and `[...]` is
// interpolated at runtime (see Player::interpolate):
//   [who]  Character's current name, in its color     [who.name]  same, uncolored
//   [var]  value of a `default` / `$` variable

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace renpy {

// ── Values ───────────────────────────────────────────────────────────────
// Variables and signal arguments: "text", 12, True / False
using Value = std::variant<std::string, int64_t, bool>;
// As interpolated in the text: booleans are "True" / "False"
std::string toString(Value const& value);

// One-shot event of the script, for the host (see Step::signals):
//   sound       { file } (`play sound`), or {} (`stop sound`)
//   <name>      { args... } (`signal name args...`)
struct Signal {
    std::string name;
    std::vector<Value> args;
};

// ── Scene state ──────────────────────────────────────────────────────────
enum class Pos { Left, Center, Right };

struct Background {
    std::optional<uint32_t> color;  // 0xRRGGBB for `scene expression "#hex"`
    std::string image;              // resolved file path otherwise (empty = black)
    bool operator==(Background const&) const = default;
};

struct Sprite {
    std::string tag;                // also the Character id, e.g. "lea"
    std::string image;              // resolved file path
    Pos pos = Pos::Center;
};

struct Music {
    std::string file;               // resolved file path (empty = no music)
    bool loop = true;
    bool operator==(Music const&) const = default;
};

struct SceneState {
    Background background;
    std::vector<Sprite> sprites;    // draw order
    Music music;                    // `play music`, until `stop music`
    Sprite const* find(std::string const& tag) const;
};

struct Character {
    std::string id, name;
    std::optional<uint32_t> color;  // 0xRRGGBB
};

// ── Compiled statements ──────────────────────────────────────────────────
struct Statement {
    enum class Kind { Say, Scene, Show, Hide, Swap, Flip, Menu, Jump, Goto, Return, Set, Play, Stop, Signal };
    enum class Op { Assign, Add, Sub };
    Kind kind;
    int line = 0;                   // source line, for error messages
    std::string who;                // Say: speaker id (empty = narration) / Set: variable / Play,Stop: channel / Swap: second tag
    std::string text;               // Say: raw text / Jump: label / Show,Hide,Swap,Flip,Scene: tag / Play: file / Signal: name
    Value value;                    // Set
    Op op = Op::Assign;             // Set
    bool loop = false;              // Play
    std::vector<Value> args;        // Signal
    std::vector<std::string> attrs; // Show/Scene/Say image attributes
    std::vector<std::string> others;// Say: the other speakers of `a & b "text"`
    std::optional<Pos> pos;         // Show `at ...` / Flip: destination
    std::optional<uint32_t> color;  // Scene expression "#hex"
    struct Choice { std::string text; size_t target; };
    std::vector<Choice> choices;    // Menu
    size_t target = 0;              // Jump/Goto (resolved index)
};

class Script {
public:
    // Throws std::runtime_error on I/O or syntax errors.
    // Image & audio files are resolved relative to image_dir & audio_dir, by
    // default the script's directory.
    static Script load(std::string const& path, std::string const& image_dir = {},
                       std::string const& audio_dir = {});

    std::vector<Statement> const& statements() const { return _stmts; }
    size_t entry() const { return _entry; }
    Character const* character(std::string const& id) const;
    std::unordered_map<std::string, Character> const& characters() const { return _characters; }
    // Initial values of the `default var = value` variables.
    std::unordered_map<std::string, Value> const& defaults() const { return _defaults; }
    // Looks up "tag attr..." in `image` statements, else falls back to
    // "<dir>/tag_attr.png" (Ren'Py's file naming convention).
    std::string resolveImage(std::string const& tag, std::vector<std::string> const& attrs) const;
    // Audio file of `play`, relative to the audio folder.
    std::string resolveAudio(std::string const& file) const;

private:
    std::string _dir, _audio_dir;
    std::vector<Statement> _stmts;
    std::unordered_map<std::string, Character> _characters;
    std::unordered_map<std::string, std::string> _images;   // "tag attr" -> path
    std::unordered_map<std::string, size_t> _labels;
    std::unordered_map<std::string, Value> _defaults;
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
struct Speaker {
    std::string id;                     // Character id
    std::string name;                   // its current name (may be renamed by `$ who.name = ...`)
    std::optional<uint32_t> color;
};
// "Tom & Léa", each name in its color (SSS::TR markup)
std::string speakersMarkup(std::vector<Speaker> const& speakers);

struct Step {
    enum class Kind { Say, Menu, End };
    Kind kind = Kind::End;
    std::string speaker;                // Character id, empty for narration (the first one when several speak)
    std::string speaker_name;           // its current name
    std::optional<uint32_t> speaker_color;
    std::vector<Speaker> speakers;      // everyone speaking (`a & b "text"`), empty for narration
    std::string speaker_markup;         // their names together, SSS::TR markup
    std::string text;                   // SSS::TR markup, ready for parseString()
    std::vector<std::string> choices;   // SSS::TR markup, Menu only
    SceneState scene;
    // One-shot events run on the way to this step, in script order (after
    // the previous step). The scene is a state: compare it to the previous one.
    std::vector<Signal> signals;
};

// A line of the log (Player::log()): a said line, or the choice picked in a menu.
struct LogEntry {
    std::string speaker_name;           // empty for narration and choices
    std::optional<uint32_t> speaker_color;
    std::string text;                   // SSS::TR markup
    bool choice = false;
    std::string speaker_markup;         // all the speakers' names, colored (SSS::TR markup)
};

// Walks the script. Every produced Step stores its own SceneState and
// variables, so going back is a simple pop of the history.
class Player {
public:
    using Vars = std::unordered_map<std::string, Value>;

    // vars: initial variables (e.g. a saved game). The script's `default`s
    // only set the ones missing.
    explicit Player(Script const& script, MarkupStyle style = {}, Vars vars = {});

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
