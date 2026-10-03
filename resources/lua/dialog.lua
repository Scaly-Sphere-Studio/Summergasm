-- Ren'Py dialog scene: plays a conversation with dialog(name) (see
-- Dialog.hpp for the controls) over the background its script sets.
-- Returns to the menu once the story is over, or on Escape.
if (is_loading)
then
    print(filename, "init start")

    -- Background of the script's `scene` statements: the Dialog node only
    -- shows the characters, the text and the choices, and reports the
    -- background as a signal. This scene draws it: its renderer is added
    -- first, so that it's drawn behind the dialog.
    bg_plane = GL.Plane.new(GL.Texture.new())
    bg_plane.translation = vec3.new(0, 0, -1)
    bg_renderer = GL.PlaneRenderer.new(cam_fixed)
    bg_renderer.planes = { bg_plane }
    window:addRenderer(bg_renderer)
    bg_textures = {}    -- by image path or color, loaded once
    bg_key = nil        -- of the texture shown
    bg_applied = nil    -- window & texture sizes its scaling was computed for
    bg_image, bg_color = nil, nil   -- asked by the script, see the signal below

    -- { "image path" } | { 0xRRGGBB } | {}, sent again when going back.
    -- Registered before the dialog starts, removed when the scene unloads.
    bg_handler = signals.on("background", function (name, value)
        bg_image, bg_color = nil, nil
        if (type(value) == "string") then bg_image = value
        elseif (type(value) == "number") then bg_color = value end
        return true
    end)

    -- Texture of an image path, or of a 0xRRGGBB color (black when none)
    function bg_texture (image, color)
        local key = image or color or 0
        if (bg_textures[key] == nil)
        then
            if (image ~= nil)
            then
                bg_textures[key] = GL.Texture.new(image)
            else
                local tex = GL.Texture.new()
                tex:setColor(RGBA.new((key >> 16) & 0xff, (key >> 8) & 0xff, key & 0xff, 255))
                bg_textures[key] = tex
            end
        end
        return key, bg_textures[key]
    end

    -- Follows the dialog's current step, covering the window. Images load
    -- asynchronously: the scaling is applied once their size is known.
    function update_background ()
        local key, tex = bg_texture(bg_image, bg_color)
        if (key ~= bg_key)
        then
            bg_plane.texture = tex
            bg_key = key
            bg_applied = nil
        end
        local w, h = window:getDimensions()
        local tw, th = tex:getDimensions()
        local applied = w .. "x" .. h .. " " .. tw .. "x" .. th
        if (tw ~= 0 and th ~= 0 and applied ~= bg_applied)
        then
            -- Planes keep their texture's ratio, scaling its smaller side
            local s = math.min(tw, th) * math.max(w / tw, h / th)
            bg_plane.scaling = vec3.new(s, s, 1)
            bg_applied = applied
        end
    end

    -- Played by the shared dialog node, which updates itself
    conversation = dialog("scene_vacances")
    conversation.end_text = '{{"effect":"FadingWaves","effect_offset":3}}~ Fin ~{{}}'
        .. "     (Retour arrière pour revenir, Entrée ou Échap pour retourner au menu)"
    conversation.on_finished = menu

    print(filename, "init end")

elseif (is_unloading)
then
    if (conversation ~= nil) then conversation:stop() end
    -- The script's music (`music` signal) belongs to the story: fade it out
    stop_music()
    signals.off(bg_handler)
    window:removeRenderer(bg_renderer)

elseif (is_running)
then
    if (conversation ~= nil) then update_background() end
end
