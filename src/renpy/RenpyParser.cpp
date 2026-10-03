#include "RenpyParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace renpy {

namespace {

// ── Source lines ─────────────────────────────────────────────────────────
struct SrcLine {
    int indent = 0;
    int line = 0;
    std::string text;
};

[[noreturn]] void fail(int line, std::string const& msg)
{
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Removes a `#` comment, ignoring any `#` inside a quoted string.
std::string stripComment(std::string const& s)
{
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char const c = s[i];
        if (quote) {
            if (c == '\\') ++i;
            else if (c == quote) quote = 0;
        }
        else if (c == '"' || c == '\'') quote = c;
        else if (c == '#') return s.substr(0, i);
    }
    return s;
}

std::vector<SrcLine> readLines(std::string const& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot open " + path);
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    if (content.starts_with("\xEF\xBB\xBF"))    // UTF-8 BOM
        content.erase(0, 3);

    std::vector<SrcLine> lines;
    std::istringstream in(content);
    std::string raw;
    for (int n = 1; std::getline(in, raw); ++n) {
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        std::string text = stripComment(raw);
        int indent = 0;
        size_t i = 0;
        for (; i < text.size() && (text[i] == ' ' || text[i] == '\t'); ++i)
            indent += text[i] == '\t' ? 4 : 1;
        text = text.substr(i);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
            text.pop_back();
        if (!text.empty())
            lines.push_back({ indent, n, std::move(text) });
    }
    return lines;
}

// ── Tokens ───────────────────────────────────────────────────────────────
struct Token {
    bool is_string = false;
    std::string value;
    bool is(std::string_view v) const { return !is_string && value == v; }
};

bool isPunct(char c) { return c == ':' || c == '=' || c == '(' || c == ')' || c == ','; }

// Ren'Py string escapes. Unknown escapes (\* \~ \%) are kept for toTRMarkup().
std::string unescape(std::string_view s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 == s.size()) { out += s[i]; continue; }
        char const n = s[++i];
        switch (n) {
            case '"': case '\'': case '\\': out += n; break;
            case 'n': out += '\n'; break;
            default:  out += '\\'; out += n; break;
        }
    }
    return out;
}

std::vector<Token> tokenize(std::string const& s, int line)
{
    std::vector<Token> toks;
    size_t i = 0;
    while (i < s.size()) {
        char const c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < s.size() && s[j] != c) j += s[j] == '\\' ? 2 : 1;
            if (j >= s.size())
                fail(line, "unterminated string");
            toks.push_back({ true, unescape(std::string_view(s).substr(i + 1, j - i - 1)) });
            i = j + 1;
        }
        else if (isPunct(c)) {
            toks.push_back({ false, std::string(1, c) });
            ++i;
        }
        else {
            size_t j = i;
            while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))
                && !isPunct(s[j]) && s[j] != '"' && s[j] != '\'')
                ++j;
            toks.push_back({ false, s.substr(i, j - i) });
            i = j;
        }
    }
    return toks;
}

std::optional<uint32_t> parseHexColor(std::string const& s)
{
    if (s.size() != 7 || s[0] != '#')
        return std::nullopt;
    try { return static_cast<uint32_t>(std::stoul(s.substr(1), nullptr, 16)); }
    catch (...) { return std::nullopt; }
}

std::optional<Pos> parsePos(std::string const& s)
{
    if (s == "left")   return Pos::Left;
    if (s == "center") return Pos::Center;
    if (s == "right")  return Pos::Right;
    return std::nullopt;
}

