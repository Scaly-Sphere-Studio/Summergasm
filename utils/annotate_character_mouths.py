"""Interactively annotate mouth positions and speech-tail directions.

Example:
    python utils/annotate_character_mouths.py resources/characters/char1.json

The image folder defaults to ``resources/assets/char``. Drag from the mouth
position in the direction the speech-bubble tail should leave the mouth.
Coordinates saved to JSON are normalized to the original image dimensions.
"""

import argparse
import json
import math
import tkinter as tk
from fractions import Fraction
from pathlib import Path
from tkinter import messagebox
from typing import Any, List, Optional, Tuple


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGE_DIR = PROJECT_ROOT / "resources" / "assets" / "char"


def load_character(json_path: Path, image_dir: Path) -> Tuple[dict, List[Tuple[str, Path]]]:
    with json_path.open(encoding="utf-8") as file:
        data = json.load(file)
    if not isinstance(data, dict) or not isinstance(data.get("expressions"), dict):
        raise ValueError("{} must contain an expressions object".format(json_path))

    character_id = json_path.stem
    expressions = data["expressions"]
    ordered_names = sorted(expressions, key=lambda name: (name != "neutral", name.casefold()))
    images = {path.name.casefold(): path for path in image_dir.iterdir() if path.is_file()}
    entries: List[Tuple[str, Path]] = []

    for name in ordered_names:
        if not isinstance(name, str) or not name:
            raise ValueError("{} contains an invalid expression name".format(json_path))
        image_name = "{}.png".format(character_id) if name == "neutral" else "{}_{}.png".format(character_id, name)
        image_path = images.get(image_name.casefold())
        if image_path is None:
            raise FileNotFoundError(
                "Missing image for expression {!r}: {}".format(name, image_dir / image_name)
            )
        expression_data = expressions[name]
        if not isinstance(expression_data, dict):
            raise ValueError("Expression {!r} in {} must be an object".format(name, json_path))
        entries.append((name, image_path))

    return data, entries


def normalized_pair(value: Any, field: str, expression: str) -> Tuple[float, float]:
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError("{} for {!r} must be a two-number array".format(field, expression))
    try:
        return float(value[0]), float(value[1])
    except (TypeError, ValueError) as error:
        raise ValueError("{} for {!r} must be a two-number array".format(field, expression)) from error


