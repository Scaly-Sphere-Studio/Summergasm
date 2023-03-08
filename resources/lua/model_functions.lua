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
