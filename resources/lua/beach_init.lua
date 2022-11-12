print("> beach_init.lua start")

local context = GL.Context.new(window)

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

print("> beach_init.lua end")