// "text", 12, -3, True, False
std::optional<Value> parseValue(Token const& t)
{
    if (t.is_string) return t.value;
    if (t.value == "True")  return true;
    if (t.value == "False") return false;
    std::string_view const digits = std::string_view(t.value).substr(t.value.starts_with('-') ? 1 : 0);
    if (!digits.empty() && std::all_of(digits.begin(), digits.end(),
        [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
        try { return int64_t(std::stoll(t.value)); }
        catch (...) {}
    }
    return std::nullopt;
}

} // namespace

std::string toString(Value const& value)
{
    if (auto const* s = std::get_if<std::string>(&value)) return *s;
    if (auto const* b = std::get_if<bool>(&value)) return *b ? "True" : "False";
    return std::to_string(std::get<int64_t>(value));
}

// ── Compiler ─────────────────────────────────────────────────────────────
class Compiler {
public:
    Compiler(Script& script, std::vector<SrcLine> lines) : _s(script), _lines(std::move(lines)) {}

    void run()
    {
        if (!_lines.empty())
            _block(_lines.front().indent);

        for (auto& stmt : _s._stmts) {
            if (stmt.kind != Statement::Kind::Jump) continue;
            auto const it = _s._labels.find(stmt.text);
            if (it == _s._labels.end())
                fail(stmt.line, "unknown label '" + stmt.text + "'");
            stmt.target = it->second;
        }
        if (auto const it = _s._labels.find("start"); it != _s._labels.end())
            _s._entry = it->second;
    }

private:
    Script& _s;
    std::vector<SrcLine> _lines;
    size_t _i = 0;

    using Kind = Statement::Kind;

    Statement& _emit(Kind kind, int line)
    {
        _s._stmts.push_back({ kind, line });
        return _s._stmts.back();
    }

    // Indent of the block opened by the line at _i - 1, or -1 if there is none.
    int _childIndent(SrcLine const& parent) const
    {
        if (_i < _lines.size() && _lines[_i].indent > parent.indent)
            return _lines[_i].indent;
        return -1;
    }

    // `kw var = value` or `kw var += value` (-=), the value being supported
    static bool _assignment(std::vector<Token> const& toks, bool is_default)
    {
        bool const assign  = toks.size() == 4 && toks[2].is("=");
        bool const augment = !is_default && toks.size() == 5 && (toks[2].is("+") || toks[2].is("-"))
                          && toks[3].is("=");
        return (assign || augment) && parseValue(toks.back());
    }

    void _skipChildren(SrcLine const& parent)
    {
        while (_i < _lines.size() && _lines[_i].indent > parent.indent)
            ++_i;
    }

    void _block(int indent)
    {
        while (_i < _lines.size()) {
            SrcLine const& l = _lines[_i];
            if (l.indent < indent) return;
            if (l.indent > indent) fail(l.line, "unexpected indentation");
            ++_i;
            _statement(l);
        }
    }

    void _statement(SrcLine const& l)
    {
        auto const toks = tokenize(l.text, l.line);
        Token const& first = toks.front();
        auto word = [&](size_t k) -> std::string {
            return k < toks.size() && !toks[k].is_string ? toks[k].value : std::string();
        };

        // Say: "text"  |  who [attrs...] "text"
        if (first.is_string) {
            auto& st = _emit(Kind::Say, l.line);
            st.text = first.value;
            return;
        }
        for (size_t k = 1; k < toks.size(); ++k) {
            if (!toks[k].is_string) continue;
            bool const all_words = std::none_of(toks.begin(), toks.begin() + k,
                [](Token const& t) { return t.is_string || isPunct(t.value[0]); });
            if (!all_words || first.value == "define" || first.value == "image"
                || first.value == "scene" || first.value == "show"
                || first.value == "play" || first.value == "signal")
                break;
            auto& st = _emit(Kind::Say, l.line);
            st.who = first.value;
            for (size_t a = 1; a < k; ++a) st.attrs.push_back(toks[a].value);
            st.text = toks[k].value;
            return;
        }

        std::string const& kw = first.value;
        if (kw == "define") {
            // define id = Character("Name", color="#hex", ...)
            if (toks.size() < 6 || !toks[2].is("=") || !toks[3].is("Character")
                || !toks[4].is("(") || !toks[5].is_string)
                return;     // not a Character: ignore
            Character c{ toks[1].value, toks[5].value, std::nullopt };
            for (size_t k = 6; k + 2 < toks.size(); ++k)
                if (toks[k].is("color") && toks[k + 1].is("=") && toks[k + 2].is_string)
                    c.color = parseHexColor(toks[k + 2].value);
            _s._characters[c.id] = c;
        }
        else if (kw == "image") {
            // image tag attr... = "file"
            std::string key;
            size_t k = 1;
            for (; k < toks.size() && !toks[k].is("="); ++k)
                key += (key.empty() ? "" : " ") + toks[k].value;
            if (k + 1 >= toks.size() || !toks[k + 1].is_string)
                fail(l.line, "expected image file");
            _s._images[key] = (std::filesystem::path(_s._dir) / toks[k + 1].value).string();
        }
        else if (kw == "label") {
            std::string const name = word(1);
            if (name.empty()) fail(l.line, "label without a name");
            _s._labels[name] = _s._stmts.size();
            if (int const ci = _childIndent(l); ci >= 0)
                _block(ci);
        }
        else if (kw == "scene") {
            auto& st = _emit(Kind::Scene, l.line);
            if (word(1) == "expression" && toks.size() > 2 && toks[2].is_string) {
                st.color = parseHexColor(toks[2].value);
                if (!st.color) fail(l.line, "only `scene expression \"#rrggbb\"` is supported");
                return;
            }
            st.text = word(1);
            for (size_t k = 2; k < toks.size() && !toks[k].is("with"); ++k)
                st.attrs.push_back(toks[k].value);
        }
        else if (kw == "show") {
            auto& st = _emit(Kind::Show, l.line);
            st.text = word(1);
            if (st.text.empty()) fail(l.line, "show without an image");
            for (size_t k = 2; k < toks.size(); ++k) {
                if (toks[k].is("with")) break;
                if (toks[k].is("at")) {
                    st.pos = parsePos(word(k + 1));
                    if (!st.pos) fail(l.line, "expected left, center or right after `at`");
                    break;
                }
                st.attrs.push_back(toks[k].value);
            }
        }
        else if (kw == "hide") {
            _emit(Kind::Hide, l.line).text = word(1);
        }
        else if (kw == "jump") {
            _emit(Kind::Jump, l.line).text = word(1);
        }
        else if (kw == "return") {
            _emit(Kind::Return, l.line);
        }
        else if (kw == "menu") {
            _menu(l);
        }
        else if (kw == "play" || kw == "stop") {
            // play music|sound "file" [loop|noloop] [fadein x]  |  stop music|sound [fadeout x]
            std::string const channel = word(1);
            if (channel != "music" && channel != "sound")
                fail(l.line, "expected `music` or `sound` after `" + kw + "`");
            auto& st = _emit(kw == "play" ? Kind::Play : Kind::Stop, l.line);
            st.who = channel;
            if (kw == "stop") return;
            if (toks.size() < 3 || !toks[2].is_string)
                fail(l.line, "expected `play " + channel + " \"file\"`");
            st.text = _s.resolveAudio(toks[2].value);
            st.loop = channel == "music";
            for (size_t k = 3; k < toks.size(); ++k) {
                if (toks[k].is("loop"))   st.loop = true;
                if (toks[k].is("noloop")) st.loop = false;
            }
        }
        else if (kw == "signal") {
            // signal name [args...]: any other word is a string argument
            auto& st = _emit(Kind::Signal, l.line);
            st.text = word(1);
            if (st.text.empty()) fail(l.line, "signal without a name");
            for (size_t k = 2; k < toks.size(); ++k)
                st.args.push_back(parseValue(toks[k]).value_or(toks[k].value));
        }
        else if ((kw == "default" || kw == "$") && !toks[1].is_string && _assignment(toks, kw == "default")) {
            // default var = value  |  $ var = value  |  $ var += 1  |  $ who.name = "text"
            auto const value = parseValue(toks.back());
            Statement::Op const op = toks[2].is("+") ? Statement::Op::Add
                                   : toks[2].is("-") ? Statement::Op::Sub : Statement::Op::Assign;
            if (op != Statement::Op::Assign && !std::holds_alternative<int64_t>(*value))
                fail(l.line, "only integers can be added or subtracted");
            if (kw == "default") _s._defaults[toks[1].value] = *value;
            else {
                auto& st = _emit(Kind::Set, l.line);
                st.who = toks[1].value;
                st.value = *value;
                st.op = op;
            }
        }
        else {
            // Other default and $, python:, init:, with, pause, play... have no visual effect here.
            if (kw != "default" && kw != "$" && kw != "with" && kw != "pause")
                std::cerr << "[renpy] line " << l.line << ": ignored `" << kw << "`\n";
            _skipChildren(l);
        }
    }

    void _menu(SrcLine const& l)
    {
        int const ci = _childIndent(l);
        if (ci < 0) fail(l.line, "empty menu");

        size_t const menu_idx = _s._stmts.size();
        _emit(Kind::Menu, l.line);
        std::vector<size_t> exits;

        while (_i < _lines.size() && _lines[_i].indent == ci) {
            SrcLine const& c = _lines[_i++];
            auto const toks = tokenize(c.text, c.line);
            // Menu captions (a say line inside the menu) are not supported.
            if (!toks.front().is_string || !toks.back().is(":"))
                fail(c.line, "expected `\"choice\":` inside menu");
            _s._stmts[menu_idx].choices.push_back({ toks.front().value, _s._stmts.size() });
            if (int const bi = _childIndent(c); bi >= 0)
                _block(bi);
            exits.push_back(_s._stmts.size());
            _emit(Kind::Goto, c.line);
        }
        for (size_t e : exits)
            _s._stmts[e].target = _s._stmts.size();
    }
};

// ── Script ───────────────────────────────────────────────────────────────
Script Script::load(std::string const& path, std::string const& image_dir, std::string const& audio_dir)
{
    Script s;
    std::string const script_dir = std::filesystem::path(path).parent_path().string();
    s._dir = image_dir.empty() ? script_dir : image_dir;
    s._audio_dir = audio_dir.empty() ? script_dir : audio_dir;
    Compiler(s, readLines(path)).run();
    return s;
}

std::string Script::resolveAudio(std::string const& file) const
{
    return (std::filesystem::path(_audio_dir) / file).string();
}

Character const* Script::character(std::string const& id) const
{
    auto const it = _characters.find(id);
    return it == _characters.end() ? nullptr : &it->second;
}

std::string Script::resolveImage(std::string const& tag, std::vector<std::string> const& attrs) const
{
    std::string key = tag;
    for (auto const& a : attrs) key += " " + a;
    if (auto const it = _images.find(key); it != _images.end())
        return it->second;
    // `show eileen` with only `image eileen happy` defined picks that one.
    if (attrs.empty())
        for (auto const& [k, file] : _images)
            if (k.starts_with(tag + " "))
                return file;
    std::string file = key;
    std::replace(file.begin(), file.end(), ' ', '_');
    return (std::filesystem::path(_dir) / (file + ".png")).string();
}

Sprite const* SceneState::find(std::string const& tag) const
{
    auto const it = std::find_if(sprites.begin(), sprites.end(),
        [&](Sprite const& s) { return s.tag == tag; });
    return it == sprites.end() ? nullptr : &*it;
}

// ── Player ───────────────────────────────────────────────────────────────
Player::Player(Script const& script, MarkupStyle style, Vars vars) : _script(script), _style(std::move(style))
{
    for (auto const& [name, value] : script.defaults())
        vars.try_emplace(name, value);
    _run(script.entry(), {}, std::move(vars));
}

void Player::next()
{
    Frame const& f = _history.back();
    if (f.step.kind == Step::Kind::Say)
        _run(f.pc + 1, f.step.scene, f.vars);
}

void Player::choose(size_t choice)
{
    Frame& f = _history.back();
    if (f.step.kind != Step::Kind::Menu)
        return;
    auto const& choices = _script.statements()[f.pc].choices;
    if (choice < choices.size()) {
        f.chosen = choice;          // set before _run(), which may reallocate _history
        _run(choices[choice].target, f.step.scene, f.vars);
    }
}

bool Player::back()
{
    if (_history.size() <= 1)
        return false;
    _history.pop_back();
    _history.back().chosen.reset(); // back on a menu: its choice is undone
    return true;
}

std::vector<LogEntry> Player::log() const
{
    std::vector<LogEntry> entries;
    for (auto const& f : _history) {
        Step const& s = f.step;
        if (s.kind == Step::Kind::End)
            continue;
        if (!s.text.empty())        // a menu without a question has no text
            entries.push_back({ s.speaker.empty() ? "" : s.speaker_name, s.speaker_color, s.text });
        if (f.chosen && *f.chosen < s.choices.size())
            entries.push_back({ "", std::nullopt, s.choices[*f.chosen], true });
    }
    return entries;
}

std::string Player::interpolate(std::string_view text) const
{
    return _interpolate(text, vars());
}

std::string Player::_name(Character const& c, Vars const& vars) const
{
    auto const it = vars.find(c.id + ".name");
    return it == vars.end() ? c.name : toString(it->second);
}

std::string Player::_interpolate(std::string_view text, Vars const& vars) const
{
    // Character names are colored wherever they appear, using their current name.
    std::vector<Highlight> names;
    for (auto const& [id, c] : _script.characters())
        if (c.color) names.push_back({ _name(c, vars), *c.color });

    return toTRMarkup(text, _style, [&](std::string const& expr) -> std::optional<Interpolated> {
        // [who] -> colored name, [who.name] -> plain name, [var] -> value
        bool const plain = expr.ends_with(".name");
        if (auto const* c = _script.character(plain ? expr.substr(0, expr.size() - 5) : expr))
            return Interpolated{ _name(*c, vars), plain ? std::nullopt : c->color };
        if (auto const it = vars.find(expr); it != vars.end())
            return Interpolated{ toString(it->second), std::nullopt };
        return std::nullopt;
    }, names);
}

void Player::_run(size_t pc, SceneState state, Vars vars)
{
    auto const& stmts = _script.statements();
    using Kind = Statement::Kind;

    // Statement run after `from`, following jumps (they change nothing).
    auto const following = [&](size_t from) {
        size_t next = from + 1;
        for (int guard = 0; next < stmts.size() && guard < 1000
             && (stmts[next].kind == Kind::Jump || stmts[next].kind == Kind::Goto); ++guard)
            next = stmts[next].target;
        return next;
    };
    // A line said right before a menu is its question: it is shown with the
    // choices, as a single step (it is not typed twice).
    std::optional<Step> question;
    std::vector<Signal> signals;

    // Guards against `label a: jump a` style loops without any dialogue.
    for (int guard = 0; pc < stmts.size() && guard < 100000; ++guard) {
        Statement const& st = stmts[pc];
        switch (st.kind) {
        case Kind::Say: {
            // `who attr "text"` also changes the speaker's shown image.
            if (!st.who.empty() && !st.attrs.empty())
                for (auto& sp : state.sprites)
                    if (sp.tag == st.who) sp.image = _script.resolveImage(st.who, st.attrs);
            Step step;
            step.kind = Step::Kind::Say;
            step.speaker = st.who;
            if (auto const* c = _script.character(st.who)) {
                step.speaker_name = _name(*c, vars);
                step.speaker_color = c->color;
            }
            else {
                step.speaker_name = st.who;
            }
            step.text = _interpolate(st.text, vars);
            if (size_t const next = following(pc); next < stmts.size() && stmts[next].kind == Kind::Menu) {
                question = std::move(step);
                pc = next;
                break;
            }
            step.scene = std::move(state);
            step.signals = std::move(signals);
            _history.push_back({ pc, std::move(step), std::move(vars) });
            return;
        }
        case Kind::Menu: {
            Step step = question ? std::move(*question) : Step{};
            step.kind = Step::Kind::Menu;
            for (auto const& c : st.choices)
                step.choices.push_back(_interpolate(c.text, vars));
            step.scene = std::move(state);
            step.signals = std::move(signals);
            _history.push_back({ pc, std::move(step), std::move(vars) });
            return;
        }
        case Kind::Scene:
            state.sprites.clear();
            state.background = {};
            if (st.color) state.background.color = st.color;
            else if (!st.text.empty()) state.background.image = _script.resolveImage(st.text, st.attrs);
            ++pc;
            break;
        case Kind::Show: {
            auto it = std::find_if(state.sprites.begin(), state.sprites.end(),
                [&](Sprite const& s) { return s.tag == st.text; });
            if (it == state.sprites.end()) {
                state.sprites.push_back({ st.text, _script.resolveImage(st.text, st.attrs), Pos::Center });
                it = std::prev(state.sprites.end());
            }
            else if (!st.attrs.empty()) {
                it->image = _script.resolveImage(st.text, st.attrs);
            }
            if (st.pos) it->pos = *st.pos;
            ++pc;
            break;
        }
        case Kind::Hide:
            std::erase_if(state.sprites, [&](Sprite const& s) { return s.tag == st.text; });
            ++pc;
            break;
        case Kind::Jump:
        case Kind::Goto:
            pc = st.target;
            break;
        case Kind::Return:
            pc = stmts.size();
            break;
        case Kind::Set: {
            if (st.op == Statement::Op::Assign) {
                vars[st.who] = st.value;
            }
            else {
                // A missing (or non integer) variable counts as 0
                Value& var = vars[st.who];
                int64_t const* current = std::get_if<int64_t>(&var);
                if (!current && var != Value())
                    std::cerr << "[renpy] line " << st.line << ": " << st.who << " is not an integer, reset to 0\n";
                int64_t const delta = std::get<int64_t>(st.value);
                var = (current ? *current : 0) + (st.op == Statement::Op::Add ? delta : -delta);
            }
            ++pc;
            break;
        }
        case Kind::Play:
            if (st.who == "music") state.music = { st.text, st.loop };
            else signals.push_back({ "sound", { st.text } });
            ++pc;
            break;
        case Kind::Stop:
            if (st.who == "music") state.music = {};
            else signals.push_back({ "sound", {} });
            ++pc;
            break;
        case Kind::Signal:
            signals.push_back({ st.text, st.args });
            ++pc;
            break;
        }
    }

    Step end;
    end.kind = Step::Kind::End;
    end.scene = std::move(state);
    end.signals = std::move(signals);
    _history.push_back({ pc, std::move(end), std::move(vars) });
}

// ── Markup ───────────────────────────────────────────────────────────────
namespace {

// Flat number form: TR ends a `{{json}}` block at the first "}}", so nested
// objects can't be used. RGB24's packed value is 0xBBGGRR, hence the swap.
std::string colorJson(uint32_t rgb)
{
    uint32_t const bgr = ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF);
    return R"({"text_color":)" + std::to_string(bgr) + "}";
}

// Letters, digits, '_' and any UTF-8 multi-byte part (é, à...) belong to a word.
bool isWordByte(char c)
{
    auto const u = static_cast<unsigned char>(c);
    return u >= 0x80 || std::isalnum(u) || c == '_';
}

} // namespace

