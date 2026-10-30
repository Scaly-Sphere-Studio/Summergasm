# Summergasm

Scenes are Lua scripts (`resources/lua/*.lua`) driving the SSS libraries
(`GL`, `TR`, `Audio`) plus the game's own bindings, listed below.

## Where things are declared

| What | Where | How to add / edit |
|---|---|---|
| Global functions bound from C++ (`load_scene`, `dialog`, `focus`, `play_sound`…) | [src/mylua.cpp](src/mylua.cpp), `setup_lua()` | `lua["name"] = [](...) { ... };` then add a line to `lua_commands_help` (same file, above `mylua_focus`) so `help()` shows its arguments |
| Usertypes & tables bound from C++ (`SceneRenderer`, `PointLight`, `FoldingMap`, `Dialog`, `vec2`, `signals`, `game`) | [src/mylua.cpp](src/mylua.cpp), `setup_lua()` | `usertype["member"] = &Class::member;` |
| Console aliases (`h`, `?`, `ls`, `us`, `rs`, `m`) | [src/imgui/console.cpp](src/imgui/console.cpp), `ConsoleMemory::pushCmd()` | Add the alias to `alias_regex` **and** to the `if / else` chain below it |
| SSS libraries (`GL`, `TR`, `Audio`, `vec3`, `RGBA`…) | [src/setup_lua.cpp](src/setup_lua.cpp) → `SSS::*::lua_setup_*()` | Bound in the SSS repositories (installed through vcpkg) |
| Global Lua helpers | [resources/lua/model_functions.lua](resources/lua/model_functions.lua) | Plain `function name (args)` |
| Startup (windows, cameras, first scene) | [resources/lua/global_setup.lua](resources/lua/global_setup.lua) | Run once by `setup_lua()` |
| Scene-local helpers | The scene's own script, e.g. [menu.lua](resources/lua/menu.lua) | Only visible in that scene's environment |

Conventions:
- Scene names are given without `.lua` (`load_scene("train")`).
- C++-bound functions return `true` on success.
- Loading / unloading scenes, sounds and musics requested while the scenes
  run are deferred to the end of the frame.
- Arguments in `[brackets]` are optional. `[on]` arguments toggle the value
  when omitted, and the function returns the new value.

## Console aliases

Typed in the in-game console. Quotes around the argument are optional.

| Alias | Runs | Description |
|---|---|---|
| `h`, `?`, `help` `[filter]` | `help([filter])` | List commands & Lua functions |
| `ls` | `list_scenes()` | List scenes & which are running |
| `ls <scene>` | `load_scene("<scene>")` | Load a scene |
| `us [scene]` | `unload_scene(["<scene>"])` | Unload a scene, default: the current one |
| `rs [scene]` | `reload_scene(["<scene>"])` | Reload a scene, default: the current one |
| `m`, `menu` | `menu()` | Return to the menu (also the Escape key) |

## Global variables

| Name | Description |
|---|---|
| `dt` | Seconds since the previous frame (capped at 0.1) |
| `time` | Seconds since the first frame |
| `scenes` | Running scenes' environments, by name (`scenes.train`) |
| `window`, `ui_window` | Main & UI windows, created by `global_setup.lua` |
| `filename` | *(per scene)* The scene's script name |
| `is_loading`, `is_running`, `is_unloading` | *(per scene)* Scene state: scripts run every frame, use these to set up and clean up |

## Functions

### Scenes & console

| Function | Arguments | Description |
|---|---|---|
| `help` | `[filter]` | List commands & Lua functions whose name contains `filter` |
| `file_script` | `path` | Run `resources/lua/<path>.lua` once |
| `load_scene` | `scene_name` | Load & run a scene, unloads the menu |
| `unload_scene` | `[scene_name]` | Unload a scene, default: the current one |
| `reload_scene` | `[scene_name]` | Reload a scene if its script compiles, default: the current one |
| `auto_reload` | `[on]` | Reload the current scene when its file changes (if it compiles). On by default |
| `list_scenes` | | List scenes & which are running |
| `list_assets` | `folder` | Sorted file names in `resources/assets/<folder>`, empty if missing |
| `menu` | | Unload every scene & return to the menu |
| `console_set_env` | `scene_name` | Run console commands in a running scene's environment |
| `console_reset_env` | | Run console commands in the global environment |

