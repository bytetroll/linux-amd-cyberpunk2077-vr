-- Pure validation of the structs returned by VRBoneModelPos/Rot. Unavailable
-- positions are non-nil userdata with w=0; they must not move the held meshes.
local M = {}

local function finite(v)
    return type(v) == "number" and v == v and v > -math.huge and v < math.huge
end

local function position(p)
    return p ~= nil and p.w == 1 and finite(p.x) and finite(p.y) and finite(p.z)
end

local function rotation(q)
    if q == nil or not (finite(q.i) and finite(q.j) and finite(q.k) and finite(q.r)) then
        return false
    end
    local norm2 = q.i*q.i + q.j*q.j + q.k*q.k + q.r*q.r
    -- Allow ordinary FK rounding drift, without normalizing or changing a
    -- valid pose. A zero or materially non-unit rotation is not usable by the
    -- conjugate-as-inverse calculation.
    return finite(norm2) and math.abs(norm2 - 1) <= 0.02
end

function M.check(wristPosition, wristRotation, anchorPosition, anchorRotation)
    if not position(wristPosition) then return false, "invalid wrist position" end
    if not rotation(wristRotation) then return false, "invalid wrist rotation" end
    if not position(anchorPosition) then return false, "invalid anchor position" end
    if not rotation(anchorRotation) then return false, "invalid anchor rotation" end
    return true
end

return M