std::string colored(std::string_view markup, uint32_t rgb)
{
    return '{' + colorJson(rgb) + '}' + std::string(markup) + "{{}}";
}

std::string toTRMarkup(std::string_view text, MarkupStyle const& style, Resolver const& resolve,
                       std::vector<Highlight> const& highlights)
{
    enum class M { Bold, Italic, Wave, Shake, Color };
    struct Open { M m; std::string json; };

    std::string out;
    std::vector<Open> open; // TR formats are a stack: {{json}} pushes, {{}} pops

    auto const emit = [&](std::string const& json) {
        if (out.ends_with('{')) out += ' ';     // keep a literal '{' from forming "{{"
        out += '{' + json + '}';
    };
    auto const push = [&](M m, std::string json) {
        emit(json);
        open.push_back({ m, std::move(json) });
    };
    auto const find = [&](M m) {    // innermost open m
        auto const it = std::find_if(open.rbegin(), open.rend(), [&](Open const& o) { return o.m == m; });
        return it == open.rend() ? open.end() : std::prev(it.base());
    };
    // Closes `it`, then reopens whatever was opened after it (overlapping markers).
    auto const close = [&](std::vector<Open>::iterator it) {
        std::vector<Open> const reopen(it + 1, open.end());
        for (size_t n = open.end() - it; n > 0; --n) out += "{{}}";
        open.erase(it, open.end());
        for (auto const& r : reopen) push(r.m, r.json);
    };
    auto const toggle = [&](M m, std::string const& json) {
        if (auto const it = find(m); it != open.end()) close(it);
        else push(m, json);
    };
    auto const is_open = [&](M m) { return find(m) != open.end(); };

    // Longest first, so "Marie-Anne" is matched before "Marie".
    std::vector<Highlight const*> words;
    for (auto const& h : highlights)
        if (!h.text.empty()) words.push_back(&h);
    std::sort(words.begin(), words.end(),
        [](Highlight const* a, Highlight const* b) { return a->text.size() > b->text.size(); });

    // Highlight whose whole word starts at s[i], if any.
    auto const match = [&](std::string_view s, size_t i) -> Highlight const* {
        if (i > 0 && isWordByte(s[i - 1]))
            return nullptr;
        for (auto const* h : words) {
            size_t const end = i + h->text.size();
            if (s.substr(i).starts_with(h->text) && (end == s.size() || !isWordByte(s[end])))
                return h;
        }
        return nullptr;
    };
    auto const colored = [&](std::string const& word, uint32_t rgb) {
        emit(colorJson(rgb));
        out += word + "{{}}";
    };
    // Plain text (e.g. an interpolated value), with its highlights colored.
    auto const append = [&](std::string_view s) {
        for (size_t i = 0; i < s.size(); ++i) {
            if (auto const* h = match(s, i)) { colored(h->text, h->color); i += h->text.size() - 1; }
            else out += s[i];
        }
    };

    for (size_t i = 0; i < text.size(); ++i) {
        char const c = text[i];
        char const n = i + 1 < text.size() ? text[i + 1] : '\0';

        if (c == '\\' && (n == '*' || n == '~' || n == '%' || n == '{' || n == '[')) {
            out += n; ++i;
        }
        else if (c == '*' && n == '*') { toggle(M::Bold, style.bold);   ++i; }
        else if (c == '~' && n == '~') { toggle(M::Wave, style.wave);   ++i; }
        else if (c == '%' && n == '%') { toggle(M::Shake, style.shake); ++i; }
        else if (c == '[' && n == '[') { out += '['; ++i; }
        else if (c == '[') {
            // Ren'Py interpolation: [expr], resolved by the caller (see Player::interpolate).
            size_t const end = text.find(']', i);
            auto const value = resolve && end != std::string_view::npos
                ? resolve(std::string(text.substr(i + 1, end - i - 1))) : std::nullopt;
            if (!value) { out += c; continue; }
            if (value->color) colored(value->text, *value->color);
            else append(value->text);
            i = end;
        }
        else if (c == '{' && n == '{') { out += '{'; ++i; }   // Ren'Py literal brace
        else if (c == '{') {
            // Ren'Py text tag: {b} {i} {color=#hex} and their closing tags are mapped,
            // others dropped.
            size_t const end = text.find('}', i);
            if (end == std::string_view::npos) { out += c; continue; }
            std::string_view const tag = text.substr(i + 1, end - i - 1);
            if      (tag == "b"  && !is_open(M::Bold))   toggle(M::Bold, style.bold);
            else if (tag == "/b" &&  is_open(M::Bold))   toggle(M::Bold, style.bold);
            else if (tag == "i"  && !is_open(M::Italic)) toggle(M::Italic, style.italic);
            else if (tag == "/i" &&  is_open(M::Italic)) toggle(M::Italic, style.italic);
            else if (tag.starts_with("color=")) {
                if (auto const rgb = parseHexColor(std::string(tag.substr(6))))
                    push(M::Color, colorJson(*rgb));
            }
            else if (tag == "/color" && is_open(M::Color)) close(find(M::Color));
            i = end;
        }
        else if (auto const* h = match(text, i)) {
            colored(h->text, h->color);
            i += h->text.size() - 1;
        }
        else {
            out += c;
        }
    }
    for (size_t k = open.size(); k > 0; --k) out += "{{}}";
    return out;
}

} // namespace renpy
