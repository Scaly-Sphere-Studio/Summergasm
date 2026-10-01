local f = debug.getinfo(1, "S").source:match(".*/(.*)")


print(f, "start")

print(f, "  Init windows")
-- Create Window
do
    local args = GL.WindowArgs.new()
    args.w = 1280
    args.h = 720
    args.title = "Summergasm"
    args.fullscreen = false
    args.maximized = true
    args.monitor_id = 1
    
    window = GL.Window.new(args)
    window.vsync = true
    window.fps_limit = 120
    
    args = GL.WindowArgs.new()
    args.w = 600
    args.h = 600
    args.title = "Summergasm - ImGUI"
    args.hidden = true
    ui_window = GL.Window.new(args)
end


print(f, "  Init text areas")
-- Text areas
do
    TR.init()
    area = TR.Area.new(1280, 720)

    print(f, "    Fmt")

    local fmt = area:getFmt()
    fmt.font = "impact.ttf"
    fmt.charsize = 150
    area:setFmt(fmt)

    print(f, "    Array")

    string_array = {
        "Lorem ipsum {{\"font\":\"arial.ttf\"}}dolor sit amet,",
        "consectetur adipiscing elit,",
        "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
        "Ut enim ad minim veniam,",
        "quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.",
        "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.",
        "Excepteur sint occaecat cupidatat non proident,",
        "sunt in culpa qui officia deserunt mollit anim id est laborum."
    }
    
    print(f, "    Parse string")
    area.string = string_array[1]
end


print(f, "  Init window objects")
-- Create & bind window related objects
do
    camera = GL.Camera.new()
    camera.position = vec3.new(0, 0, 3)
    camera.proj_type = GL.Projection.Ortho

    cam_fixed = GL.Camera.new()
    cam_fixed.position = camera.position
    cam_fixed.proj_type = GL.Projection.OrthoFixed
end


print(f, "  Init watermark")
-- Development disclaimer, always displayed on top of every scene.
-- update_watermark() is called every frame by C++, which also keeps
-- watermark_renderer as the last (topmost) renderer of the window.
do
    watermark_area = TR.Area.new(800, 40)
    local fmt = TR.Fmt.new()
    fmt.charsize = 18
    fmt.alignment = TR.Alignment.Right
    fmt.has_outline = true
    fmt.alpha = 170
    watermark_area:setFmt(fmt)
    watermark_area.string = "Game in development - Subject to change"

    watermark = GL.Plane.new(watermark_area)

    -- Own camera, so scenes moving / zooming cam_fixed don't move the watermark
    watermark_cam = GL.Camera.new()
    watermark_cam.position = vec3.new(0, 0, 3)
    watermark_cam.proj_type = GL.Projection.OrthoFixed

    watermark_renderer = GL.PlaneRenderer.new(watermark_cam, true)
    watermark_renderer.planes = { watermark }
    window:addRenderer(watermark_renderer)

    -- Distance (in pixels) from the top right corner of the window,
    -- leaving room for a settings ribbon at the top
    watermark_margin_top = 64
    watermark_margin_right = 24

    local last_w, last_h = 0, 0
    function update_watermark ()
        local w, h = window:getDimensions()
        if (w == last_w and h == last_h)
        then
            return
        end
        last_w, last_h = w, h
        local aw, ah = watermark_area:getDimensions()
        -- Planes are already scaled to their texture ratio: scale by the
        -- height to get a pixel-perfect size with an OrthoFixed camera
        watermark.scaling = vec3.new(ah, ah, 1)
        watermark.translation = vec3.new(
            w / 2 - aw / 2 - watermark_margin_right,
            h / 2 - ah / 2 - watermark_margin_top,
            0)
    end
end


print(f, "  Init audio")
file_script("audio_setup")

print(f, "  Init model functions")
file_script("model_functions")

print(f, "end")

load_scene("menu")
