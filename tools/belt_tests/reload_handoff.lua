-- Exercise production handoff math and the actual slide selection/grab branch.
assert(load(candidate, 'complete_reload'))
local handoff=assert(load(handoffSource, 'slide_handoff'))()
local function extract(source, first, last)
    local begin=assert(source:find(first,1,true),first)
    return source:sub(begin,assert(source:find(last,begin,true),last)-1)
end
local cases=0
local function check(name, fn)
    local ok,err=pcall(fn); assert(ok,name..': '..tostring(err)); cases=cases+1
end
local slideBranch=extract(candidate,
    '            if not gripOf(free) then magS.gripLock, magS.slideHandoff',
    '            if slideS.releasing then')
local pointBranch=extract(candidate,'    local function slideHandPoint(hand)','    -- THE AMMO WATCH.')
local function fixture()
    local f={down=true, point={.15,0,0}, target={.15,0,0}, angle=0}
    local function noop() end
    local env=setmetatable({free=0, magS={state='in',gripLock=true,snapD=-1,
            slideHandoff=handoff.record(0,0,0,0)},
        sc={previewRadius=.05,styleAngleMax=60,grips={overhand={off={},rot={0,0,0,1}}}},
        slideS={}, gWeapon=true, gFx=1,gFy=0,gFz=0,gDx=0,gDy=1,gDz=0,gSx=0,gSy=0,gSz=1,
        spx=0,spy=0,spz=0, SLIDE_HANDOFF=handoff,
        handModel=function() return table.unpack(f.point) end,
        gripOf=function() return f.down end,
        gripTarget=function() return table.unpack(f.target) end,
        VRHandRawRot=function() return {i=0,j=0,k=math.sin(f.angle/2),r=math.cos(f.angle/2)} end,
        basisQuat=function() return 0,0,0,1 end,
        qmul=function(_,_,_,_,i,j,k,r) return i,j,k,r end,
        len3=function(x,y,z) return math.sqrt(x*x+y*y+z*z) end,
        fadeIn=noop,fadeOut=noop,w0IsEmpty=function() return false end,driveSlide=noop,
    }, {__index=_G})
    f.env=env
    f.tick=assert(load(pointBranch..'\nreturn function()\n'..slideBranch..'\nend',
        'production_slide_branch','t',env))()
    function f:run()
        self.env.hx,self.env.hy,self.env.hz=table.unpack(self.point)
        self.tick()
        return self.env.slideS.grabbed==true
    end
    return f
end
check('same squeeze can move from inserted magazine to slide', function()
    local f=fixture(); assert(f:run())
    assert(f.env.magS.gripLock and f.env.magS.slideHandoff==nil and f.env.slideS.hand==0)
end)
check('same squeeze at insertion point cannot jump onto overlapping slide', function()
    local f=fixture(); f.point={.02,0,0}; f.target={.02,0,0}
    assert(not f:run() and f.env.magS.gripLock)
end)
check('one insertion permits only one slide handoff per squeeze', function()
    local f=fixture(); assert(f:run())
    f.env.slideS.grabbed=false
    assert(not f:run() and f.env.magS.gripLock and f.env.magS.slideHandoff==nil)
end)
check('ordinary release and regrip still works at same point', function()
    local f=fixture(); f.point={.02,0,0}; f.target={.02,0,0}; f.down=false
    assert(not f:run() and not f.env.magS.gripLock and f.env.magS.slideHandoff==nil)
    f.down=true; assert(f:run())
end)
check('handoff still requires slide reach', function()
    local f=fixture(); f.point={.30,0,0}; assert(not f:run())
end)
check('handoff still requires recorded grip orientation', function()
    local f=fixture(); f.angle=math.pi; assert(not f:run())
end)
check('nearer removable magazine retains priority', function()
    local f=fixture(); f.point={.16,0,0}; f.env.magS.snapD=.005; assert(not f:run())
end)
check('still held magazine prevents handoff', function()
    local f=fixture(); f.env.magS.state='hand'; assert(not f:run())
end)
check('ejected and gliding magazines cannot enable handoff', function()
    for _,state in ipairs({'gone','glide'}) do
        local f=fixture(); f.env.magS.state=state; assert(not f:run())
    end
end)
check('other hand cannot inherit insertion squeeze', function()
    local f=fixture(); f.env.magS.slideHandoff.hand=1; assert(not f:run())
end)
check('whole weapon translation is not a reach', function()
    local f=fixture(); f.point={2,0,0}; f.target={2,0,0}; f.env.spx=2
    assert(not f:run())
end)
check('whole weapon rotation is not a reach', function()
    local f=fixture(); f.env.magS.slideHandoff=handoff.record(0,.15,0,0)
    f.point={0,.15,0}; f.target={0,.15,0}
    f.env.gFx,f.env.gFy=0,1; f.env.gDx,f.env.gDy=-1,0
    assert(not f:run())
end)
check('physical move with translated rotated weapon still reaches slide', function()
    local f=fixture(); f.point={2,.15,0}; f.target={2,.15,0}; f.env.spx=2
    f.env.gFx,f.env.gFy=0,1; f.env.gDx,f.env.gDy=-1,0
    assert(f:run())
end)
check('invalid hand samples cannot manufacture a handoff', function()
    for _,x in ipairs({0/0,math.huge,-math.huge}) do
        assert(handoff.record(0,x,0,0)==nil)
        assert(not handoff.canTake(handoff.record(0,0,0,0),0,x,0,0,.01,.05))
    end
    assert(not handoff.canTake(handoff.record(0,0,0,0),0,nil,nil,nil,.01,.05))
end)
check('invalid native hand validity is rejected before measuring', function()
    local sample={x=0,y=0,z=0,w=0}
    local env=setmetatable({VRHandRawModel=function() return sample end},{__index=_G})
    local read=assert(load(extract(candidate,'local function handModel(side)','local function len3(')
        ..'\nreturn handModel','production_hand_read','t',env))()
    assert(read(0)==nil); sample.w=1; assert(read(0)==0)
end)
return string.format('Reload slide handoff: %d regression cases passed',cases)
