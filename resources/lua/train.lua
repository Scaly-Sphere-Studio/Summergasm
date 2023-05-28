if (is_loading)
then
    print(filename, "init start")

    bg_renderer = Parallax.new(cam_fixed)
    bg_renderer.planes = {
        --GL.Plane.new("train/train trail_beach_07.png"),
        GL.Plane.new("train/train trail_beach_08.png"),
        GL.Plane.new("train/train trail_beach_09.png"),
        GL.Plane.new("train/train trail_beach_10.png"),
        GL.Plane.new("train/train trail_beach_11.png")
    }
    bg_renderer:forEach(function(plane)
        plane:setTextureCallback(function(plane)
            local w, h = plane.texture:getDimensions()
            plane:scale(h)
            plane:translate(vec3.new(0, -h / 2, 0))
        end)
    end)
    bg_renderer.speed = 300
    window:addRenderer(bg_renderer)
    
    motor = GL.Plane.new("train/train_result_5.png")
    wheels = GL.Plane.new("train/train_result_4.png")
    wheel_protection = GL.Plane.new("train/train_result_6.png")
    wagon = GL.Plane.new("train/train_result_7.png")
          
    train_renderer = GL.PlaneRenderer.new(cam_fixed, true)
    train_renderer.planes = { motor, wheels, wheel_protection, wagon }
    train_renderer:forEach(function(plane)
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane:scale(400)
    end)
    window:addRenderer(train_renderer)

    print(filename, "init end")

elseif (is_running)
then

    train_renderer:forEach(drag_plane_fixed)

    if (window:keyIsPressed(GL.KEY_SPACE))
    then
        bg_renderer:toggle();
    end

    move_camera(camera, 0.1)
    move_camera(cam_fixed, 30)
    zoom_camera(camera, 0.01)
    zoom_camera(cam_fixed, 0.01)
end
