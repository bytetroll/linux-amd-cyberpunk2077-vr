-- CyberpunkVRPort — hand-to-visual-holster equip.
--
-- Outfit clothing items are entGarmentSkinnedMeshComponent — skinned to bones on the GPU, no CPU
-- world transform. We approximate the prop handle as a point offset OUTWARD from the hip bone in
-- the body's lateral direction (plugin publishes the distances). Side is classified by the equipped
-- outfit item names (katana / pistol / holster keywords).
--
-- Plugin (after VRIK FK):
--   shared[20] = right wrist -> right hip prop point  (hip + bodyRight * 0.18m)
--   shared[21] = right wrist -> left  hip prop point  (hip - bodyRight * 0.05m, closer for cross-body reach)
--   shared[22] = right wrist -> back-strap anchor (over right shoulder, for rifle/sniper)
--   shared[49] = right grip binary
--
-- Zone -> equip:
--   B (back, no VH required) -> primary weapon (slot 1, rifle/sniper)
--   L (left hip, VH katana)  -> melee
--   R (right hip, VH pistol) -> one-handed ranged (pistol — not sniper)

local PROX_R           = 0.25
local PROX_L           = 0.40
local PROX_BACK        = 0.25   -- was 0.35: the over-shoulder anchor also caught a raised
                                -- weapon-ready wrist at the waist/chest -> zone B misfires
local BACK_MARGIN      = 0.03   -- B must beat the right-hip distance by a clear margin
local EQUIP_COOLDOWN_S = 0.6

local LEFT_SLOTS = {
    "OutfitSlots.ThighLeft", "OutfitSlots.KneeLeft", "OutfitSlots.AnkleLeft",
}
local RIGHT_SLOTS = {
    "OutfitSlots.ThighRight", "OutfitSlots.KneeRight", "OutfitSlots.AnkleRight",
}
local MELEE_KW  = { "katana", "sword", "blade", "_eqh", "s10_military", "knife", "machete" }
local RANGED_KW = { "pistol", "revolver", "holster" }

local TransactionSystem = nil
local SlotIDs = nil
local WeaponRightSlot = nil
local WeaponLeftSlot = nil
local cache = { L = nil, R = nil }
local sinceScan = 0.0
local rightGripPrev = 0
local cooldown = 0.0
local lastZone = nil

local function classify(name)
    local lc = name:lower()
    for _, kw in ipairs(MELEE_KW)  do if lc:find(kw, 1, true) then return "melee"  end end
    for _, kw in ipairs(RANGED_KW) do if lc:find(kw, 1, true) then return "ranged" end end
    return nil
end

local function classifySide(pl, slots)
    for _, slotName in ipairs(slots) do
        local item = TransactionSystem:GetItemInSlot(pl, SlotIDs[slotName])
        if item then
            local nm = TDBID.ToStringDEBUG(item:GetItemID():GetTDBID())
            local cls = classify(nm)
            if cls then return cls end
        end
    end
    return nil
end

local function rescan(pl)
    cache.L = classifySide(pl, LEFT_SLOTS)
    cache.R = classifySide(pl, RIGHT_SLOTS)
end

local function weaponInSlot(pl, slot)
    local item = TransactionSystem:GetItemInSlot(pl, slot)
    -- WeaponLeft also carries magazines and smoking props. Only weapons count as held.
    if item and item:IsA(CName.new("gameweaponObject")) then return item end
    return nil
end

local function distance(slot)
    local value = GetVRSharedSlot(slot)
    if type(value) ~= 'number' or value ~= value or value < 0 then return math.huge end
    return value
end

registerForEvent('onInit', function()
    TransactionSystem = Game.GetTransactionSystem()
    WeaponRightSlot = TweakDBID.new("AttachmentSlots.WeaponRight")
    WeaponLeftSlot = TweakDBID.new("AttachmentSlots.WeaponLeft")
    SlotIDs = {}
    for _, s in ipairs(LEFT_SLOTS)  do SlotIDs[s] = TweakDBID.new(s) end
    for _, s in ipairs(RIGHT_SLOTS) do SlotIDs[s] = TweakDBID.new(s) end
end)

