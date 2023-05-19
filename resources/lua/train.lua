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
    bg_renderer.speed = -500
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


    local cam_speed = 30
    if (window:keyIsHeld(GL.KEY_UP)) then
        cam_fixed:move(vec3.new(0, cam_speed, 0))
    end
    if (window:keyIsHeld(GL.KEY_DOWN)) then
        cam_fixed:move(vec3.new(0, -cam_speed, 0))
    end
    if (window:keyIsHeld(GL.KEY_LEFT)) then
        cam_fixed:move(vec3.new(-cam_speed, 0, 0))
    end
    if (window:keyIsHeld(GL.KEY_RIGHT)) then
        cam_fixed:move(vec3.new(cam_speed, 0, 0))
    end
    local cam_zoom = 0.01
    if (window:keyIsHeld(GL.KEY_KP_ADD)) then
        cam_fixed:zoomIn(cam_zoom)
    end
    if (window:keyIsHeld(GL.KEY_KP_SUBTRACT)) then
        cam_fixed:zoomOut(cam_zoom)
    end
end
