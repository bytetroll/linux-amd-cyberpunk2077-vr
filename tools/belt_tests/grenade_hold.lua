-- Run the actual hold function with lexical mocks. The runner supplies source
-- strings candidate/reference/validitySource; no game or engine is accessed.
local compile = loadstring or load
assert(compile(candidate)) -- Also checks the full module's local-variable limit.
local validity = assert(compile(validitySource))()
local function extract(source, first, last)
    local a = assert(source:find(first, 1, true))
    local b = assert(source:find(last, a, true))
    return source:sub(a, b - 1)
end
local function factory(source)
    return assert(compile('return function(E,pinS,grenadeParts,VRBoneModelPos,VRBoneModelRot,holdNow,Vector4,Quaternion,trace,HOLD_POSE_VALIDITY)\n'
        .. 'local WRIST_BONE={[0]=23,[1]=24};local ANCHOR_BONE={[0]=26,[1]=28}\n'
        .. 'local qmul4,qrot4,grenadeHoldPose\n'
        .. extract(source, 'qmul4 = function(', '-- THE SAME TRANSFORM TO EVERY PART')
        .. extract(source, 'grenadeHoldPose = function()', 'loadPose = function(')
        .. '\nreturn grenadeHoldPose\nend'))()
end
local current, previous = factory(candidate), factory(reference)
local function scenario(which, changed, options)
    options = options or {}
    local bones = {
        wp={x=0.6,y=-0.3,z=1.1,w=1}, ap={x=0.2,y=0.1,z=0.9,w=1},
        wq={i=0,j=0,k=math.sin(0.2),r=math.cos(0.2)},
        aq={i=math.sin(0.1),j=0,k=0,r=math.cos(0.1)},
    }
    if changed then changed(bones) end
    local calls, logs, reads, parts = {}, {}, {}, {}
    local E={CFG={grab_trace=options.trace,grenade_hold={x=0.01,y=-0.02,z=0.03}},
             S={grab={side=1,partNames={},partTypes={}}},eulerQuat=function() return 0,0,0,1 end}
    for i=1,3 do
        parts[i]={SetLocalTransform=function(_,p,q)
            calls[#calls+1]={i,p.x,p.y,p.z,p.w,q.i,q.j,q.k,q.r}
            if options.errorPart == i then error('fixture detached component') end
        end}
        E.S.grab.partNames[parts[i]]='cached-'..i
        E.S.grab.partTypes[parts[i]]='entMeshComponent'
    end
    E.S.grab.pin={parts[3]}
    local pin={itemP={'old'},itemQ={'old'},pull=options.pull and {0.01,0.02,0.03} or nil}
    local function pos(idx)
        reads[#reads+1]='pos'..idx
        if options.readError then error('fixture read failure') end
        if idx==24 then return bones.wp end
        return bones.ap
    end
    local function rot(idx) reads[#reads+1]='rot'..idx;return idx==24 and bones.wq or bones.aq end
    local tick=which(E,pin,function() return parts end,pos,rot,
        function() return {0.1,0.2,0.3},{0,math.sin(0.05),0,math.cos(0.05)} end,
        {new=function(x,y,z,w) return {x=x,y=y,z=z,w=w} end},
        {new=function(i,j,k,r) return {i=i,j=j,k=k,r=r} end},
        function(fmt,...) logs[#logs+1]=string.format(fmt,...) end,validity)
    for frame=1,(options.ticks or 1) do
        if options.beforeTick then options.beforeTick(bones,frame) end
        tick()
    end
    return {calls=calls,logs=logs,reads=reads,pin=pin,state=E.S.grab}
end
local checks=0
local function equal(a,b)
    assert(#a.calls==#b.calls)
    for i=1,#a.calls do for j=1,#a.calls[i] do
        assert(a.calls[i][j]==b.calls[i][j], 'valid transform changed')
    end end
    assert(table.concat(a.reads,',')==table.concat(b.reads,','),'bone read order changed')
    checks=checks+1
end
for _,pull in ipairs({false,true}) do
    for _,trace in ipairs({false,true}) do
        equal(scenario(current,nil,{pull=pull,trace=trace}),scenario(previous,nil,{pull=pull,trace=trace}))
    end
end
local invalid={
    function(b) b.wp.w=0 end,
    function(b) b.ap.w=0 end,
    function(b) b.wp.x=0/0 end,
    function(b) b.ap.z=math.huge end,
    function(b) b.wq={i=0,j=0,k=0,r=0} end,
    function(b) b.aq={i=0,j=0,k=0,r=0} end,
    function(b) b.wq.r=0/0 end,
    function(b) b.aq.i=-math.huge end,
    function(b) b.wq.r=2 end,
    function(b) b.wp=nil end,
}
for _,mutate in ipairs(invalid) do
    local result=scenario(current,mutate,{trace=true})
    assert(#result.calls==0,'invalid bone wrote a component transform')
    assert(result.pin.itemP==nil and result.pin.itemQ==nil,'stale pin frame survived')
    assert(result.logs[#result.logs]:find('skipped:',1,true),'invalid reason not recorded')
    checks=checks+1
end
local failed=scenario(current,nil,{readError=true,trace=true})
assert(#failed.calls==0 and failed.pin.itemP==nil)
assert(failed.logs[1]:find('wrist position begin',1,true))
assert(failed.logs[#failed.logs]:find('skipped:',1,true))
local three=scenario(current,nil,{trace=true,ticks=3})
local many=scenario(current,nil,{trace=true,ticks=12})
assert(#many.logs==#three.logs and many.state.holdTraceAttempts==3 and many.state.holdTraceValidPasses==3,
       'first-hold trace exceeded bound')
assert(#many.calls==36,'trace bound stopped normal transforms')
assert(#scenario(current,nil,{trace=false,ticks=4}).logs==0,'trace logged while disabled')
assert(table.concat(three.logs,'\n'):find('name=cached-1 type=entMeshComponent',1,true),'cached identity absent')
local errors=scenario(current,nil,{trace=true,errorPart=2})
assert(#errors.calls==3,'existing per-component error isolation changed')
assert(table.concat(errors.logs,'\n'):find('transform end part=2 ok=false',1,true),'component failure not traced')
local recover=scenario(current,nil,{trace=true,ticks=20,beforeTick=function(b,frame)
    b.wp.w=frame<=8 and 0 or 1
end})
assert(#recover.calls==36,'recovered FK did not resume normal transforms')
assert(recover.state.holdTraceAttempts==3 and recover.state.holdTraceValidPasses==3,
       'invalid reads consumed the valid-pass trace budget')
local joined=table.concat(recover.logs,'\n')
assert(joined:find('hold valid[1] wrist p=',1,true),'recovered valid bones were not recorded')
assert(joined:find('hold valid[3] transform end part=3 ok=true',1,true),'third valid write not traced')
assert(not joined:find('hold valid[4]',1,true),'valid trace exceeded bound after recovery')
local invalidMany=scenario(current,invalid[1],{trace=true,ticks=20})
local invalidThree=scenario(current,invalid[1],{trace=true,ticks=3})
assert(#invalidMany.logs==#invalidThree.logs,'persistent invalid FK exceeded trace bound')
checks=checks+7
return 'PASS: '..checks..' grenade hold cases; valid transforms identical, invalid transforms suppressed, trace bounded'
