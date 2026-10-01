if (is_loading)
then
    print(filename, "init start")

    sand = GL.Plane.new("plage/sand.png")
    scale_when_loaded(sand, vec3.new(4, 4, 4))
    wet = GL.Plane.new("plage/wet_sand_layer.png")
    scale_when_loaded(wet, vec3.new(4, 4, 4))
    wet:translate(vec3.new(0, 0, 0.3))
    wet.alpha = 0
    water = GL.Plane.new("plage/water_layer.png")
    scale_when_loaded(water, vec3.new(4, 4, 4))
    water:translate(vec3.new(0, 0, 0.6))
    foam = GL.Plane.new("plage/foam.png")
    foam:translate(vec3.new(0, 0, 0.9))
    scale_when_loaded(foam, vec3.new(4, 4, 4))

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
    apply_loaded_scalings()

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

    -- Keep each layer at its own depth (sand 0, wet 0.3, water 0.6, foam 0.9):
    -- planes at the same z fail the depth test and aren't drawn
    local wave_y = math.cos(math.pi * coeff) / 2
    water.translation = vec3.new(0, wave_y, 0.6)
    foam.translation = vec3.new(0, wave_y, 0.9)
    foam.alpha = math.abs(coeff)
    wet.alpha = wet.alpha - 0.004

    coeff = coeff + 0.005

    if (coeff >= 1)
    then
        coeff = -1
        wet.translation = vec3.new(0, water.translation.y, 0.3)
        wet.alpha = 1
    end
end
