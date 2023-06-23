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


print(f, "  Init audio")
file_script("audio_setup")

print(f, "  Init model functions")
file_script("model_functions")

print(f, "end")

load_scene("homescreen")
