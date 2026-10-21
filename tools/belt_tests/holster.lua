local function fixture()
    local events, calls, slots = {}, {}, {}
    local shared = {[20]=0.1, [21]=1, [22]=1, [23]=0, [49]=0, [69]=0, [163]=0}
    local player = {smoking=false}
    function player:VRSmokeHasCig() return self.smoking end
    for _, name in ipairs({'VRUnequipWeapon','VRHolsterMelee','VREquipPrimaryWeapon',
        'VREquipWeaponSlot2','VREquipWeaponSlot3','VREquipMeleeWeapon','VREquipOneHandedRangedWeapon'}) do
        player[name] = function() calls[#calls+1] = name end
    end
    local env = setmetatable({
        registerForEvent=function(name, fn) events[name]=fn end,
        Game={GetPlayer=function() return player end,
              GetTransactionSystem=function() return {GetItemInSlot=function(_,_,slot) return slots[slot] end} end},
        TweakDBID={new=function(value) return value end}, CName={new=function(value) return value end},
        TDBID={ToStringDEBUG=function(value) return value end},
        WeaponObject={IsMelee=function(id) return id.melee end},
        GetVRSharedSlot=function(slot) return shared[slot] end,
        spdlog={info=function() end},
    }, {__index=_G})
    assert(load(candidate, 'holster', 't', env))()
    events.onInit()
    local function tick(grip)
        shared[49]=grip
        events.onUpdate(1)
    end
    return {player=player, shared=shared, slots=slots, calls=calls, tick=tick}
end

local function item(name, weapon, melee)
    local id = {melee=melee or false, GetTDBID=function() return name end}
    return {IsA=function(_,typeName) return weapon and typeName=='gameweaponObject' end,
            GetItemID=function() return id end}
end

local cases = 0
local function check(name, fn)
    local ok, err = pcall(fn)
    assert(ok, name .. ': ' .. tostring(err))
    cases=cases+1
end
check('right hip holsters without special clothing', function()
    local f=fixture(); f.slots['AttachmentSlots.WeaponRight']=item('Items.QuestGun',true)
    f.tick(1); assert(f.calls[1]=='VRUnequipWeapon'); f.tick(1); assert(#f.calls==1)
end)
check('left carried melee uses game classification', function()
    local f=fixture(); f.slots['AttachmentSlots.WeaponLeft']=item('Items.QuestReward',true,true)
    f.tick(1); assert(f.calls[1]=='VRHolsterMelee')
end)
check('props in hand do not count as weapons', function()
    local f=fixture(); f.slots['AttachmentSlots.WeaponLeft']=item('Items.Magazine',false)
    f.tick(1); assert(#f.calls==0)
end)
check('immersive draw still requires a matching outfit', function()
    local f=fixture(); f.tick(1); assert(#f.calls==0)
    f.slots['OutfitSlots.ThighRight']=item('Items.pistol_holster',false)
    f.tick(0); f.tick(1); assert(f.calls[1]=='VREquipOneHandedRangedWeapon')
end)
check('simple draw selects right slot', function()
    local f=fixture(); f.shared[23]=1; f.tick(1); assert(f.calls[1]=='VREquipWeaponSlot2')
end)
check('invalid distances never select a zone', function()
    local f=fixture(); f.shared[23]=1
    f.shared[20]=nil; f.shared[21]=0/0; f.shared[22]=-1
    f.tick(1); assert(#f.calls==0)
end)
for _, owner in ipairs({'smoking','scanner','wheel'}) do
    check(owner .. ' owns grip without replay on return', function()
        local f=fixture(); f.shared[23]=1
        if owner=='smoking' then f.player.smoking=true
        elseif owner=='scanner' then f.shared[69]=1 else f.shared[163]=1 end
        f.tick(1); assert(#f.calls==0)
        f.player.smoking=false; f.shared[69]=0; f.shared[163]=0
        f.tick(1); assert(#f.calls==0)
        f.tick(0); f.tick(1); assert(#f.calls==1)
    end)
end
return string.format('Holster: %d regression cases passed', cases)
