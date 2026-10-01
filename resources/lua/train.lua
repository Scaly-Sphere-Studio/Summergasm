if (is_loading)
then
    print(filename, "init start")

    local win_w, win_h = window:getDimensions()

    -- Sky: fixed, covers the whole window, behind everything
    sky = GL.Plane.new("train/train trail_beach_02.png")
    sky.translation = vec3.new(0, 0, -1)
    scale_when_loaded(sky, function(plane, w, h)
        local s = math.max(win_h, win_w * h / w)
        plane.scaling = vec3.new(s, s, 1)
    end)
    sky_renderer = GL.PlaneRenderer.new(cam_fixed)
    sky_renderer.planes = { sky }
    window:addRenderer(sky_renderer)

    -- Background trail, scrolling behind the train
    bg_renderer = Parallax.new(cam_fixed)
    bg_renderer.planes = {
        --GL.Plane.new("train/train trail_beach_07.png"),
        GL.Plane.new("train/train trail_beach_08.png"),
        GL.Plane.new("train/train trail_beach_09.png"),
        GL.Plane.new("train/train trail_beach_10.png"),
        GL.Plane.new("train/train trail_beach_11.png")
    }
    bg_renderer:forEach(function(plane)
        -- Called on every texture content update (several times while
        -- loading): set absolute values, not relative scale()/translate()
        plane:setTextureCallback(function(plane)
            local w, h = plane.texture:getDimensions()
            plane.scaling = vec3.new(h, h, 1)
            local t = plane.translation
            plane.translation = vec3.new(t.x, -h / 2, t.z)
        end)
    end)
    bg_renderer.speed = 300
    window:addRenderer(bg_renderer)

    -- Train parts, from back to front: each needs its own depth, as planes
    -- at the same z fail the depth test and only the first drawn one shows
    wagon = GL.Plane.new("train/train_result_7.png")
    motor = GL.Plane.new("train/train_result_5.png")
    wheel_protection = GL.Plane.new("train/train_result_6.png")
    wheels = GL.Plane.new("train/train_result_4.png")

    train_renderer = GL.PlaneRenderer.new(cam_fixed, true)
    train_renderer.planes = { wagon, motor, wheel_protection, wheels }
    local train_z = 0
    train_renderer:forEach(function(plane)
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane.translation = vec3.new(0, 0, train_z)
        train_z = train_z + 0.1
        scale_when_loaded(plane, vec3.new(400, 400, 400))
    end)
    window:addRenderer(train_renderer)

    -- Foreground dunes, closer to the camera: drawn over the train, along
    -- the bottom of the window, scrolling faster than the background
    local dunes_scale = 0.5
    fg_renderer = Parallax.new(cam_fixed, true)
    fg_renderer.planes = {
        GL.Plane.new("train/train trail_beach_15.png"),
        GL.Plane.new("train/train trail_beach_16.png"),
        GL.Plane.new("train/train trail_beach_17.png"),
        GL.Plane.new("train/train trail_beach_18.png")
    }
    fg_renderer:forEach(function(plane)
        scale_when_loaded(plane, function(plane, w, h)
            local s = h * dunes_scale
            plane.scaling = vec3.new(s, s, 1)
            local t = plane.translation
            plane.translation = vec3.new(t.x, (s - win_h) / 2, t.z)
        end)
    end)
    fg_renderer.speed = 600
    window:addRenderer(fg_renderer)

    print(filename, "init end")

elseif (is_running)
then
    apply_loaded_scalings()

    train_renderer:forEach(drag_plane_fixed)

    if (window:keyIsPressed(GL.KEY_SPACE))
    then
        bg_renderer:toggle()
        fg_renderer:toggle()
    end

    move_camera(camera, 0.1)
    move_camera(cam_fixed, 30)
    zoom_camera(camera, 0.01)
    zoom_camera(cam_fixed, 0.01)
end
