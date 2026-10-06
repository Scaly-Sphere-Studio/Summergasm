#pragma once
// What the dialog knows of each character beyond the Ren'Py script: one
// resources/characters/<id>.json per Character id (the sprite tag), e.g. l.json:
//   {
//     "name": "Léa",
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
// "name" is only there for whoever reads the file.
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
    // Loads g->characters_folder/<id>.json the first time an id is asked. A
    // missing or bad file is warned about once, the character then has no data.
    // expression: "" or unknown -> "neutral"; no data -> nullopt
    std::optional<ExpressionInfo> get(std::string const& id, std::string const& expression);
    // Forgets what was loaded: the files are read again when next asked.
    void reload() { _chars.clear(); }

private:
    using Expressions = std::unordered_map<std::string, ExpressionInfo>;
    std::unordered_map<std::string, std::optional<Expressions>> _chars;
};

} // namespace dialog
