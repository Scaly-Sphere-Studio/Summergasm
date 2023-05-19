if (is_loading)
then
    print(filename, "init start")

    bg = {
        --GL.Plane.new("train/train trail_beach_07.png"),
        GL.Plane.new("train/train trail_beach_08.png"),
        GL.Plane.new("train/train trail_beach_09.png"),
        GL.Plane.new("train/train trail_beach_10.png"),
        GL.Plane.new("train/train trail_beach_11.png")
    }


    for i = 1, #bg do
        local plane = bg[i]
        local function update_plane()
            local w, h = plane.texture:getDimensions()
            plane:scale(h)
            plane:translate(vec3.new(0, -h / 2, 0))
        end
        plane.texture:setUpdateCallback(update_plane)
        plane.hitbox = GL.PlaneHitbox.Full
    end
    
    motor = GL.Plane.new("train/train_result_5.png")
    wheels = GL.Plane.new("train/train_result_4.png")
    wheel_protection = GL.Plane.new("train/train_result_6.png")
    wagon = GL.Plane.new("train/train_result_7.png")
    train = { motor, wheels, wheel_protection, wagon }
    for i = 1, #train do
        local plane = train[i]
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane:scale(400)
    end
    bg_renderer = Parallax.new(cam_fixed)
    bg_renderer.planes = bg
    bg_renderer.speed = 300
    window:addRenderer(bg_renderer)

    train_renderer = GL.PlaneRenderer.new(cam_fixed, true)
    train_renderer.planes = train
    window:addRenderer(train_renderer)

    print(filename, "init end")
elseif (is_unloading)
then
    window:removeRenderer(bg_renderer)
    window:removeRenderer(train_renderer)

elseif (is_running)
then

    for i = 1, #train do
        local plane = train[i]
        drag_plane_fixed(plane)
    end

    if (window:keyIsPressed(GL.KEY_SPACE))
    then
        bg_renderer:toggle();
    end

    move_camera(camera, 0.1)
    move_camera(cam_fixed, 30)
    zoom_camera(camera, 0.01)
    zoom_camera(cam_fixed, 0.01)
end
