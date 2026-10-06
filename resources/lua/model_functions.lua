function audio_plane (plane)
    local source = Audio.Source.get(0)
    if (plane:isHovered() and window:clickIsPressed(GL.LEFT_CLICK))
    then
        if (source.is_playing)
        then
            source:pause()
        else
            source:play()
        end
    end
    if (plane:isHovered() and window:clickIsPressed(GL.RIGHT_CLICK))
    then
        source:stop()
        source:play()
    end
end

-- Planes created from an image file are resized to the image's pixel size by
-- GL once the file has loaded (async), overriding any earlier scaling.
-- Register the wanted scaling here, and call apply_loaded_scalings() every
-- frame to re-apply it once the image is loaded.
-- Not a weak table: planes passed by forEach() are temporary userdata that
-- would be collected before being loaded. Entries are removed once applied.
local pending_scalings = {}

function scale_when_loaded (plane, scaling)
    pending_scalings[plane] = scaling
end

function apply_loaded_scalings ()
    for plane, scaling in pairs(pending_scalings) do
        local w, h = plane.texture:getDimensions()
        if (w ~= 0 and h ~= 0)
        then
            -- scaling is either a vec3, or a function(plane, w, h)
            if (type(scaling) == "function")
            then
                scaling(plane, w, h)
            else
                plane.scaling = scaling
            end
            pending_scalings[plane] = nil
        end
    end
end

function drag_plane (plane)
    if (plane:isHeld())
    then
        local x, y = window:getCursorDiff()
        if (x ~= 0 or y ~= 0)
        then
            local w, h = window:getDimensions()
            if (w > h)
            then
                plane:translate(vec3.new(x / (h / 2), y / (h / 2), 0))
            else
                plane:translate(vec3.new(x / (w / 2), y / (w / 2), 0))
            end
        end
    end
end

function drag_plane_fixed (plane)
    if (plane:isHeld())
    then
        local x, y = window:getCursorDiff()
        if (x ~= 0 or y ~= 0)
        then
            plane:translate(vec3.new(x, y, 0))
        end
    end
end

-- Distance at which a perspective camera sees 1 world unit as 1 pixel on the
-- z = 0 plane (zoom excluded)
function perspective_distance (camera, win_h)
    return (win_h / 2) / math.tan(math.rad(camera.fov / 2))
end

-- Scale factor so a plane at depth z keeps its pixel size when seen from a
-- perspective camera at distance D (see perspective_distance, z = 0 is exact)
function depth_scale (D, z)
    return (D - z) / D
end

function move_camera (camera, speed)
    if (window:keyIsHeld(GL.KEY_UP)) then
        camera:move(vec3.new(0, speed, 0))
    end
    if (window:keyIsHeld(GL.KEY_DOWN)) then
        camera:move(vec3.new(0, -speed, 0))
    end
    if (window:keyIsHeld(GL.KEY_LEFT)) then
        camera:move(vec3.new(-speed, 0, 0))
    end
    if (window:keyIsHeld(GL.KEY_RIGHT)) then
        camera:move(vec3.new(speed, 0, 0))
    end
end

function zoom_camera (camera, zoom)
    if (window:keyIsHeld(GL.KEY_KP_ADD)) then
        camera:zoomIn(zoom)
    end
    if (window:keyIsHeld(GL.KEY_KP_SUBTRACT)) then
        camera:zoomOut(zoom)
    end
end