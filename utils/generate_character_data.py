"""Generate missing character expression data from character PNG filenames.

Examples:
    python utils/generate_character_data.py
    python utils/generate_character_data.py path/to/sprites --output-dir path/to/data

Files named ``char1.png`` provide the ``neutral`` expression; files named
``char1_happy.png`` provide the ``happy`` expression. The optional aliases.json
maps those prefixes to canonical JSON filenames, and existing values are kept.
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict

from character_aliases import DEFAULT_ALIAS_FILE, canonical_id, load_aliases


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGE_DIR = PROJECT_ROOT / "resources" / "assets" / "char"
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "resources" / "characters"
FILENAME_PATTERN = re.compile(r"^([^_]+)(?:_(.+))?$")


def discover_expressions(image_dir: Path, aliases: Dict[str, str]) -> Dict[str, Dict[str, Path]]:
    """Return character IDs mapped to their expression images."""
    characters: Dict[str, Dict[str, Path]] = {}
    for image_path in sorted(image_dir.iterdir(), key=lambda path: path.name.casefold()):
        if not image_path.is_file() or image_path.suffix.casefold() != ".png":
            continue

        match = FILENAME_PATTERN.fullmatch(image_path.stem)
        if match is None:
            print("Skipping PNG with unsupported filename: {}".format(image_path.name), file=sys.stderr)
            continue

        image_prefix, expression = match.groups()
        character_id = canonical_id(image_prefix, aliases)
        expression = expression or "neutral"
        if not character_id or not expression:
            print("Skipping PNG with unsupported filename: {}".format(image_path.name), file=sys.stderr)
            continue

        character_expressions = characters.setdefault(character_id, {})
        if expression in character_expressions:
            raise ValueError(
                "Duplicate expression {!r} for {!r}: {} and {}".format(
                    expression, character_id, character_expressions[expression].name, image_path.name
                )
            )
        character_expressions[expression] = image_path
    return characters


def default_expression() -> dict:
    return {
        "mouth": [0.5, 0.25],
        "mouth_dir": [0.0, 1.0],
        "tail": "straight",
        "intensity": 0.0,
    }


def generate_character_data(image_dir: Path, output_dir: Path, alias_file: Path = DEFAULT_ALIAS_FILE) -> int:
    if not image_dir.is_dir():
        raise NotADirectoryError("Image directory does not exist: {}".format(image_dir))

    aliases, _ = load_aliases(alias_file)
    characters = discover_expressions(image_dir, aliases)
    output_dir.mkdir(parents=True, exist_ok=True)
    for character_id in sorted(characters, key=str.casefold):
        json_path = output_dir / (character_id + ".json")
        if json_path.exists():
            with json_path.open(encoding="utf-8") as file:
                data = json.load(file)
            if not isinstance(data, dict):
                raise ValueError("{} must contain a JSON object".format(json_path))
            expressions = data.get("expressions")
            if not isinstance(expressions, dict):
                raise ValueError("{} must contain an expressions object".format(json_path))
        else:
            data = {"name": character_id.capitalize(), "expressions": {}}
            expressions = data["expressions"]

        added = 0
        for expression in sorted(characters[character_id], key=lambda name: (name != "neutral", name.casefold())):
            if expression not in expressions:
                expressions[expression] = default_expression()
                added += 1

        json_path.write_text(
            json.dumps(data, ensure_ascii=False, indent=4) + "\n",
            encoding="utf-8",
        )
        print("{}: {} expression(s) added, {} image(s) found".format(json_path.name, added, len(characters[character_id])))

    if not characters:
        print("No character PNG files found in {}".format(image_dir))
    return len(characters)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "image_dir",
        nargs="?",
        type=Path,
        default=DEFAULT_IMAGE_DIR,
        help="folder containing <character>[_<expression>].png files",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="folder for the generated character JSON files",
    )
    parser.add_argument(
        "--alias-file",
        type=Path,
        default=DEFAULT_ALIAS_FILE,
        help="JSON file mapping image prefixes to canonical character IDs",
    )
    args = parser.parse_args()
    generate_character_data(args.image_dir, args.output_dir, args.alias_file)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
