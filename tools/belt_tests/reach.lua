-- Execute the production highlight scanner, setup callback and grenade tick
-- with lexical engine mocks. No game, component or filesystem writes occur.
assert(load(candidate, 'complete_belt'))
assert(load(grenadeSource, 'complete_grenade'))
local function extract(source, first, last)
    local begin = assert(source:find(first, 1, true), first)
    return source:sub(begin, assert(source:find(last, begin, true), last) - 1)
end
local scanner = extract(candidate, 'updateHighlight = function(dt, forceSide)', 'local THROW_CFG_VERSION')
local setup = extract(candidate, 'local GRENADE = (function()', '-- THE POINT ON THE HAND')
local grab = extract(grenadeSource, 'updateGrenadeGrab = function(now, dt)', '-- PUT ONE IN THE HAND')
local grenade = 'ucbeltgrenade'
local function fixture()
    local f = {hands={[0]={x=0,y=0,z=0}, [1]={x=2,y=0,z=0}}, centres={},
               enabled={}, eligible={}, groups={}, reads=0, palms=0, probes=0, hips=true,
               down={[0]=false,[1]=false}, attaches={}, releases=0}
    f.cfg={highlight=true, highlight_rate_hz=20, highlight_radius_m=.14, highlight_release=1.35,
           grenade_grab=true, grenade_grab_plane=2}
    f.state={comps={}, hlAcc=0, litHand={}, litOn={}, grab={was={[0]=false,[1]=false}}}
    local env=setmetatable({CFG=f.cfg, S=f.state, PERFORMANCE={legacyHighlightScan=false}, OFFSET_GROUP=f.groups,
        hipsModelFrame=function() if f.hips then return 0,0,0,0,0,0,1 end end,
        palmPoint=function(side) f.palms=f.palms+1; return f.hands[side] end,
        elementCentre=function(host) local c=f.centres[host]; if c then return c.x,c.y,c.z end end,
        markable=function(host) return f.eligible[host] end,
        toHipsFrame=function(x,y,z) return x,y,z end,
        qrot=function(_,_,_,_,x,y,z) return x,y,z end,
        applyLit=function(want) f.state.want=want; f.state.lit=next(want) end,
        reassertDevices=function() f.probes=f.probes+1 end,
        VRPalmModelPos=function() end,
        playerPlane=function() return 2 end,
        require=function(name)
            assert(name=='grenade/grenade')
            return {setup=function(e) f.E=e end}
        end,
    }, {__index=_G})
    f.env=env
    f.tick=assert(load('local updateHighlight\n'..scanner..setup..'\nreturn updateHighlight', 'reach', 't', env))()
    local function noop() end
    local grabEnv=setmetatable({E=f.E, Game={GetPlayer=function() return {} end}, pinS={out=false},
        leverTick=noop, throwSample=noop, throwTick=noop, setTriggerBlock=noop, poseTick=noop,
        updatePin=noop, grenadeHoldPose=noop, trace=noop,
        gripDown=function(side) return f.down[side] end,
        grenadeAttach=function(_,side,host)
            f.attaches[#f.attaches+1]={side=side,host=host}
            f.state.grab.slot='held'; f.state.grab.side=side; f.state.grab.host=host; f.state.grab.plane=2
        end,
        grenadeRelease=function() f.releases=f.releases+1; f.state.grab.slot=nil end,
    }, {__index=_G})
    f.grabEnv=grabEnv
    f.grabTick=assert(load('local updateGrenadeGrab\n'..grab..'\nreturn updateGrenadeGrab','grab','t',grabEnv))()
    function f:add(host,x,group)
        self.centres[host]={x=x,y=0,z=0}; self.enabled[host]=true; self.eligible[host]=true
        self.groups[host]=group
        self.state.comps[host]={IsEnabled=function()
            self.reads=self.reads+1
            if self.enabled[host]=='throw' then error('detached component') end
            return self.enabled[host]
        end}
    end
    return f
end
local cases=0
local function check(name, fn)
    local ok,err=pcall(fn); assert(ok,name..': '..tostring(err)); cases=cases+1
end
check('nearest changed beyond release clears old row', function()
    local f=fixture(); f:add('A',.30); f:add('B',.20); f.state.litHand[0]='A'
    f.tick(.06); assert(f.state.litHand[0]==nil and f.state.lit==nil)
end)
check('old row stays within its own release radius', function()
    local f=fixture(); f:add('A',.18); f:add('B',.16); f.state.litHand[0]='A'
    f.tick(.06); assert(f.state.litHand[0]=='A')
end)
check('new nearest within enter radius still wins', function()
    local f=fixture(); f:add('A',.18); f:add('B',.10); f.state.litHand[0]='A'
    f.tick(.06); assert(f.state.litHand[0]=='B')
end)
check('disabled old row cannot be retained', function()
    local f=fixture(); f:add('A',.10); f:add('B',.20); f.state.litHand[0]='A'; f.enabled.A=false
    f.tick(.06); assert(f.state.litHand[0]==nil)
end)
check('removed old row cannot be retained', function()
    local f=fixture(); f:add('B',.20); f.state.litHand[0]='A'
    f.tick(.06); assert(f.state.litHand[0]==nil)
end)
check('grouped row uses closest enabled member for release', function()
    local f=fixture(); f:add('A1',.30,'A'); f:add('A2',.18,'A'); f:add('B',.16)
    f.state.litHand[0]='A'; f.tick(.06); assert(f.state.litHand[0]=='A')
end)
check('hands select independently and union marks', function()
    local f=fixture(); f:add('A',.10); f:add('B',2.10); f.tick(.06)
    assert(f.state.litHand[0]=='A' and f.state.litHand[1]=='B' and f.state.want.A and f.state.want.B)
end)
check('grip edge rejects cached distant grenade between highlight ticks', function()
    local f=fixture(); f:add(grenade,.10); f.tick(.06); f.hands[0].x=1
    f.tick(.01); assert(f.state.litHand[0]==grenade)
    local acc=f.state.hlAcc; f.down[0]=true; f.grabTick(1,.01)
    assert(#f.attaches==0 and f.state.litHand[0]==nil and f.state.hlAcc==acc)
end)
check('grip edge finds newly reached grenade between highlight ticks', function()
    local f=fixture(); f:add(grenade,.30); f.tick(.06); assert(f.state.litHand[0]==nil)
    f.hands[0].x=.25; f.down[0]=true; f.grabTick(1,.01)
    assert(#f.attaches==1 and f.attaches[1].host==grenade)
    f.grabTick(2,.01); assert(#f.attaches==1)
    f.down[0]=false; f.grabTick(3,.01); assert(f.releases==1)
end)
check('edge chooses current target rather than old target', function()
    local f=fixture(); f:add(grenade,.10); f:add(grenade..'_emp',1.10); f.tick(.06)
    f.hands[0].x=1; f.down[0]=true; f.grabTick(1,.01)
    assert(#f.attaches==1 and f.attaches[1].host==grenade..'_emp')
end)
check('disabled between scans rejects grip', function()
    local f=fixture(); f:add(grenade,.10); f.tick(.06); f.enabled[grenade]=false
    f.down[0]=true; f.grabTick(1,.01); assert(#f.attaches==0)
end)
check('failed proximity edge requires release before retry', function()
    local f=fixture(); f:add(grenade,.30); f.down[0]=true; f.grabTick(1,.01)
    f.hands[0].x=.25; f.grabTick(2,.01); assert(#f.attaches==0)
    f.down[0]=false; f.grabTick(3,.01); f.down[0]=true; f.grabTick(4,.01); assert(#f.attaches==1)
end)
for _,failure in ipairs({'hips','palm','api','disabled','empty','detached','nan','infinite'}) do
    check('grip rejects unavailable reach '..failure, function()
        local f=fixture(); f:add(grenade,.10); f.tick(.06)
        if failure=='hips' then f.hips=false
        elseif failure=='palm' then f.hands[0]=nil
        elseif failure=='api' then f.env.VRPalmModelPos=false
        elseif failure=='disabled' then f.cfg.highlight=false
        elseif failure=='empty' then f.state.comps={}
        elseif failure=='detached' then f.enabled[grenade]='throw'
        elseif failure=='nan' then f.hands[0].x=0/0
        else f.hands[0].x=math.huge end
        f.down[0]=true; f.grabTick(1,.01)
        assert(#f.attaches==0 and f.state.litHand[0]==nil)
    end)
end
check('edge scans one hand without changing cosmetic cadence or probes', function()
    local f=fixture(); f:add(grenade,.10); f:add('mag',2.1); f.cfg.probe_devices=true; f.tick(.06)
    local reads,palms,probes=f.reads,f.palms,f.probes
    f.tick(.01); local acc=f.state.hlAcc
    assert(f.E.grabTarget(0)==grenade)
    assert(f.reads==reads+2 and f.palms==palms+1 and f.probes==probes and f.state.hlAcc==acc)
    assert(f.state.litHand[1]=='mag' and f.state.want.mag)
end)
check('one successful grip prevents a second attachment that frame', function()
    local f=fixture(); f:add(grenade,.10); f.hands[1].x=0; f.down[0]=true; f.down[1]=true
    f.grabTick(1,.01); assert(#f.attaches==1)
end)
check('non-grenade nearest row still blocks grenade selection', function()
    local f=fixture(); f:add(grenade,.10); f:add('mag',.05); f.down[0]=true
    f.grabTick(1,.01); assert(#f.attaches==0 and f.state.litHand[0]=='mag')
end)
check('release stows before same-frame pin pull or button dwell can arm', function()
    local f=fixture(); f:add(grenade,.10); f.down[0]=true; f.grabTick(1,.01)
    local arms,throws=0,0
    f.grabEnv.leverTick=function() arms=arms+1; f.grabEnv.pinS.out=true end
    f.grabEnv.updatePin=f.grabEnv.leverTick
    f.grabEnv.grenadeThrow=function() throws=throws+1; return true end
    f.down[0]=false; f.grabTick(2,.01)
    assert(f.releases==1 and arms==0 and throws==0)
end)
check('already armed grenade still throws when grip opens', function()
    local f=fixture(); f:add(grenade,.10); f.down[0]=true; f.grabTick(1,.01)
    f.grabEnv.pinS.out=true
    local throws=0
    f.grabEnv.grenadeThrow=function() throws=throws+1; return true end
    f.down[0]=false; f.grabTick(2,.01)
    assert(throws==1 and f.releases==0)
end)
check('holding grenade still updates button and pin interactions', function()
    local f=fixture(); f:add(grenade,.10); f.down[0]=true; f.grabTick(1,.01)
    local lever,pin=0,0
    f.grabEnv.leverTick=function() lever=lever+1 end
    f.grabEnv.updatePin=function() pin=pin+1 end
    f.grabTick(2,.01)
    assert(lever==1 and pin==1 and f.releases==0)
end)
check('attach and release discard old button timers and grenade pose frames', function()
    local function noop() end
    local env=setmetatable({E={CFG={grenade_grab_plane=2}, S={grab={},litHand={}},
        propItem=function() return 'item','vrp_gren_emp' end,
        propAllowSlot=noop, playerPlane=function() return 2 end, log=noop},
        HAND_SLOT={[0]='left',[1]='right'}, trace=noop, playSnd=noop,
        poseClear=noop, magnetClear=noop, setTriggerBlock=noop,
        pinS={out=true,itemP={},itemQ={},prev={},grabQ={}}, leverS={armAt=1},
    }, {__index=_G})
    local attach,release=assert(load('local grenadeAttach,grenadeRelease\n'
        ..extract(grenadeSource,'grenadeAttach = function(', '-- THE FREE HAND AND THE PIN.')
        ..'\nreturn grenadeAttach,grenadeRelease','grenade_lifecycle','t',env))()
    local drops=0
    local player={VRPortHoldItem=function() return true end,
                  VRPortDropItem=function() drops=drops+1 end}
    assert(attach(player,0,'pouch',10))
    assert(env.leverS.armAt==nil and env.pinS.itemP==nil and env.pinS.itemQ==nil)
    assert(not env.pinS.out and env.pinS.prev==nil and env.pinS.grabQ==nil)
    env.leverS.armAt=10; env.pinS.itemP={}; env.pinS.itemQ={}
    release(player)
    assert(drops==1 and env.leverS.armAt==nil and env.pinS.itemP==nil and env.pinS.itemQ==nil)
end)
loadstring=load -- The existing scan-parity fixture also runs in LuaJIT/CET.
local parity=assert(load(paritySource, 'highlight_parity'))()(candidate)
return string.format('Belt reach: %d regression cases passed; %d scan-parity comparisons passed (%d vs %d reads)',
    cases,parity.cases,parity.legacy_reads,parity.shared_reads)
