-- A magazine insertion consumes its squeeze. Index users can keep that
-- squeeze while moving to the slide, but staying at the well must not grab
-- another part automatically. Points are in the same weapon-relative frame.
local M = {}
local function finite(n)
    return type(n) == 'number' and n == n and math.abs(n) < math.huge
end

function M.record(hand, x, y, z)
    if (hand ~= 0 and hand ~= 1) or not (finite(x) and finite(y) and finite(z)) then return nil end
    return { hand = hand, x = x, y = y, z = z }
end

function M.canTake(seated, hand, x, y, z, distance, radius)
    if not seated or seated.hand ~= hand then return false end
    if not (finite(x) and finite(y) and finite(z) and finite(distance) and finite(radius)) then
        return false
    end
    if radius <= 0 or distance < 0 or distance > radius then return false end
    local dx, dy, dz = x - seated.x, y - seated.y, z - seated.z
    -- Eight centimetres separates an intentional reach from the insertion's
    -- release motion. The slide still enforces its own position/angle gate.
    return dx * dx + dy * dy + dz * dz >= 0.08 * 0.08
end

return M