### Audio

File names are relative to the sounds / musics folders (absolute paths are kept).

| Function | Arguments | Description |
|---|---|---|
| `play_sound` | `file, [volume = 100]` | Play a sound, sounds overlap freely |
| `play_music` | `file, [loop = true], [fade]` | Crossfade to a music, only one plays at a time |
| `stop_music` | `[fade]` | Fade the music out |

### Dialog & display settings

| Function | Arguments | Description |
|---|---|---|
| `dialog` | `name` | Play `resources/dialogs/<name>[.rpy\|.txt]` (or a direct path) over the current scene, replacing the previous one. Returns the `Dialog` |
| `debug_mouths` | `[on]` | Show each character's mouth & direction used by the speech bubble tails. Reloads `resources/characters/*.json` |
| `dialog_tails` | `[on]` | Draw the speech bubble tails. Saved in `settings.ini` |
| `grey_characters` | `[on]` | Draw the characters not talking in black & white. Saved in `settings.ini` |

### Depth of field

Apply to every `SceneRenderer` of the window and print their focus band.
Distances are from the camera.

| Function | Arguments | Description |
|---|---|---|
| `dof` | `[on]` | Toggle depth of field |
| `focus` | `[distance, band_width]` | Sharp band centered on `distance` (width kept if omitted), prints it without argument. `focus(0, 0)`: all blurred |
| `focus_near` | `distance` | Near end of the sharp band (pushes the far one if needed) |
| `focus_far` | `distance` | Far end of the sharp band (pushes the near one if needed). `focus_near(0) focus_far(math.huge)`: all sharp |
| `focus_move` | `delta` | Move the sharp band, away from the camera if positive |

### Game state (`game`)

Values kept for the whole game and saved with it: strings, integers or booleans.

| Function / field | Arguments | Description |
|---|---|---|
| `game.vars` | | Read & write values by name (`game.vars.met_alex = true`), `nil` erases one, `pairs(game.vars)` iterates on a copy |
| `game.save` | `[name = "save"]` | Save the values in `saves/<name>.json`. `false` on error |
| `game.load` | `[name = "save"]` | Load the values of `saves/<name>.json`. `false` on error or missing save |
| `game.reset` | | Erase every value |

### Signals (`signals`)

Signals sent by the Ren'Py dialog scripts (`background`, `music`, `sound`, or any name).
Handlers run from the latest registered to the first, until one returns `true`.

| Function | Arguments | Description |
|---|---|---|
| `signals.on` | `name, handler` | Register `handler(name, ...)` for a signal (`"*"`: every one). Returns an id. A scene must remove its handlers when it unloads |
| `signals.off` | `id` | Remove a handler |

### `Dialog`

Ren'Py dialog node, see [inc/Dialog.hpp](inc/Dialog.hpp) & [dialog.lua](resources/lua/dialog.lua).
Started dialogs are updated by the main loop.

| Member | Arguments | Description |
|---|---|---|
| `Dialog.new` | `[name]` | New node, starts the conversation at once if given a name |
| `:start` | `name` | Start a conversation (`.rpy` / `.txt`), images relative to `resources/assets/` |
| `:stop` | | End the conversation & destroy its UI, settings kept |
| `:hide` / `:show` | | Stop / resume drawing, the story waits while hidden |
| `.active` | *read-only* | A conversation is loaded, hidden or not |
| `.hidden` | *read-only* | Hidden by `hide()` |
| `.visible` | *read-only* | Active and not hidden: it takes the inputs |
| `.log_open` | *read-only* | The log is open |
| `.end_text` | string | Shown on the story's End step (TR markup) until confirmed. Empty: finishes on the last line |
| `.show_characters`, `.show_box`, `.show_choices`, `.show_controls` | bool | Show each part of the UI |
| `.background_image` | *read-only* | Current image path, or `nil` |
| `.background_color` | *read-only* | Current `0xRRGGBB` color, or `nil` |
| `.on_finished` | *write-only* `function ()` or `nil` | Called once the conversation is over |
| `.on_signal` | *write-only* `function (name, ...)` or `nil` | This dialog's own signal handler, called before `signals`'. Returns `true` when handled |

