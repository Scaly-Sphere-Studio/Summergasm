#pragma once
// What the dialog knows of each character beyond the Ren'Py script:
// resources/characters/<canonical-name>.json, with scene IDs mapped in aliases.json:
//   {
//     "name": "Léa",
//     "color": "#ff7eb6",
//     "expressions": {
//       "neutral": { "mouth": [0.52, 0.18], "mouth_dir": [0.6, 0.8],
//                    "tail": "straight", "intensity": 0.0 }
//     }
//   }
// mouth: position in the image, normalized (0..1, origin at the top-left).
// mouth_dir: where the speech bubble tail leaves the mouth, in the image
// (Y-down). Both are given for the image as drawn in the file: mirroring
// (right side) and tilting are applied by the sprite's model matrix.
// tail & intensity: the speech bubble tail of that expression's lines, a say
// line may override them (see renpy::Step::tail). Optional: "straight", 0.
// "name" is for display/debugging; color is the character's dialog name color.
#include "includes.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace dialog {

enum class TailStyle { Straight, Broken };
// "straight" / "broken", nullopt for anything else
std::optional<TailStyle> parseTailStyle(std::string const& name);

struct ExpressionInfo {
    glm::vec2 mouth{ 0.5f, 0.2f };
    glm::vec2 dir{ 0.f, 1.f };      // normalized
    TailStyle tail = TailStyle::Straight;
    float intensity = 0.f;          // 0..1
};

class CharacterData {
public:
    // Resolves an alias and loads its canonical JSON once. Missing or bad
    // character files are warned about once; expression data is then absent.
    // expression: "" or unknown -> "neutral"; no data -> nullopt
    std::optional<ExpressionInfo> get(std::string const& id, std::string const& expression);
    std::optional<uint32_t> color(std::string const& id);
    std::string imagePrefix(std::string const& id);
    // Forgets what was loaded: the files are read again when next asked.
    void reload();

private:
    using Expressions = std::unordered_map<std::string, ExpressionInfo>;
    struct CharacterInfo {
        Expressions expressions;
        std::optional<uint32_t> color;
    };
    std::string canonicalId(std::string const& id);
    std::optional<CharacterInfo> load(std::string const& id);
    std::optional<CharacterInfo> const& info(std::string const& id);

    bool _aliases_loaded = false;
    std::unordered_map<std::string, std::string> _aliases;
    std::unordered_map<std::string, std::string> _image_prefixes;
    std::unordered_map<std::string, std::optional<CharacterInfo>> _chars;
};

} // namespace dialog
