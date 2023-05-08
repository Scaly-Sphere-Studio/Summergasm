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

    bg_is_setup = false
    bg_width = 0

    function setup_bg()
        for i = 1, #bg do
            local plane = bg[i]
            if (plane.texture:hasRunningThread())
            then
                return;
            end
        end

        for i = 1, #bg do
            local plane = bg[i]
            local w, h = plane.texture:getDimensions()
            print(w, h)
            plane.scaling = vec3.new(h)
            plane.translation = vec3.new(bg_width, -300, 0)
            bg_width = bg_width + w-1
        end

        bg_is_setup = true
        print(bg_width)
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
    bg_renderer = GL.PlaneRenderer.new(cam_fixed)
    bg_renderer.planes = bg
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

    if (not bg_is_setup)
    then
        print("not yet")
        setup_bg()
    else
        for i = 1, #bg do
            local plane = bg[i]
            plane:translate(vec3.new(-5, 0, 0))
            if (plane.translation.x < (bg_width / -2))
            then
                plane:translate(vec3.new(bg_width, 0, 0))
            end
        end
    end

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
