if (is_loading)
then
    print(filename, "init start")

    area = TR.Area.new(1920, 1080)
    local fmt = TR.Fmt.new()
    --fmt.font = "Noto Sans Mono Condensed Medium Nerd Font Complete Windows Compatible.ttf"
    fmt.charsize = 40
    fmt.alignment = TR.Alignment.Center
    area:setFmt(fmt, 0)

    fmt.charsize = 28
    fmt.alignment = TR.Alignment.Left
    area:setFmt(fmt, 1)

    fmt.charsize = 24
    area:setFmt(fmt, 2)

    area.string = [[
Summergasm (dev homescreen)
{{fmt: 1}}
F1 => ImGui settings
F2 => Lua Console{{fmt: 2}}
      => file_script()
      => load_scene()
      => unload_scene()
      => (all other available Lua functions...)
{{fmt: 1}}
List of scenes available in F1 settings
]]

    text = GL.Plane.new(area)
    text:scale(2)
    text.hitbox = GL.PlaneHitbox.Full

    plane_renderer = GL.PlaneRenderer.new(camera)
    plane_renderer.planes:add(text)
    window:addRenderer(plane_renderer)

    print(filename, "init end")

elseif (is_unloading)
then
    window:removeRenderer(plane_renderer)
    
elseif (is_running)
then
    drag_plane(text)
    unload_scene("homescreen")
    load_scene("train.lua")
end
