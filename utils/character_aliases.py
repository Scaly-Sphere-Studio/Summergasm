"""Read the character alias and image-prefix configuration."""

import json
from pathlib import Path
from typing import Dict, Tuple


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ALIAS_FILE = PROJECT_ROOT / "resources" / "characters" / "aliases.json"


def load_aliases(path: Path = DEFAULT_ALIAS_FILE) -> Tuple[Dict[str, str], Dict[str, str]]:
    if not path.exists():
        return {}, {}

    with path.open(encoding="utf-8") as file:
        config = json.load(file)
    if not isinstance(config, dict):
        raise ValueError("{} must contain a JSON object".format(path))

    aliases = config.get("aliases", {})
    image_prefixes = config.get("image_prefixes", {})
    if not isinstance(aliases, dict) or not isinstance(image_prefixes, dict):
        raise ValueError("{} must contain aliases and image_prefixes objects".format(path))

    for section_name, section in (("aliases", aliases), ("image_prefixes", image_prefixes)):
        if any(
            not isinstance(key, str)
            or not key
            or not isinstance(value, str)
            or not value
            for key, value in section.items()
        ):
            raise ValueError("{} entries in {} must map non-empty strings".format(section_name, path))
    return aliases, image_prefixes


def canonical_id(identifier: str, aliases: Dict[str, str]) -> str:
    return aliases.get(identifier, identifier)
