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
        -- Called on every texture content update (several times while
        -- loading): set absolute values, not relative scale()/translate()
        plane:setTextureCallback(function(plane)
            local w, h = plane.texture:getDimensions()
            local s = h * depth_scale(D, bg_z)
            plane.scaling = vec3.new(s, s, 1)
            local t = plane.translation
            plane.translation = vec3.new(t.x, -s / 2, bg_z)
        end)
    end
    scene_renderer:addScrollLayer(bg_tiles)

    -- Train parts, from back to front, each at its own depth
    wheels = GL.Plane.new("train/train_result_4.png")
    wheel_protection = GL.Plane.new("train/train_result_6.png")
    motor = GL.Plane.new("train/train_result_5.png")
    wagon = GL.Plane.new("train/train_result_7.png")
    train_parts = { wheels, wheel_protection, motor, wagon  }
    for i, plane in ipairs(train_parts) do
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane.translation = vec3.new(0, 0, i - 1)
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