### `SceneRenderer`

Whole 3D scene of planes seen through a perspective camera, with scroll
layers, point lights, shadows and depth of field. See
[inc/SceneRenderer.hpp](inc/SceneRenderer.hpp). Also has every `GL.PlaneRenderer` member.
Colors are sRGB `vec3`.

| Member | Arguments | Description |
|---|---|---|
| `SceneRenderer.new` | `[camera]` | New renderer |
| `:addPlane` | `plane` | Add a plane |
| `:removePlane` | `plane` | Remove a plane, and forget its settings below |
| `:addScrollLayer` | `{ planes }` | Add the planes as a scroll layer: tiles lined up along x from x = 0, scrolling & wrapping together |
| `:clearScrollLayers` | | Stop scrolling all layers (their planes stay) |
| `.scroll_layer_count` | *read-only* | Number of scroll layers |
| `.scroll_speed` | number | World units per second, the same for every layer |
| `:play` / `:pause` / `:toggle` | | Scrolling |
| `.is_playing` | *read-only* | Scrolling is playing |
| `.depth_sort` | bool | Sort planes back to front before each render |
| `.lighting` | bool | Off: unlit |
| `.opaque_cutoff` | number | Texels at least this opaque are drawn in the opaque pass |
| `.ambient` | `vec3` | White: planes look unlit when there is no light |
| `.lights` | | The `PointLight`s |
| `:addLight` / `:removeLight` | `light` | Add / remove a `PointLight` |
| `.shadows` | bool | Off: no light casts shadows |
| `.shadow_map_size` | int | Each light's shadow map size, in texels |
| `.shadow_fit_margin` | number | Shadow maps cover the camera's view enlarged by this ratio |
| `.depth_of_field` | bool | Blur planes outside the focus band (lit planes only) |
| `.focus_blur` | number | Blur at infinity behind the band, in pixels |
| `.focus_max_blur` | number | Maximum blur in front of the band, in pixels |
| `.focus_transition` | number | Seconds for the band to move to a new one |
| `:setFocus` | `distance, band_width` | Band centered on `distance` |
| `:moveFocus` | `delta` | Move the band, away from the camera if positive |
| `:setFocusNear` / `:setFocusFar` | `distance` | Set one end, pushing the other if needed |
| `.focus_near`, `.focus_far`, `.focus_distance`, `.focus_band_width` | *read-only* | Band set |
| `:setBlurClamp` / `:getBlurClamp` | `plane, [clamp]` | Keep the blur inside the plane (default: scroll layer tiles only) |
| `.normal_map_y_down` | bool | Normal maps are green down (default: OpenGL, up) |
| `:setNormalMap` / `:getNormalMap` | `plane, [texture \| path \| nil]` | Plane's normal map |
| `:setLightingFactor` / `:getLightingFactor` | `plane, [factor]` | 0: unlit (e.g. a sky), 1: fully lit (default) |
| `:setCastShadow` / `:getCastShadow` | `plane, [cast]` | The plane casts shadows (default: true) |
| `:setShadowTransmission` / `:getShadowTransmission` | `plane, [transmission]` | Light going through, tinted: 0 opaque (default), 1 colored glass |
| `:setEmissiveMap` / `:getEmissiveMap` | `plane, [texture \| path \| nil]` | Added after lighting, not shadowed |
| `:setEmissiveColor` / `:getEmissiveColor` | `plane, [color]` | White by default |
| `:setEmissiveIntensity` / `:getEmissiveIntensity` | `plane, [intensity]` | 1 by default |
| `:addGradient` | `{ { t, 0xRRGGBB \| vec3, [alpha], [ease] }, ... }` | New gradient map (a row of the 256x256 atlas, sRGB), returns its index (-1: full). `ease`: `"Linear"`, `"SineEaseIn"`, `"QuadEaseInOut"`... from that stop to the next |
| `:setGradientStops` | `index, stops` | Replace a gradient |
| `:clearGradients` | | Remove every gradient |
| `.gradient_count` | *read-only* | Number of gradients |
| `:setGradient` / `:getGradient` | `plane, [index \| nil]` | The texture's luminance (0 black .. 1 white) picks the gradient's color, alpha multiplied. -1: none (default) |
| `:setCausticsMap` / `:getCausticsMap` | `plane, [texture \| path \| nil]` | Tiled caustics added after lighting, two scrolling copies combined. Set to repeat |
| `:setCausticsColor` / `:getCausticsColor` | `plane, [color]` | White by default |
| `:setCausticsIntensity` / `:getCausticsIntensity` | `plane, [intensity]` | 1 by default |
| `:setCausticsTile` / `:getCausticsTile` | `plane, [tile]` | World size of a tile, 100 by default |
| `:setCausticsWaterline` / `:getCausticsWaterline` | `plane, [water_y, fade]` | Full at the waterline (world y, 0), fading out over `fade` world units above it (100), none below |
| `.caustics_speed` | number | Tiles per second, for every plane |
| `:addMesh` / `:removeMesh` | `mesh` | Add / remove a `FoldingMap`: opaque, lit only, casts shadows |

