print("> Init.lua start")

print("  > Init text areas")
-- Text areas
do
    area = TR.Area.create(1280, 720)

    print("    > Fmt0")

    local fmt = area:getFmt(0)
    fmt.font = "impact.ttf"
    fmt.charsize = 150
    area:setFmt(fmt, 0)

    print("    > Fmt1")

    local fmt = area:getFmt(1)
    fmt.font = "arial.ttf"
    fmt.charsize = 150
    fmt.outline_size = 50
    fmt.has_outline = true
    fmt.has_shadow = true
    area:setFmt(fmt, 1)

    print("    > Array")

    string_array = {
        "Lorem ipsum {fmt: 1}dolor sit amet,",
        "consectetur adipiscing elit,",
        "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
        "Ut enim ad minim veniam,",
        "quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.",
        "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.",
        "Excepteur sint occaecat cupidatat non proident,",
        "sunt in culpa qui officia deserunt mollit anim id est laborum."
    }
    
    print("    > Parse string")
    area.string = string_array[1]
end

print("  > Init windows")
-- Create Window
do
    local args = GL.WindowArgs.new()
    args.w = 1280
    args.h = 720
    args.title = "Summergasm"
    args.fullscreen = false
    args.maximized = true
    args.monitor_id = 1
    
    window = GL.Window.create(args)
    window.vsync = true
    window.fps_limit = 120
    
    args = GL.WindowArgs.new()
    args.w = 600
    args.h = 600
    args.title = "Summergasm - ImGUI"
    args.hidden = true
    ui_window = GL.Window.create(args)
end

local context = GL.Context.new(window)

print("  > Create window objects")

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
chunk.title = "Beach"
chunk.planes = { sand, wet, water, foam }
plane_renderer.chunks:add(chunk)

coeff = 0

print("> Init.lua end")