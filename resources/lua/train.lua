if (is_loading)
then
    print(filename, "init start")

    local win_w, win_h = window:getDimensions()

    -- Perspective camera, placed so that the z = 0 plane is pixel-exact:
    -- planes at z = 0 keep the sizes they had with an OrthoFixed camera.
    -- Other depths are compensated with depth_scale() to keep their size.
    cam_train = GL.Camera.new()
    cam_train.proj_type = GL.Projection.Perspective
    cam_train.fov = 40
    cam_distance = perspective_distance(cam_train, win_h)
    cam_train.position = vec3.new(0, 0, cam_distance)
    cam_train.z_near = 10
    cam_train.z_far = cam_distance + 6000
    cam_yaw = 0
    local D = cam_distance

    -- The whole scene in one renderer: planes are sorted back to front from
    -- the camera, the parallax comes from their depth
    scene_renderer = SceneRenderer.new(cam_train)
    scene_renderer.scroll_speed = 600

    -- Sky: far behind everything, covers the whole window
    local sky_z = -5000
    sky = GL.Plane.new("train/train trail_beach_02.png")
    sky.translation = vec3.new(0, 0, sky_z)
    scale_when_loaded(sky, function(plane, w, h)
        local s = math.max(win_h, win_w * h / w) * depth_scale(D, sky_z)
        plane.scaling = vec3.new(s, s, 1)
    end)
    scene_renderer:addPlane(sky)
    -- The sky is its own light
    scene_renderer:setLightingFactor(sky, 0)
    scene_renderer:setCastShadow(sky, false)

    local nuage_z = sky_z + 500
    local nuage_scaling = 0.7
    nuage = GL.Plane.new("train/nuage.png")
    nuage.translation = vec3.new(0, 0, nuage_z)
    scale_when_loaded(nuage, function(plane, w, h)
        local s = math.max(win_h, win_w * h / w) * depth_scale(D, nuage_z)
        plane.scaling = vec3.new(s * nuage_scaling, s * nuage_scaling, 1) 
    end)
    scene_renderer:addPlane(nuage)
    -- The sky is its own light
    scene_renderer:setLightingFactor(nuage, 0)
    scene_renderer:setCastShadow(nuage, false)


    -- Background trail, scrolling behind the train
    local bg_z = -1500
    local bg_tiles = {
        --GL.Plane.new("train/train trail_beach_07.png"),
        GL.Plane.new("train/train trail_beach_08.png"),
        GL.Plane.new("train/train trail_beach_09.png"),
        GL.Plane.new("train/train trail_beach_10.png"),
        GL.Plane.new("train/train trail_beach_11.png")
    }
    for _, plane in ipairs(bg_tiles) do
        plane.translation = vec3.new(0, 0, bg_z)
        -- Not in setTextureCallback: GL resets the scaling to the image size
        -- once loaded, after the callback
        scale_when_loaded(plane, function(plane, w, h)
            local s = h * depth_scale(D, bg_z)
            plane.scaling = vec3.new(s, s, 1)
            local t = plane.translation
            plane.translation = vec3.new(t.x, -s / 2, bg_z)
        end)
    end
    scene_renderer:addScrollLayer(bg_tiles)

    -- Dune layers between the background trail and the train, catching the
    -- sun's shadows. scale: on screen, relative to the image size;
    -- bottom: on screen, in pixels from the window's center
    local function add_dune_layer(names, z, scale, bottom)
        local tiles = {}
        for _, name in ipairs(names) do
            local plane = GL.Plane.new("train/train trail_beach_" .. name .. ".png")
            plane.translation = vec3.new(0, 0, z)
            scale_when_loaded(plane, function(plane, w, h)
                local k = depth_scale(D, z)
                local s = h * scale
                plane.scaling = vec3.new(s * k, s * k, 1)
                local t = plane.translation
                plane.translation = vec3.new(t.x, (bottom + s / 2) * k, z)
            end)
            tiles[#tiles + 1] = plane
        end
        scene_renderer:addScrollLayer(tiles)
        return tiles
    end
    mid_far_tiles = add_dune_layer({ "07", "17", "07", "16" }, -1000, 0.6, -win_h / 2)
    mid_near_tiles = add_dune_layer({ "15", "18", "17", "16", "15", "17" }, -500, 0.42, -win_h / 2 + 80)

    -- Train parts, from back to front, each at its own depth
    wheels = GL.Plane.new("train/train_result_4.png")
    wheel_protection = GL.Plane.new("train/train_result_6.png")
    motor = GL.Plane.new("train/train_result_5.png")
    wagon = GL.Plane.new("train/train_result_7.png")
    train_parts = { wheels, wheel_protection, motor, wagon  }
    for i, plane in ipairs(train_parts) do
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane.translation = vec3.new(0, -200, (i - 1)*20)
        scale_when_loaded(plane, vec3.new(400, 400, 400))
        scene_renderer:addPlane(plane)
    end

    -- Foreground dunes, between the train and the camera, along the bottom
    -- of the window
    local fg_z = 300
    local dunes_scale = 0.5
    local fg_tiles = {
        GL.Plane.new("train/train trail_beach_15.png"),
        GL.Plane.new("train/train trail_beach_16.png"),
        GL.Plane.new("train/train trail_beach_17.png"),
        GL.Plane.new("train/train trail_beach_18.png")
    }
    for _, plane in ipairs(fg_tiles) do
        plane.translation = vec3.new(0, 0, fg_z)
        scale_when_loaded(plane, function(plane, w, h)
            local k = depth_scale(D, fg_z)
            local s = h * dunes_scale
            plane.scaling = vec3.new(s * k, s * k, 1)
            local t = plane.translation
            plane.translation = vec3.new(t.x, (s - win_h) / 2 * k, fg_z)
        end)
    end
    scene_renderer:addScrollLayer(fg_tiles)

    -- Lighting (sRGB colors, tune them in the inspector: Scene > scene_renderer)
    scene_renderer.ambient = vec3.new(0.55, 0.55, 0.62)

    -- Sun: slightly higher than the train, between it and the dunes behind.
    -- Planes face the camera: straight from above, each dune's shadow would
    -- fall behind itself. Coming from the left, the shadows slide right,
    -- into the gaps between the dunes behind and past the fence posts.
    -- The train, in front of the sun, is backlit: warm terminator rim.
    sun = PointLight.new()
    sun.position = vec3.new(500, 200, 2500)
    sun.color = vec3.new(1, 0.92, 0.8)
    sun.intensity = 1.5
    sun.radius = 9000
    sun.falloff = 0.5
    sun.terminator_color = vec3.new(1, 0.45, 0.2)
    sun.terminator_width = 0
    -- Shadows toward the dunes behind, filled with a deep blue
    sun.cast_shadows = true
    sun.shadow_direction = vec3.new(1400, -500, -1200)
    sun.shadow_fov = 140
    sun.shadow_bias = 1
    sun.shadow_softness = 1.5
    -- Large source: shadows soften as they stretch away from their casters
    sun.size = 0
    sun.shadow_color = vec3.new(0.12, 0.1, 0.3)
    scene_renderer:addLight(sun)
    scene_renderer.shadow_map_size = 2048

    -- Close colored light by the train, with a wide terminator ramp
    lamp = PointLight.new()
    lamp.position = vec3.new(250, 50, 120)
    lamp.color = vec3.new(1, 0.8, 0.5)
    lamp.intensity = 1.5
    lamp.radius = 900
    lamp.falloff = 2
    lamp.terminator_color = vec3.new(1, 0.3, 0.2)
    lamp.terminator_width = 0.4
    lamp.shadow_color = vec3.new(0.1, 0.05, 0.25)
    -- Train parts are 1 unit apart: bias below that
    lamp.cast_shadows = true
    lamp.shadow_fov = 140
    lamp.shadow_bias = 0.3
    lamp.size = 20
    scene_renderer:addLight(lamp)

    local function add_light(position, color, intensity, radius, falloff)
        local light = PointLight.new()
        light.position = position
        light.color = color
        light.intensity = intensity
        light.radius = radius
        light.falloff = falloff
        scene_renderer:addLight(light)
        return light
    end

    -- Magenta glow over the background trail
    trail_light = add_light(vec3.new(600, 150, -1300), vec3.new(1, 0.25, 0.8), 2.5, 1600, 1.5)

    -- Cyan light left of the train, teal terminator
    side_light = add_light(vec3.new(-450, 150, 150), vec3.new(0.3, 0.9, 1), 1.8, 700, 2)
    side_light.terminator_color = vec3.new(0, 0.5, 0.45)
    side_light.terminator_width = 0.3
    side_light.cast_shadows = true
    side_light.shadow_fov = 140
    side_light.shadow_bias = 0.3
    side_light.size = 20

    -- Colored shadows: planes letting light through, tinted by their texels
    -- (0 = opaque, 1 = colored glass), e.g.
    -- scene_renderer:setShadowTransmission(some_glass_plane, 0.8)

    -- Lights riding along the foreground dunes, one per tile (see is_running).
    -- In front of the dunes (dz > 0) they light them directly; just behind
    -- them (dz < 0) the dunes are backlit, showing the terminator & shadow
    -- colors, and the train behind gets their color.
    -- x & y are offsets from the tile's center, in tile heights.
    local dune_light_setups = {
        { color = vec3.new(1, 0.35, 0.15), x = -0.3, y = 0.25, dz = 80 },
        { color = vec3.new(0.35, 0.55, 1), x = 0.2, y = 0.1, dz = -40,
          terminator = vec3.new(0.7, 0.2, 1), shadow = vec3.new(0.05, 0.05, 0.3) },
        { color = vec3.new(0.4, 1, 0.35), x = 0, y = 0.35, dz = 60 },
        { color = vec3.new(1, 0.8, 0.2), x = -0.15, y = 0.05, dz = -40,
          terminator = vec3.new(1, 0.25, 0.1), shadow = vec3.new(0.3, 0.05, 0.05) },
    }
    dune_lights = {}
    for i, plane in ipairs(fg_tiles) do
        local setup = dune_light_setups[(i - 1) % #dune_light_setups + 1]
        local light = add_light(vec3.new(0, 0, fg_z + setup.dz), setup.color, 2, 500, 1.5)
        -- Moved with the tile by the renderer, after scrolling (no lag)
        light.follow = plane
        if (setup.terminator)
        then
            light.terminator_color = setup.terminator
            light.terminator_width = 0.5
            light.shadow_color = setup.shadow
        end
        dune_lights[#dune_lights + 1] = { plane = plane, light = light, x = setup.x, y = setup.y, dz = setup.dz }
    end

    window:addRenderer(scene_renderer)

    print(filename, "init end")

elseif (is_running)
then
    apply_loaded_scalings()

    -- Keep z = 0 pixel-exact when the window height changes
    local _, win_h = window:getDimensions()
    local D = perspective_distance(cam_train, win_h)
    if (D ~= cam_distance)
    then
        cam_train:move(vec3.new(0, 0, D - cam_distance), false)
        cam_train.z_far = D + 6000
        cam_distance = D
    end

    for _, plane in ipairs(train_parts) do
        drag_plane_fixed(plane)
    end

    -- Dune lights' offsets from their tile, in tile heights (known once loaded)
    for _, d in ipairs(dune_lights) do
        local h = d.plane.scaling.y
        d.light.follow_offset = vec3.new(d.x * h, d.y * h, d.dz)
    end

    if (window:keyIsPressed(GL.KEY_SPACE))
    then
        scene_renderer:toggle()
    end

    -- Depth checks: W/S dolly, Q/E yaw, R reset (US key positions)
    if (window:keyIsHeld(GL.KEY_W)) then
        cam_train:move(vec3.new(0, 0, -20))
    end
    if (window:keyIsHeld(GL.KEY_S)) then
        cam_train:move(vec3.new(0, 0, 20))
    end
    if (window:keyIsHeld(GL.KEY_Q)) then
        cam_train:rotate(vec2.new(0, -0.5))
        cam_yaw = cam_yaw - 0.5
    end
    if (window:keyIsHeld(GL.KEY_E)) then
        cam_train:rotate(vec2.new(0, 0.5))
        cam_yaw = cam_yaw + 0.5
    end
    if (window:keyIsPressed(GL.KEY_R)) then
        cam_train:rotate(vec2.new(0, -cam_yaw))
        cam_yaw = 0
        cam_train.position = vec3.new(0, 0, cam_distance)
        cam_train.zoom = 1
    end

    move_camera(cam_train, 30)
    zoom_camera(cam_train, 0.01)
end