registerForEvent('onUpdate', function(dt)
    dt = dt or 0.016
    cooldown = math.max(0.0, cooldown - dt)

    if type(GetVRSharedSlot) ~= 'function' then return end
    local pl = Game.GetPlayer()
    if not pl or not TransactionSystem then rightGripPrev = 0; return end

    local g = ((GetVRSharedSlot(49) or 0) > 0.5) and 1 or 0
    local edge = (g == 1 and rightGripPrev == 0)
    rightGripPrev = g
    -- The scanner owns both grips through its release tail. Never replay that squeeze as a draw.
    if (GetVRSharedSlot(69) or 0) > 0.5 then return end

    -- While a cigarette is in the hands it occupies the WeaponRight slot -- which this mod would
    -- read as a held weapon -- and the right grip is what puts the cig in the mouth. Both of those
    -- collide, so suspend every holster gesture until the cig is put away. pcall because
    -- VRSmokeHasCig only exists when the smoking mod is installed.
    local okSmoke, smoking = pcall(function() return pl:VRSmokeHasCig() end)
    if okSmoke and smoking then return end

    -- WHEEL GRAB owns the grip while a hand is at the steering wheel: shared[163] bit0 = the right
    -- hand is there. The grip that grabs the wheel must not also equip a weapon, and the flag is
    -- raised on PROXIMITY -- before the press -- so there is no edge to race.
    local wheelArmed = GetVRSharedSlot(163)
    if wheelArmed and (math.floor(wheelArmed) % 2) == 1 then
        return
    end

    sinceScan = sinceScan + dt
    if sinceScan >= 1.0 then sinceScan = 0.0; rescan(pl) end

    if not edge or cooldown > 0 then return end

    local dR, dL, dB = distance(20), distance(21), distance(22)

    -- Which body zone is the right hand reaching to? (shared by both modes)
    -- B needs a CLEAR win over the right hip: the shoulder anchor sits close to a
    -- raised wrist, and a fuzzy B-vs-R call was swapping weapon slots 1<->2.
    local zone
    if dB < PROX_BACK and (dB + BACK_MARGIN) < math.min(dL, dR) then
        zone = "B"
    elseif dL < PROX_L and dL <= dR then
        zone = "L"
    elseif dR < PROX_R then
        zone = "R"
    end
    if not zone then return end

    -- Reliable "is a weapon in hand": check both the right and left attachment slots.
    -- GetActiveWeapon() can return null / lag for a melee weapon right after an equip (or while it's
    -- in a lowered state), which would wrongly reset lastZone and send us into the RE-EQUIP branch
    -- instead of holstering — exactly the "it equips again instead of putting away" the katana shows.
    -- THE LEFT SLOT IS CHECKED TOO: the weapon carry system moves a heavy weapon (katana, rifle) to
    -- AttachmentSlots.WeaponLeft while the left hand holds it (CyberpunkVR_CarryLeft = 1). A weapon
    -- there is still "in hand" for holster purposes — reaching to the left hip should put it away.
    local rightItem = weaponInSlot(pl, WeaponRightSlot)
    local leftItem  = weaponInSlot(pl, WeaponLeftSlot)
    local hasWeapon = (rightItem ~= nil) or (leftItem ~= nil)
    -- Prefer the right item for classification; if it is nil, the left item is the one in hand.
    local heldItem = rightItem or leftItem

    -- Putting away a held weapon must work even without a matching VisualHolster outfit item.
    -- Clothing classification selects what to DRAW; it must not prevent a right-hip holster.
    if hasWeapon then
        if WeaponObject.IsMelee(heldItem:GetItemID()) then
            pl:VRHolsterMelee()
        else
            pl:VRUnequipWeapon()
        end
        lastZone = nil
        cooldown = EQUIP_COOLDOWN_S
        spdlog.info("[Holster] -> HOLSTER (held weapon), zone=" .. zone)
        return
    end

    -- shared[23]: 0/unset = immersive (classify by visual holster), 1 = simple slot mapping.
    -- (Inverted on the plugin side so the zero-initialised shared block defaults to immersive.)
    local simpleHolsters = ((GetVRSharedSlot(23) or 0) > 0.5)

    if simpleHolsters then
        -- SIMPLE SLOT MODE: visual holsters ignored. Over-shoulder/back = EquipmentSlot1,
        -- right hip = EquipmentSlot2, left hip = EquipmentSlot3.
        if zone == "B" then
            pl:VREquipPrimaryWeapon()
        elseif zone == "R" then
            pl:VREquipWeaponSlot2()
        else
            pl:VREquipWeaponSlot3()
        end
        lastZone = zone
        spdlog.info("[Holster] simple -> equip slot for zone " .. zone)
        cooldown = EQUIP_COOLDOWN_S
        return
    end

    -- IMMERSIVE MODE: equip is chosen by which visual holster the hand reaches.
    rescan(pl)
    local cls
    if zone == "B" then
        cls = "primary"
    elseif zone == "L" then
        cls = cache.L
    else
        cls = cache.R
    end
    if not cls then return end

    spdlog.info(string.format("[Holster] grip zone=%s cls=%s dR=%.2f dL=%.2f dB=%.2f hasWpn=%s",
        tostring(zone), tostring(cls), dR, dL, dB, tostring(hasWeapon)))

    if cls == "melee" then
        pl:VREquipMeleeWeapon(); lastZone = zone
        spdlog.info("[Holster] -> equip melee")
    elseif cls == "primary" then
        pl:VREquipPrimaryWeapon(); lastZone = zone
        spdlog.info("[Holster] -> equip primary")
    else
        pl:VREquipOneHandedRangedWeapon(); lastZone = zone
        spdlog.info("[Holster] -> equip ranged")
    end
    cooldown = EQUIP_COOLDOWN_S
end)
