-- Pre-fix hold-function excerpts for regression comparison only.
-- Original CyberpunkVR Port source; see repository attribution and license.
qmul4 = function(ax, ay, az, aw, bx, by, bz, bw)
    return aw * bx + ax * bw + ay * bz - az * by,
           aw * by - ax * bz + ay * bw + az * bx,
           aw * bz + ax * by - ay * bx + az * bw,
           aw * bw - ax * bx - ay * by - az * bz
end

qrot4 = function(i, j, k, r, x, y, z)
    local tx, ty, tz = 2.0 * (j * z - k * y), 2.0 * (k * x - i * z), 2.0 * (i * y - j * x)
    return x + r * tx + (j * tz - k * ty),
           y + r * ty + (k * tx - i * tz),
           z + r * tz + (i * ty - j * tx)
end

-- THE SAME TRANSFORM TO EVERY PART
grenadeHoldPose = function()
    local parts = grenadeParts()
    if parts == nil then return end
    local side = E.S.grab.side
    if side == nil then return end

    local wp, wq, ap, aq
    local ok = pcall(function()
        wp = VRBoneModelPos(WRIST_BONE[side]);  wq = VRBoneModelRot(WRIST_BONE[side])
        ap = VRBoneModelPos(ANCHOR_BONE[side]); aq = VRBoneModelRot(ANCHOR_BONE[side])
    end)
    if not (ok and wp and wq and ap and aq) then return end

    -- the hold, in model space
    local HP, HQ = holdNow()
    local hx, hy, hz = qrot4(wq.i, wq.j, wq.k, wq.r, HP[1], HP[2], HP[3])
    hx, hy, hz = wp.x + hx, wp.y + hy, wp.z + hz
    local mi, mj, mk, mr = qmul4(wq.i, wq.j, wq.k, wq.r, HQ[1], HQ[2], HQ[3], HQ[4])
    -- WHERE THE GRENADE ENDED UP, in model space, kept for the pin: it is the only frame both the
    -- recording and the live hand agree on, and it is computed here anyway.
    pinS.itemP = { hx, hy, hz }
    pinS.itemQ = { mi, mj, mk, mr }

    -- ...expressed in the anchor's frame, which is what a component's local transform is
    local li, lj, lk, lr = qmul4(-aq.i, -aq.j, -aq.k, aq.r, mi, mj, mk, mr)
    local lx, ly, lz = qrot4(-aq.i, -aq.j, -aq.k, aq.r, hx - ap.x, hy - ap.y, hz - ap.z)

    -- ...plus whatever the panel is nudging it by, which ships at zero and exists so the measurement
    -- can be corrected on the picture without touching the numbers it came from.
    -- THE NUDGE IS PER TYPE. Neither window of the cutting grenade's take reads right in the hand, and
    -- picking between two wrong numbers is not a method -- so the correction is tuned on the picture,
    -- for the type being held, and cannot disturb the ones that already sit correctly.
    local o = E.CFG.grenade_hold or {}
    local byKind = E.CFG.grenade_hold_kind
    if type(byKind) == "table" and E.S.grab.kind ~= nil and byKind[E.S.grab.kind] ~= nil then
        o = byKind[E.S.grab.kind]
    end
    local ni, nj, nk, nr = E.eulerQuat(o.rx, o.ry, o.rz)
    li, lj, lk, lr = qmul4(li, lj, lk, lr, ni, nj, nk, nr)
    lx, ly, lz = lx + (o.x or 0.0), ly + (o.y or 0.0), lz + (o.z or 0.0)

    -- THE PIN IS NOT WRITTEN HERE ONCE IT IS BEING PULLED. Every other part of the grenade sits on the
    -- one transform; the pin has to leave the hand that holds the body, so while a hand is on it, it
    -- gets a transform of its own -- the same one, walked along the pull direction by however far the
    -- hand has actually travelled. That is the difference between a pin that VANISHES and a pin that
    -- comes out, which is what was asked for.
    local pin = E.S.grab.pin
    local isPin = {}
    if pin ~= nil then for i = 1, #pin do isPin[pin[i]] = true end end
    for i = 1, #parts do
        local c = parts[i]
        if not (isPin[c] and pinS.pull ~= nil) then
            pcall(function()
                c:SetLocalTransform(Vector4.new(lx, ly, lz, 1.0), Quaternion.new(li, lj, lk, lr))
            end)
        end
    end
    if pin ~= nil and pinS.pull ~= nil then
        -- WHERE THE HAND WENT, and nowhere else. The first version walked the pin along a direction
        -- taken from the recorded throw, and it was wrong three ways at once: the pin left upwards
        -- instead of along the pull, it moved further than the hand did, and the hand -- pinned by the
        -- magnet -- stood still while its own pin flew away.
        --
        -- Following the hand's own travel fixes all three by construction. Pull a little and it comes
        -- out a little; push back and it goes back in; let go and it is home. It is a vector, in model
        -- space, turned into the anchor's frame like everything else here.
        -- `pinS.pull` is in the GRENADE's frame; a component's local transform is in the anchor's, so
        -- it goes out through model space -- the item's own rotation first, then back into the anchor.
        local wx, wy, wz = qrot4(mi, mj, mk, mr, pinS.pull[1], pinS.pull[2], pinS.pull[3])
        local ax, ay, az = qrot4(-aq.i, -aq.j, -aq.k, aq.r, wx, wy, wz)
        for i = 1, #pin do
            local c = pin[i]
            pcall(function()
                c:SetLocalTransform(Vector4.new(lx + ax, ly + ay, lz + az, 1.0),
                                    Quaternion.new(li, lj, lk, lr))
            end)
        end
    end
end

loadPose = function() end
