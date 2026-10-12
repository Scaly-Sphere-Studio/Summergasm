-- Night around a fire pit: two characters behind the fire, two in front,
-- standing on a dirt ground lit by the flames. The sky is the window's
-- (black) clear color, the ground fades into it past the firelight.
if (is_loading)
then
    print(filename, "init start")

    -- Perspective camera above the front characters, looking down at the fire
    cam_fire = GL.Camera.new()
    cam_fire.proj_type = GL.Projection.Perspective
    cam_fire.fov = 40
    cam_fire.z_near = 10
    cam_fire.z_far = 6000
    cam_start = vec3.new(0, 200, 1450)
    cam_pitch = -0
    cam_fire.position = cam_start
    -- Positive x angles look up
    cam_fire.rotation = vec2.new(cam_pitch, 0)

    scene_renderer = SceneRenderer.new(cam_fire)

    -- Ground: one plane laid flat (y = 0), the image's top toward the back,
    -- from z = -1980 to 1100
    local ground_depth = 3080
    ground = GL.Plane.new("firepit/ground.jpg")
    ground.rotation = vec3.new(-90, 0, 0)
    ground.translation = vec3.new(0, 0, -440)
    scale_when_loaded(ground, vec3.new(ground_depth, ground_depth, 1))
    scene_renderer:addPlane(ground)
    scene_renderer:setCastShadow(ground, false)

    -- Characters, standing on the ground (feet at y = 0). height: in world
    -- units, mirror: faces the other way
    local character_setups = {
        -- Behind the fire
        { image = "char/char5.png",       x = -210, z = -220, height = 430 },
        { image = "char/char1.png",       x = 200,  z = -220, height = 470 },
        -- In front of the fire, apart so it stays visible between them
        { image = "char/char4.png",       x = -370, z = 220,  height = 300 },
        { image = "char/char1_happy.png", x = 370,  z = 220,  height = 470, mirror = true },
    }
    characters = {}
    for _, setup in ipairs(character_setups) do
        local plane = GL.Plane.new(setup.image)
        plane.hitbox = GL.PlaneHitbox.Alpha
        plane.translation = vec3.new(setup.x, setup.height / 2, setup.z)
        local sx = setup.mirror and -setup.height or setup.height
        scale_when_loaded(plane, vec3.new(sx, setup.height, 1))
        scene_renderer:addPlane(plane)
        characters[#characters + 1] = plane
    end

    -- Flames: unlit & glowing through their emissive map, not blocking the
    -- fire light inside them. x, z: around the pit's center, height: base
    -- size, animated in is_running
    local flame_setups = {
        { x = 0,   z = 0,   height = 170, mirror = false },
        { x = -42, z = -12, height = 115, mirror = true },
        { x = 40,  z = 10,  height = 100, mirror = false },
    }
    flames = {}
    for i, setup in ipairs(flame_setups) do
        local plane = GL.Plane.new("firepit/flame.png")
        plane.translation = vec3.new(setup.x, setup.height / 2, setup.z)
        scene_renderer:addPlane(plane)
        scene_renderer:setLightingFactor(plane, 0)
        scene_renderer:setCastShadow(plane, false)
        scene_renderer:setEmissiveMap(plane, "firepit/flame.png")
        scene_renderer:setEmissiveColor(plane, vec3.new(1, 0.7, 0.4))
        scene_renderer:setEmissiveIntensity(plane, 1.5)
        flames[#flames + 1] = { plane = plane, x = setup.x, z = setup.z, height = setup.height,
            mirror = setup.mirror and -1 or 1, phase = i * 1.7 }
    end

    -- Lighting (sRGB colors, tune them in the inspector: Scene > scene_renderer)
    -- Night: dim & blue, the fire does the rest
    scene_renderer.ambient = vec3.new(0.1, 0.11, 0.2)
    scene_renderer.shadow_map_size = 2048

    -- Fire light, just above the flames. The characters in front are
    -- backlit: wide warm terminator wrapping around them.
    fire_light = PointLight.new()
    fire_light_base = vec3.new(0, 110, 0)
    fire_light.position = fire_light_base
    fire_light.color = vec3.new(1, 0.55, 0.2)
    fire_light_intensity = 2.2
    fire_light.intensity = fire_light_intensity
    fire_light.radius = 1600
    fire_light.falloff = 1.6
    fire_light.terminator_color = vec3.new(1, 0.25, 0.05)
    fire_light.terminator_width = 0.6
    fire_light.shadow_color = vec3.new(0.02, 0.01, 0.04)
    -- In the middle of the scene: shadows in a cone toward the ground,
    -- the characters' shadows stretching away from the fire
    fire_light.cast_shadows = true
    fire_light.shadow_direction = vec3.new(0, -1, 0)
    fire_light.shadow_fov = 170
    fire_light.shadow_bias = 1
    fire_light.size = 40
    scene_renderer:addLight(fire_light)

    -- Faint moonlight from the top left, behind the camera: keeps the
    -- characters in front from being pure silhouettes
    moon_light = PointLight.new()
    moon_light.position = vec3.new(-1500, 2500, 2000)
    moon_light.color = vec3.new(0.45, 0.55, 1)
    moon_light.intensity = 0.35
    moon_light.radius = 9000
    moon_light.falloff = 0.5
    scene_renderer:addLight(moon_light)

    window:addRenderer(scene_renderer)

    -- Flicker clock, in seconds (advanced by the global dt each frame)
    fire_t = 0

    print(filename, "init end")

elseif (is_unloading)
then
    window:removeRenderer(scene_renderer)

elseif (is_running)
then
    apply_loaded_scalings()

    -- Flicker: a few unrelated sines, the light following the flames
    -- The rates below were tuned per frame at 60 fps: t counts 60ths of a second
    fire_t = fire_t + dt
    local t = fire_t * 60
    local flicker = 0.12 * math.sin(t * 0.21) + 0.07 * math.sin(t * 0.53 + 1) + 0.05 * math.sin(t * 1.37)
    fire_light.intensity = fire_light_intensity * (1 + flicker)
    fire_light.position = vec3.new(
        fire_light_base.x + 6 * math.sin(t * 0.31),
        fire_light_base.y + 8 * flicker,
        fire_light_base.z)

    for _, f in ipairs(flames) do
        -- GL resets the scaling once the image is loaded: set every frame
        local w = f.plane.texture:getDimensions()
        if (w ~= 0)
        then
            local p = t * 0.09 + f.phase
            local sy = 1 + 0.08 * math.sin(p * 2.3) + 0.04 * math.sin(p * 5.1)
            local sx = 1 - 0.04 * math.sin(p * 2.3 + 0.5)
            local h = f.height * sy
            f.plane.scaling = vec3.new(f.mirror * f.height * sx, h, 1)
            -- Base on the ground
            f.plane.translation = vec3.new(f.x, h / 2, f.z)
        end
    end

    -- Arrows move, keypad +/- zoom, R reset
    if (window:keyIsPressed(GL.KEY_R)) then
        cam_fire.position = cam_start
        cam_fire.zoom = 1
    end
    move_camera(cam_fire, 10)
    zoom_camera(cam_fire, 0.01)
end
