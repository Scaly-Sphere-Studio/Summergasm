if (is_loading)
then
    print(filename, "init start")

    area = TR.Area.new(1920, 1080)
    local fmt = TR.Fmt.new()
    --fmt.font = "Noto Sans Mono Condensed Medium Nerd Font Complete Windows Compatible.ttf"
    fmt.charsize = 28
    fmt.alignment = TR.Alignment.Left
    area:setFmt(fmt)

    area.string = [[{{"charsize":40,"alignment":"Center"}}
Summergasm (dev homescreen)
{{}}
F1 => ImGui settings
F2 => Lua Console{{"charsize":24}}
      => file_script()
      => load_scene()
      => unload_scene()
      => (all other available Lua functions...)
{{}}
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
end
