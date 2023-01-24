local filename = debug.getinfo(1, "S").source:match(".*/(.*)")
local state = get_script_state(filename)
local context = GL.Context.new(window)

if (state == SceneState.Loading)
then
    print(filename, "init start")
    
    camera = GL.Camera.create()
    camera.position = vec3.new(0, 0, 3)
    camera.proj_type = GL.Projection.Ortho

    sand = GL.Plane.create(GL.Texture.create("resources/assets/plage/sand.png"))
    sand:scale(4.0)
    wet = GL.Plane.create(GL.Texture.create("resources/assets/plage/wet_sand_layer.png"))
    wet:scale(4.0)
    wet:translate(vec3.new(0, 0, 0.3))
    wet.alpha = 0
    water = GL.Plane.create(GL.Texture.create("resources/assets/plage/water_layer.png"))
    water:scale(4.0)
    water:translate(vec3.new(0, 0, 0.6))
    foam = GL.Plane.create(GL.Texture.create("resources/assets/plage/foam.png"))
    foam:translate(vec3.new(0, 0, 0.9))
    foam:scale(4.0)

    plane_renderer = GL.PlaneRenderer.create(window)

    local chunk = GL.Chunk.new(camera)
    chunk.title = filename
    chunk.planes = { sand, wet, water, foam }
    plane_renderer.chunks:add(chunk)

    coeff = 0
    
    print(filename, "init end")

elseif (state == SceneState.Unloading)
then
    for i=1,#plane_renderer.chunks do
        if (plane_renderer.chunks[i].title == filename) then
            plane_renderer.chunks:erase(i)
            break
        end
    end
    -- TODO: cleanup textures
    sand = nil
    wet = nil
    water = nil
    foam = nil

elseif (state == SceneState.Running)
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
