-- Map: a plain background with the map overlay (see MapOverlay.hpp) opened
-- over it at once. M closes / reopens the map, Escape closes it, then a
-- second Escape goes back to the menu.
if (is_loading)
then
    print(filename, "init start")

    -- Own camera (pixels, origin at the center)
    map_scene_cam = GL.Camera.new()
    map_scene_cam.position = vec3.new(0, 0, 3)
    map_scene_cam.proj_type = GL.Projection.OrthoFixed

    local desk_texture = GL.Texture.new()
    desk_texture:setColor(RGBA.new(52, 70, 82, 255))
    desk = GL.Plane.new(desk_texture)
    -- Covers any window
    scale_when_loaded(desk, vec3.new(20000, 20000, 1))

    map_scene_renderer = GL.PlaneRenderer.new(map_scene_cam, true)
    map_scene_renderer.planes = { desk }
    window:addRenderer(map_scene_renderer)

    -- Added after the background: drawn over it
    map_overlay.open()

    print(filename, "init end")

elseif (is_unloading)
then
    -- At once: not folding back over the next scene
    map_overlay.hide()
    window:removeRenderer(map_scene_renderer)

elseif (is_running)
then
    apply_loaded_scalings()
end