class MouthAnnotator:
    def __init__(self, root: tk.Tk, json_path: Path, image_dir: Path) -> None:
        self.root = root
        self.json_path = json_path
        self.data, self.entries = load_character(json_path, image_dir)
        if not self.entries:
            raise ValueError("{} contains no expressions to annotate".format(json_path))

        self.index = 0
        self.drag_start: Optional[Tuple[float, float]] = None
        self.drag_end: Optional[Tuple[float, float]] = None
        self.image: Optional[tk.PhotoImage] = None
        self.current_image_path: Optional[Path] = None
        self.image_scale = 1.0
        self.image_offset = (0.0, 0.0)
        self.image_width = 0
        self.image_height = 0
        self.original_image_width = 0
        self.original_image_height = 0

        screen_width = root.winfo_screenwidth()
        screen_height = root.winfo_screenheight()
        canvas_width = max(320, min(1000, screen_width - 80))
        canvas_height = max(280, min(760, screen_height - 190))
        root.title("Character mouth annotator")
        root.resizable(False, False)
        root.geometry("{}x{}".format(canvas_width + 24, canvas_height + 100))

        self.status = tk.StringVar()
        tk.Label(root, textvariable=self.status, anchor="w", padx=8, pady=4).pack(fill="x")
        self.canvas = tk.Canvas(
            root,
            width=canvas_width,
            height=canvas_height,
            background="#303030",
            highlightthickness=0,
        )
        self.canvas.pack(padx=12)
        tk.Label(
            root,
            text="Drag mouth to tail direction | Left/Right: previous/next | Enter/Esc: finish",
            anchor="w",
            padx=8,
            pady=5,
        ).pack(fill="x")

        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)
        root.bind("<Left>", self.on_left)
        root.bind("<Right>", self.on_right)
        root.bind("<Return>", self.on_finish)
        root.bind("<Escape>", self.on_finish)
        root.protocol("WM_DELETE_WINDOW", self.finish)
        root.update_idletasks()
        self.render()

    def current_expression(self) -> Tuple[str, dict]:
        name, _ = self.entries[self.index]
        return name, self.data["expressions"][name]

    def save(self) -> bool:
        temporary_path = self.json_path.with_name(self.json_path.name + ".tmp")
        try:
            temporary_path.write_text(
                json.dumps(self.data, ensure_ascii=False, indent=4) + "\n",
                encoding="utf-8",
            )
            temporary_path.replace(self.json_path)
        except OSError as error:
            if temporary_path.exists():
                temporary_path.unlink()
            messagebox.showerror("Could not save character data", str(error), parent=self.root)
            return False
        return True

    def render(self) -> None:
        if self.index >= len(self.entries):
            self.canvas.delete("all")
            self.status.set("All {} expressions saved. Press Enter or Esc to close.".format(len(self.entries)))
            return

        expression, expression_data = self.current_expression()
        _, image_path = self.entries[self.index]
        if image_path != self.current_image_path:
            source = tk.PhotoImage(file=str(image_path))
            self.original_image_width = source.width()
            self.original_image_height = source.height()
            max_width = max(1, self.canvas.winfo_width())
            max_height = max(1, self.canvas.winfo_height())
            scale = min(1.0, max_width / source.width(), max_height / source.height())
            ratio = Fraction(scale).limit_denominator(256)
            if ratio.numerator == 0:
                ratio = Fraction(1, math.ceil(1.0 / scale))
            self.image = source.zoom(ratio.numerator).subsample(ratio.denominator)
            self.image_scale = ratio.numerator / ratio.denominator
            self.image_width = self.image.width()
            self.image_height = self.image.height()
            self.image_offset = (
                max(0.0, (self.canvas.winfo_width() - self.image_width) / 2),
                max(0.0, (self.canvas.winfo_height() - self.image_height) / 2),
            )
            self.current_image_path = image_path
        self.canvas.delete("all")
        if self.image is not None:
            self.canvas.create_image(*self.image_offset, image=self.image, anchor="nw")
        self.draw_annotation(expression_data)
        character_name = self.data.get("name", self.json_path.stem)
        self.status.set(
            "{} - {} ({}/{}) - {}".format(
                character_name, expression, self.index + 1, len(self.entries), image_path.name
            )
        )

    def draw_annotation(self, expression_data: dict) -> None:
        expression, _ = self.current_expression()
        mouth_x, mouth_y = normalized_pair(expression_data.get("mouth", [0.5, 0.25]), "mouth", expression)
        direction_x, direction_y = normalized_pair(
            expression_data.get("mouth_dir", [0.0, 1.0]), "mouth_dir", expression
        )
        x_offset, y_offset = self.image_offset
        mouth_x = x_offset + mouth_x * self.image_width
        mouth_y = y_offset + mouth_y * self.image_height

        if self.drag_start is not None:
            start_x, start_y = self.image_to_canvas(self.drag_start)
            end_x, end_y = self.image_to_canvas(self.drag_end)
        else:
            direction_length = math.hypot(direction_x, direction_y)
            if direction_length > 0:
                direction_x /= direction_length
                direction_y /= direction_length
            start_x, start_y = mouth_x, mouth_y
            end_x, end_y = mouth_x + direction_x * 42, mouth_y + direction_y * 42

        self.canvas.create_line(start_x, start_y, end_x, end_y, fill="#00e5ff", width=3, arrow=tk.LAST)
        self.canvas.create_oval(mouth_x - 6, mouth_y - 6, mouth_x + 6, mouth_y + 6, outline="#ff3030", width=3)
        if self.drag_start is not None:
            self.canvas.create_oval(start_x - 5, start_y - 5, start_x + 5, start_y + 5, fill="#ffff00", outline="")

    def canvas_to_image(self, event: tk.Event) -> Tuple[float, float]:
        x = (self.canvas.canvasx(event.x) - self.image_offset[0]) / self.image_scale
        y = (self.canvas.canvasy(event.y) - self.image_offset[1]) / self.image_scale
        return (
            min(max(x, 0.0), self.entries_image_width()),
            min(max(y, 0.0), self.entries_image_height()),
        )

    def image_to_canvas(self, point: Tuple[float, float]) -> Tuple[float, float]:
        return (
            self.image_offset[0] + point[0] * self.image_scale,
            self.image_offset[1] + point[1] * self.image_scale,
        )

    def entries_image_width(self) -> float:
        return float(self.original_image_width)

    def entries_image_height(self) -> float:
        return float(self.original_image_height)

    def on_press(self, event: tk.Event) -> None:
        if self.index >= len(self.entries):
            return
        self.drag_start = self.canvas_to_image(event)
        self.drag_end = self.drag_start
        self.render()

    def on_drag(self, event: tk.Event) -> None:
        if self.drag_start is None:
            return
        self.drag_end = self.canvas_to_image(event)
        self.render()

    def on_release(self, event: tk.Event) -> None:
        if self.drag_start is None:
            return
        self.drag_end = self.canvas_to_image(event)
        start_x, start_y = self.drag_start
        end_x, end_y = self.drag_end
        direction_x, direction_y = end_x - start_x, end_y - start_y
        direction_length = math.hypot(direction_x, direction_y)
        _, expression_data = self.current_expression()
        expression_data["mouth"] = [
            round(start_x / self.entries_image_width(), 6),
            round(start_y / self.entries_image_height(), 6),
        ]
        if direction_length > 1e-6:
            expression_data["mouth_dir"] = [
                round(direction_x / direction_length, 6),
                round(direction_y / direction_length, 6),
            ]
        self.drag_start = None
        self.drag_end = None
        if self.save():
            self.index += 1
            self.render()
        else:
            self.render()

    def on_left(self, _event: tk.Event) -> str:
        del _event
        if self.index > 0:
            self.index -= 1
            self.drag_start = None
            self.drag_end = None
            self.render()
        return "break"

    def on_right(self, _event: tk.Event) -> str:
        del _event
        if self.index < len(self.entries):
            if self.save():
                self.index += 1
                self.drag_start = None
                self.drag_end = None
                self.render()
        return "break"

    def on_finish(self, _event: tk.Event) -> str:
        del _event
        self.finish()
        return "break"

    def finish(self) -> None:
        if self.save():
            self.root.destroy()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("json_file", type=Path, help="character JSON file to annotate")
    parser.add_argument(
        "--image-dir",
        type=Path,
        default=DEFAULT_IMAGE_DIR,
        help="folder containing the matching character PNG files",
    )
    args = parser.parse_args()
    if not args.image_dir.is_dir():
        parser.error("image directory does not exist: {}".format(args.image_dir))
    if not args.json_file.is_file():
        parser.error("character JSON file does not exist: {}".format(args.json_file))

    root = tk.Tk()
    MouthAnnotator(root, args.json_file, args.image_dir)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
