if (is_loading)
then
    print(filename, "init start")

    sand = GL.Plane.new("plage/sand.png")
    sand:scale(4.0)
    wet = GL.Plane.new("plage/wet_sand_layer.png")
    wet:scale(4.0)
    wet:translate(vec3.new(0, 0, 0.3))
    wet.alpha = 0
    water = GL.Plane.new("plage/water_layer.png")
    water:scale(4.0)
    water:translate(vec3.new(0, 0, 0.6))
    foam = GL.Plane.new("plage/foam.png")
    foam:translate(vec3.new(0, 0, 0.9))
    foam:scale(4.0)

    renderer = GL.PlaneRenderer.new(camera)
    renderer.planes = { sand, wet, water, foam }
    window:addRenderer(renderer)

    coeff = 0
    
    print(filename, "init end")

elseif (is_unloading)
then
    window:removeRenderer(renderer)

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
