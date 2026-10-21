-- Exercise the actual carry function and actual holster callback together.
local declarations = assert(weapon:match('(local CARRY_LEFT.-)local function updateMuzzle'))
local carry = assert(weapon:match('(local function carryTick%(%s*dt%s*%).-)\nregisterForEvent'))
local RIGHT, LEFT = 'AttachmentSlots.WeaponRight', 'AttachmentSlots.WeaponLeft'
local function fixture()
    local slots, added, flags, events = {}, {}, {}, {}
    local shared = {[20]=.1,[21]=1,[22]=1,[23]=0,[49]=0,[69]=0,[155]=0,[163]=0}
    local id = {GetTDBID=function() return 'Items.Rifle' end}
    local item = {GetItemID=function() return id end,
        IsA=function(_,name) return name=='gameweaponObject' end,
        GetWorldPosition=function() return {x=0,y=0,z=0} end,
        GetWorldOrientation=function() return {i=0,j=0,k=0,r=1} end}
    local player = {VRSmokeHasCig=function() return false end}
    function player:VRUnequipWeapon() slots[LEFT]=nil;slots[RIGHT]=nil end
    player.VRHolsterMelee = player.VRUnequipWeapon
    local ts = {
        GetItemInSlot=function(_,_,slot) return slots[slot] end,
        RemoveItemFromSlot=function(_,_,slot) slots[slot]=nil;return true end,
        AddItemToSlot=function(_,_,slot,itemID,_,entity)
            added[#added+1]={slot=slot,id=itemID,entity=entity}
            slots[slot]=entity or item -- models the cached-ID recreation bug
            return true
        end,
    }
    local env = setmetatable({
        Game={GetPlayer=function() return player end,GetTransactionSystem=function() return ts end},
        TweakDBID={new=function(value) return value end},CName={new=function(value) return value end},
        TDBID={ToStringDEBUG=function(value) return value end},
        WeaponObject={IsMelee=function() return false end},
        GetVRSharedSlot=function(slot) return shared[slot] end,
        SetVRCarryLeft=function(flag) flags[#flags+1]=flag end,
        GetVRCarryNear=function() return 1 end,
        VRP_lastClass=2,
        slotGetRel=function() return {x=.01,y=0,z=0} end,
        slotSetRel=function() return true end,
        slotFrame=function() return {ox=0,oy=0,oz=0,xx=1,xy=0,xz=0,yx=0,yy=1,yz=0,zx=0,zy=0,zz=1} end,
        quatFromBasis=function() return 0,0,0,1 end,
        logAlways=function() end,spdlog={info=function() end},
        registerForEvent=function(name,fn) events[name]=fn end,
    },{__index=_G})
    local source=declarations..carry..[[
        return {tick=carryTick,seed=function(id)
            carryOn=true;carryItemID=id;carryLeftSlot=SLOT_LEFT_STOCK
        end,state=function() return carryOn,carryItemID end}
    ]]
    local api=assert(load(source,'actual carryTick','t',env))()
    assert(load(holster,'actual holster','t',env))();events.onInit()
    return {slots=slots,added=added,flags=flags,shared=shared,item=item,id=id,api=api,
        holster=events.onUpdate,env=env,seed=function() slots[LEFT]=item;api.seed(id) end}
end
local cases=0
local function check(name,fn)
    local ok,err=pcall(fn)
    assert(ok,name..': '..tostring(err));cases=cases+1
end
check('holster first cannot resurrect the removed left weapon',function()
    local f=fixture();f.seed();f.shared[49]=1
    f.holster(1);f.api.tick(.016)
    assert(#f.added==0 and f.slots[LEFT]==nil and f.slots[RIGHT]==nil)
    local active,id=f.api.state();assert(not active and id==nil and f.flags[#f.flags]==0)
end)
check('carry first still lets holster remove the returned weapon',function()
    local f=fixture();f.seed();f.shared[49]=1
    f.api.tick(.016);f.holster(1)
    assert(#f.added==1 and f.added[1].entity==f.item)
    assert(f.slots[LEFT]==nil and f.slots[RIGHT]==nil)
end)
for _,grip in ipairs({0,1}) do
    check('external removal resets carry with grip '..grip,function()
        local f=fixture();f.seed();f.slots[LEFT]=nil;f.shared[49]=grip
        f.api.tick(.016)
        local active,id=f.api.state();assert(not active and id==nil)
        assert(#f.added==0 and f.flags[#f.flags]==0)
    end)
end
check('normal return keeps the existing entity',function()
    local f=fixture();f.seed();f.shared[49]=1;f.api.tick(.016)
    assert(#f.added==1 and f.added[1].entity==f.item and f.slots[RIGHT]==f.item)
    f.api.tick(.016);assert(#f.added==1)
end)
check('normal two-hand transfer keeps the existing entity',function()
    local f=fixture();f.slots[RIGHT]=f.item;f.shared[49]=1;f.shared[155]=1
    f.api.tick(.016)
    assert(#f.added==1 and f.added[1].slot==LEFT and f.added[1].entity==f.item)
    assert(f.slots[LEFT]==f.item and f.slots[RIGHT]==nil)
end)
check('scanner blocks carry without replaying the held grip',function()
    local f=fixture();f.seed();f.shared[49]=1;f.shared[69]=1
    f.api.tick(.016);assert(#f.added==0)
    f.shared[69]=0;f.api.tick(.016);assert(#f.added==0)
    f.shared[49]=0;f.api.tick(.016);f.shared[49]=1;f.api.tick(.016)
    assert(#f.added==1 and f.added[1].entity==f.item)
end)
check('scanner blocks initial left transfer',function()
    local f=fixture();f.slots[RIGHT]=f.item;f.shared[49]=1;f.shared[155]=1;f.shared[69]=1
    f.api.tick(.016);assert(#f.added==0 and f.slots[RIGHT]==f.item)
end)
check('scanner still permits cleanup after external holster',function()
    local f=fixture();f.seed();f.slots[LEFT]=nil;f.shared[49]=1;f.shared[69]=1
    f.api.tick(.016);assert(not f.api.state() and #f.added==0 and f.flags[#f.flags]==0)
end)
return string.format('Weapon carry: %d regression cases passed',cases)
