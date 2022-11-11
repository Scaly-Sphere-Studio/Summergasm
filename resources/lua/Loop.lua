local speed = 0.015

if (window:keyIsPressed(GL.KEY_UP))
then
    camera:move( vec3.new(0, speed, 0) )
end
if(window:keyIsPressed(GL.KEY_DOWN))
then
    camera:move( vec3.new(0, -speed, 0) )
end
if(window:keyIsPressed(GL.KEY_LEFT))
then
    camera:move( vec3.new(-speed, 0, 0) )
end
if(window:keyIsPressed(GL.KEY_RIGHT))
then
    camera:move( vec3.new(speed, 0, 0) )
end

water.translation = vec3.new(0, math.cos(math.pi * coeff) / 2, 0)
foam.translation = water.translation
foam.alpha = math.abs(coeff)
wet.alpha = wet.alpha - 0.004

coeff = coeff + 0.005

if (coeff >= 1)
then
    coeff = -1
    wet.translation = water.translation
    wet.alpha = 1
end