### `FoldingMap`

Paper map mesh, 4 columns by 2 rows, folded like a city map: accordion folded
along its vertical lines, then in half along the center line (top half behind,
around a rounded crease). Column 0 stays in place: folded, the bottom left
panel's front is on top. Lies in its local xy plane, centered on the origin,
front toward +z. See [inc/FoldingMap.hpp](inc/FoldingMap.hpp). Also has
`.translation`, `.rotation`, `.scaling` (as planes).

| Member | Arguments | Description |
|---|---|---|
| `FoldingMap.new` | `front, back, width, height` | Textures (`back` can be `nil`: the front one, mirrored) or image paths in the assets folder |
| `.half_fold` | number | 0 flat .. 1 folded in half. Fold it only with the accordion closed |
| `.accordion` | number | 0 flat .. 1 closed |
| `.thickness` | number | Distance between the folded layers (1.5) |
| `.crease_radius` | number | Innermost layer's radius around the crease (3) |
| `.width`, `.height` | *read-only* | Local size |
| `.front`, `.back` | `GL.Texture` | Back faces show the back texture mirrored along x (the sheet turned over) |
| `.hidden`, `.cast_shadow` | bool | |
| `:getBounds` | | `min, max` (`vec3`): local bounding box of the current shape |

### `map_overlay`

City map shown over the current scene, like the dialog (see
[inc/MapOverlay.hpp](inc/MapOverlay.hpp)): a `FoldingMap` of
`assets/map/map_front.png` & `map_back.png`, 2D (orthographic, unlit, with a
faux perspective pinching the panels folding away). The map is turned 180
degrees in its plane: the folded map shows its blue cover centered on screen,
then unfolds from behind it, away from the viewer, until the map's front fills
the screen (edges cropped), the cover at the top right; the scenes are
blurred behind it. **M** opens it (not while a dialog is shown) or
closes it, **Escape** closes it, and a dialog showing up closes it.

| Function | Description |
|---|---|
| `map_overlay.open()` / `.close()` / `.toggle()` | Open (ignored while a dialog is shown) / fold back & close |
| `map_overlay.hide()` | Remove at once, without folding back |
| `map_overlay.is_open()` | Opening or open |
| `map_overlay.is_shown()` | Drawn: opening, open or closing |

### `PointLight`

Created with `PointLight.new()`, added with `SceneRenderer:addLight()`. Distances in world units.

| Field | Type | Description |
|---|---|---|
| `position` | `vec3` | |
| `color` | `vec3` | sRGB |
| `intensity` | number | |
| `radius` | number | Distance at which the light reaches 0 |
| `falloff` | number | Attenuation: `(1 - distance / radius) ^ falloff` |
| `terminator_color` | `vec3` | Color at the terminator (fake subsurface scattering) |
| `terminator_width` | number | In N.L units, 0 disables the terminator color & wrap |
| `shadow_color` | `vec3` | Fills the unlit part of the light's area |
| `enabled` | bool | |
| `follow` | plane or `nil` | The light moves with this plane |
| `follow_offset` | `vec3` | Offset from the followed plane |
| `cast_shadows` | bool | |
| `shadow_direction` | `vec3` | Shadow cone direction when the camera isn't on one side of the light |
| `shadow_fov` | number | Maximum cone angle, in degrees |
| `shadow_bias` | number | Against self-shadowing (planes 1 unit apart need < 1) |
| `shadow_softness` | number | Minimum penumbra size, in shadow map texels |
| `shadow_ambient` | number | Share of the ambient light hidden by the cast shadows |
| `size` | number | Radius of the light source (0: point), widens penumbras |

