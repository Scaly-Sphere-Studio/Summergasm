local context = GL.Context.new(window)

if (is_loading)
then
    print(filename, "init start")

    bg = {
        --GL.Plane.create("train/train trail_beach_07.png"),
        GL.Plane.create("train/train trail_beach_08.png"),
        GL.Plane.create("train/train trail_beach_09.png"),
        GL.Plane.create("train/train trail_beach_10.png"),
        GL.Plane.create("train/train trail_beach_11.png")
    }
    current_bg = 1

    local train1 = GL.Plane.create("train/train_result_5.png")
    local train2 = GL.Plane.create("train/train_result_4.png")
    local train3 = GL.Plane.create("train/train_result_6.png")
    local train4 = GL.Plane.create("train/train_result_7.png")

    plane_renderer.chunks:add(GL.Chunk.new(cam_fixed))
    plane_renderer.chunks[1].planes = bg

    plane_renderer.chunks:add(GL.Chunk.new(cam_fixed, true))
    --plane_renderer.chunks[2].planes = { train1, train2, train3, train4 }


    print(filename, "init end")
elseif (is_unloading)
then

elseif (is_running)
then
    local total = 0
    for i = 1, #bg do
        local plane = bg[i]
        local w, h = plane.texture:getDimensions()
        plane.scaling = vec3.new(h)
        plane.translation = vec3.new(total, h, 0)
        total = total + w-1
    end

    local cam_speed = 30
    if (window:keyIsPressed(GL.KEY_UP)) then
        cam_fixed:move(vec3.new(0, cam_speed, 0))
    end
    if (window:keyIsPressed(GL.KEY_DOWN)) then
        cam_fixed:move(vec3.new(0, -cam_speed, 0))
    end
    if (window:keyIsPressed(GL.KEY_LEFT)) then
        cam_fixed:move(vec3.new(-cam_speed, 0, 0))
    end
    if (window:keyIsPressed(GL.KEY_RIGHT)) then
        cam_fixed:move(vec3.new(cam_speed, 0, 0))
    end
end
