local context = GL.Context.new(window)

if (is_loading)
then
    print(filename, "init start")

    local area = TR.Area.create(1920, 1080)
    local fmt = TR.Fmt.new()
    fmt.font = "Noto Sans Mono Condensed Medium Nerd Font Complete Windows Compatible.ttf"
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

    text = GL.Plane.create(GL.Texture.create(area))
    text:scale(2)

    local chunk = GL.Chunk.new(camera)
    chunk.title = filename
    chunk.planes = { text }
    plane_renderer.chunks:add(chunk)

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
    
end