### `vec2`

| Member | Arguments | Description |
|---|---|---|
| `vec2.new` | `[x[, y]]` | 2D vector (e.g. `Camera:rotate`'s pitch & yaw, in degrees) |
| `.x`, `.y` | number | |

### Lua helpers ([model_functions.lua](resources/lua/model_functions.lua))

| Function | Arguments | Description |
|---|---|---|
| `audio_plane` | `plane` | Left click: play / pause audio source 0, right click: restart it |
| `scale_when_loaded` | `plane, scaling` | Apply `scaling` (a `vec3`, or `function (plane, w, h)`) once the plane's image is loaded |
| `apply_loaded_scalings` | | Call every frame to apply the pending `scale_when_loaded` |
| `drag_plane` | `plane` | Drag a held plane with the mouse (normalized coordinates) |
| `drag_plane_fixed` | `plane` | Drag a held plane with the mouse (pixel coordinates) |
| `perspective_distance` | `camera, win_h` | Distance at which a perspective camera sees 1 world unit as 1 pixel on z = 0 |
| `depth_scale` | `D, z` | Scale keeping a plane at depth `z` the same pixel size, from distance `D` |
| `move_camera` | `camera, speed` | Move the camera with the arrow keys |
| `zoom_camera` | `camera, zoom` | Zoom with keypad + / - |

### Boats ([boat_factory.lua](resources/lua/boat_factory.lua))

Sailboats assembled from greyscale parts in `resources/assets/boat/parts/`:
`boat_<i>.png` (hull), `mat_<i>.png` (mast), `toile_<i>.png` (sail), all on the
same canvas. Any index of each part combines with the others, tinted by the
gradients of `boat_palettes`.

| Function / value | Arguments | Description |
|---|---|---|
| `boat_parts` | `[refresh]` | Indices found per part (`{ boat = { 1, 2 }, ... }`), scanned once or again if `refresh` |
| `boat_palettes` | | Gradient stops per part (`boat`, `mat`, `toile`), placed on the parts' grey levels |
| `register_boat_palettes` | `renderer` | Add every palette to a `SceneRenderer` (done by `create_boat` if needed) |
| `create_boat` | `renderer, [{ x, z, height, mirror, boat, mat, toile, palettes }]` | New random boat, waterline at y = 0. Returns `{ planes, hull, x, z, height... }` |
| `update_boat` | `boat, t` | Bobbing & roll, `t` in seconds |
| `remove_boat` | `renderer, boat` | Remove its planes |
| `boat_waterline` | | Waterline, as a share of the canvas height from its bottom |

### Scene-local helpers & callbacks

| Function | Arguments | Defined in | Description |
|---|---|---|---|
| `on_escape` | | any scene | Optional: called on Escape for the current scene, return `true` if handled (else returns to the menu) |
| `tr_color` | `rgb` | [menu.lua](resources/lua/menu.lua) | Convert `0xRRGGBB` to TR's `0xBBGGRR` |
| `colored` | `text, rgb` | [menu.lua](resources/lua/menu.lua) | Prefix `text` with a TR color markup |
| `set_button_style` | `button, style_name` | [menu.lua](resources/lua/menu.lua) | Apply a menu button style (`selected`, `disabled`…) |
| `bg_texture` | `image, color` | [dialog.lua](resources/lua/dialog.lua) | Cached texture of an image path or `0xRRGGBB` color |
| `update_background` | | [dialog.lua](resources/lua/dialog.lua) | Follow the dialog's background, covering the window |
| `update_watermark` | | [global_setup.lua](resources/lua/global_setup.lua) | Keep the watermark pixel-perfect when the window is resized |

## Tip

In the in-game console, `help [filter]` lists the live API (including Lua
functions with their arguments), and Tab autocompletes (e.g. `GL.Plane.`).
