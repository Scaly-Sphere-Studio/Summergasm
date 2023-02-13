local context = GL.Context.new(window)

if (is_loading)
then
    print(filename, "init start")

    sand = GL.Plane.create("plage/sand.png")
    sand:scale(4.0)
    wet = GL.Plane.create("plage/wet_sand_layer.png")
    wet:scale(4.0)
    wet:translate(vec3.new(0, 0, 0.3))
    wet.alpha = 0
    water = GL.Plane.create("plage/water_layer.png")
    water:scale(4.0)
    water:translate(vec3.new(0, 0, 0.6))
    foam = GL.Plane.create("plage/foam.png")
    foam:translate(vec3.new(0, 0, 0.9))
    foam:scale(4.0)

    local chunk = GL.Chunk.new(camera)
    chunk.title = filename
    chunk.planes = { sand, wet, water, foam }
    plane_renderer.chunks:add(chunk)

    coeff = 0
    
    print(filename, "init end")

elseif (is_unloading)
then
    for i=1,#plane_renderer.chunks do
        if (plane_renderer.chunks[i].title == filename) then
            plane_renderer.chunks:erase(i)
            break
        end
    end

elseif (is_running)
then
    local speed = 0.015

    if (window:keyIsPressed(GL.KEY_UP))
    then
        camera:move( vec3.new(0, speed, 0) )
    end
    if(window:keyIsPressed(GL.KEY_DOWN))
    then
        camera:move( vec3.new(0, -speed, 0) )
    end
    if(window:keyIsPressed(GL.KEY_LEFT))
    then
        camera:move( vec3.new(-speed, 0, 0) )
    end
    if(window:keyIsPressed(GL.KEY_RIGHT))
    then
        camera:move( vec3.new(speed, 0, 0) )
    end

    water.translation = vec3.new(0, math.cos(math.pi * coeff) / 2, 0)
    foam.translation = water.translation
    foam.alpha = math.abs(coeff)
    wet.alpha = wet.alpha - 0.004

    coeff = coeff + 0.005

    if (coeff >= 1)
    then
        coeff = -1
        wet.translation = water.translation
        wet.alpha = 1
    end
end
