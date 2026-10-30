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
    title_w, title_h = 1000, 130

    footer_area = TR.Area.new(1400, 36)
    do
        local fmt = TR.Fmt.new()
        fmt.charsize = 20
        fmt.alignment = TR.Alignment.Center
        footer_area:setFmt(fmt)
    end
    footer_area.string = colored(
        "Arrows + Enter or click: play  -  Escape: menu / quit  -  F1: settings  -  F2: Lua console",
        0x9d93bd)
    footer = GL.Plane.new(footer_area)
    footer_w, footer_h = 1400, 36

    -- Buttons: scene == nil means not available yet (greyed). Laid out by
    -- layout_menu() in as many columns as needed, scaled down (text
    -- included) when they don't fit the window's width.
    button_w, button_h = 420, 64
    button_gap, group_gap, column_gap = 18, 28, 48
    -- Smallest scale of the buttons, whatever the window's size
    min_button_scale = 0.4
    buttons = {
        { label = "Dialog",   scene = "dialog" },
        { label = "Beach",    scene = "beach" },
        { label = "Train",    scene = "train" },
        { label = "Fire pit", scene = "firepit" },
        { label = "Marina",   scene = "boat" },
        { label = "Hands" },
        { label = "Fountain" },
        { label = "Map",      scene = "map" },
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
    for i, button in ipairs(buttons) do
        button.enabled = button.scene ~= nil
        -- The greyed buttons are set apart (unless starting a column)
        button.gap_before = i > 1 and button.enabled ~= buttons[i - 1].enabled

        button.area = TR.Area.new(button_w, button_h)
        local fmt = TR.Fmt.new()
        fmt.charsize = 30
        fmt.alignment = TR.Alignment.Center
        button.area:setFmt(fmt)
        button.area:setMargins(14, 0)     -- roughly centers the line vertically

        button.plane = GL.Plane.new(button.area)
        button.plane.hitbox = GL.PlaneHitbox.Full
        if (not button.enabled)
        then
            button.plane.alpha = 0.6
        end
        set_button_style(button, button.enabled and "idle" or "disabled")

        planes[#planes + 1] = button.plane
    end

    -- Splits the buttons, in order, into columns at most `limit` high (at
    -- scale s). Sets each button's column & y offset from its column's top,
    -- returns the number of columns and the highest one's height.
    local function pack (s, limit)
        local columns, y, tallest = 1, 0, 0
        for i, button in ipairs(buttons) do
            local gap = 0
            if (i > 1)
            then
                gap = (button_gap + (button.gap_before and group_gap or 0)) * s
            end
            if (i > 1 and y + gap + button_h * s > limit)
            then
                columns, y, gap = columns + 1, 0, 0
            end
            button.column = columns
            button.offset = y + gap
            y = y + gap + button_h * s
            tallest = math.max(tallest, y)
        end
        return columns, tallest
    end

    -- Places the title at the top, the footer at the bottom, and the buttons
    -- in the space between: the largest scale (up to 1) at which their
    -- columns fit the width, the columns balanced and the block centered
    function layout_menu (w, h)
        local title_s = math.min(1, (w - 40) / title_w)
        title.scaling = vec3.new(title_h * title_s, title_h * title_s, 1)
        local title_y = h / 2 - 70 * title_s
        title.translation = vec3.new(0, title_y, 0)

        local footer_s = math.min(1, (w - 20) / footer_w)
        footer.scaling = vec3.new(footer_h * footer_s, footer_h * footer_s, 1)
        local footer_y = -h / 2 + footer_h
        footer.translation = vec3.new(0, footer_y, 0)

        local top = title_y - title_h * title_s / 2 - 30
        local bottom = footer_y + footer_h / 2 + 24
        local avail_h = math.max(top - bottom, button_h * min_button_scale)
        local avail_w = math.max(w - 40, button_w * min_button_scale)

        local s = 1
        local columns, height
        while (true)
        do
            columns, height = pack(s, avail_h)
            local width = (columns * button_w + (columns - 1) * column_gap) * s
            if (width <= avail_w or s <= min_button_scale)
            then
                break
            end
            s = math.max(min_button_scale, s - 0.05)
        end
        -- Balanced: the lowest height limit keeping that many columns
        if (columns > 1)
        then
            local lo, hi = button_h * s, avail_h
            for _ = 1, 20 do
                local mid = (lo + hi) / 2
                if (pack(s, mid) <= columns)
                then
                    hi = mid
                else
                    lo = mid
                end
            end
            columns, height = pack(s, hi)
        end

        local block_w = (columns * button_w + (columns - 1) * column_gap) * s
        local block_top = (top + bottom) / 2 + height / 2
        for _, button in ipairs(buttons) do
            local x = -block_w / 2 + (button.column - 1) * (button_w + column_gap) * s + button_w * s / 2
            local y = block_top - button.offset - button_h * s / 2
            button.plane.scaling = vec3.new(button_h * s, button_h * s, 1)
            button.plane.translation = vec3.new(x, y, 0)
        end
        menu_columns = columns
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
    -- Laid out again when the window is resized
    local w, h = window:getDimensions()
    if ((w ~= last_w or h ~= last_h) and w > 0 and h > 0)
    then
        last_w, last_h = w, h
        layout_menu(w, h)
    end

    -- Keyboard: Up / Down go through the buttons in order (column after
    -- column), Left / Right to the nearest one in the next column; both skip
    -- the greyed buttons
    local function step (dir)
        local i = selected
        repeat
            i = (i - 1 + dir) % #buttons + 1
        until buttons[i].enabled
        selected = i
    end
    local function step_column (dir)
        local current = buttons[selected]
        local column = current.column
        for _ = 1, menu_columns - 1 do
            column = (column - 1 + dir) % menu_columns + 1
            local best, best_d = nil, math.huge
            for i, button in ipairs(buttons) do
                if (button.enabled and button.column == column)
                then
                    local d = math.abs(button.offset - current.offset)
                    if (d < best_d)
                    then
                        best, best_d = i, d
                    end
                end
            end
            if (best)
            then
                selected = best
                return
            end
        end
    end
    if (window:keyIsPressed(GL.KEY_UP))    then step(-1) end
    if (window:keyIsPressed(GL.KEY_DOWN))  then step(1) end
    if (window:keyIsPressed(GL.KEY_LEFT))  then step_column(-1) end
    if (window:keyIsPressed(GL.KEY_RIGHT)) then step_column(1) end

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
