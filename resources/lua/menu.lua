-- Main menu, loaded by default (see global_setup.lua) and unloaded as soon
-- as another scene is loaded. menu() (or Escape) unloads every scene and
-- comes back here; Escape in the menu quits.
-- One button per scene, the greyed ones are scenes to come.
if (is_loading)
then
    print(filename, "init start")

    -- TR colors are stored as 0xBBGGRR: converts from the usual 0xRRGGBB
    function tr_color (rgb)
        local r = math.floor(rgb / 0x10000) % 0x100
        local g = math.floor(rgb / 0x100) % 0x100
        local b = rgb % 0x100
        return b * 0x10000 + g * 0x100 + r
    end

    function colored (text, rgb)
        return '{{"text_color":' .. tr_color(rgb) .. '}}' .. text
    end

    -- Own camera (pixels, origin at the center, Y up), so scenes moving
    -- camera / cam_fixed don't move the menu
    menu_cam = GL.Camera.new()
    menu_cam.position = vec3.new(0, 0, 3)
    menu_cam.proj_type = GL.Projection.OrthoFixed

    title_area = TR.Area.new(1000, 130)
    do
        local fmt = TR.Fmt.new()
        fmt.charsize = 96
        fmt.alignment = TR.Alignment.Center
        fmt.has_outline = true
        fmt.outline_size = 3
        title_area:setFmt(fmt)
    end
    title_area.string = colored("Summergasm", 0xf2b632)
    title = GL.Plane.new(title_area)
    title.scaling = vec3.new(130, 130, 1)
    title.translation = vec3.new(0, 290, 0)

    footer_area = TR.Area.new(1400, 36)
    do
        local fmt = TR.Fmt.new()
        fmt.charsize = 20
        fmt.alignment = TR.Alignment.Center
        footer_area:setFmt(fmt)
    end
    footer_area.string = colored(
        "Up / Down + Enter or click: play  -  Escape: menu / quit  -  F1: settings  -  F2: Lua console",
        0x9d93bd)
    footer = GL.Plane.new(footer_area)
    footer.scaling = vec3.new(36, 36, 1)

    -- Buttons: scene == nil means not available yet (greyed)
    button_w, button_h = 420, 64
    local button_gap, group_gap = 18, 28
    buttons = {
        { label = "Dialog",   scene = "dialog" },
        { label = "Beach",    scene = "beach" },
        { label = "Train",    scene = "train" },
        { label = "Hands" },
        { label = "Fountain" },
        { label = "Map" },
    }

    -- Text color (0xRRGGBB) & label format per state
    local styles = {
        idle     = { color = 0xffffff, fmt = "%s" },
        selected = { color = 0xf2b632, fmt = "»  %s  «" },
        disabled = { color = 0x6e6a78, fmt = "%s" },
    }

    function set_button_style (button, style_name)
        if (button.style == style_name)
        then
            return
        end
        button.style = style_name
        local style = styles[style_name]
        button.area.string = colored(string.format(style.fmt, button.label), style.color)
    end

    local planes = { title, footer }
    local y = 150
    for i, button in ipairs(buttons) do
        button.enabled = button.scene ~= nil
        if (i > 1 and button.enabled ~= buttons[i - 1].enabled)
        then
            y = y - group_gap   -- the greyed buttons are set apart
        end

        button.area = TR.Area.new(button_w, button_h)
        local fmt = TR.Fmt.new()
        fmt.charsize = 30
        fmt.alignment = TR.Alignment.Center
        button.area:setFmt(fmt)
        button.area:setMargins(14, 0)     -- roughly centers the line vertically

        button.plane = GL.Plane.new(button.area)
        button.plane.hitbox = GL.PlaneHitbox.Full
        button.plane.scaling = vec3.new(button_h, button_h, 1)
        button.plane.translation = vec3.new(0, y, 0)
        if (not button.enabled)
        then
            button.plane.alpha = 0.6
        end
        set_button_style(button, button.enabled and "idle" or "disabled")

        planes[#planes + 1] = button.plane
        y = y - button_h - button_gap
    end

    -- Index of the selected button (keyboard & mouse), among the enabled ones
    selected = 1

    renderer = GL.PlaneRenderer.new(menu_cam, true)
    renderer.planes = planes
    window:addRenderer(renderer)

    last_w, last_h = 0, 0

    print(filename, "init end")

elseif (is_unloading)
then
    window:removeRenderer(renderer)

elseif (is_running)
then
    -- The footer follows the bottom of the window
    local w, h = window:getDimensions()
    if (w ~= last_w or h ~= last_h)
    then
        last_w, last_h = w, h
        footer.translation = vec3.new(0, -h / 2 + 36, 0)
    end

    -- Keyboard: Up / Down skip the greyed buttons
    local function step (dir)
        local i = selected
        repeat
            i = (i - 1 + dir) % #buttons + 1
        until buttons[i].enabled
        selected = i
    end
    if (window:keyIsPressed(GL.KEY_UP))   then step(-1) end
    if (window:keyIsPressed(GL.KEY_DOWN)) then step(1) end

    local launch = window:keyIsPressed(GL.KEY_ENTER) or window:keyIsPressed(GL.KEY_KP_ENTER)
        or window:keyIsPressed(GL.KEY_SPACE)

    -- Mouse: hovering selects, clicking launches
    for i, button in ipairs(buttons) do
        if (button.enabled and button.plane:isHovered())
        then
            selected = i
            if (window:clickIsPressed(GL.LEFT_CLICK))
            then
                launch = true
            end
        end
    end

    for i, button in ipairs(buttons) do
        if (button.enabled)
        then
            set_button_style(button, i == selected and "selected" or "idle")
        end
    end

    -- Loading is deferred until every scene ran, then this menu is unloaded
    if (launch)
    then
        load_scene(buttons[selected].scene)
    end
end
