// Pre-fix native function excerpts retained only for isolated regression tests.
// Original CyberpunkVR Port source; see the repository attribution and license.
void VRBoneModelRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    int32_t idx = -1;
    RED4ext::GetParameter(aFrame, &idx);
    aFrame->code++;
    if (!aOut) return;
    aOut->i = 0.0f; aOut->j = 0.0f; aOut->k = 0.0f; aOut->r = 1.0f;
    if (idx < 0 || idx >= g_VRFKSnapCount) return;
    aOut->i = g_VRFKSnapRot[idx][0];
    aOut->j = g_VRFKSnapRot[idx][1];
    aOut->k = g_VRFKSnapRot[idx][2];
    aOut->r = g_VRFKSnapRot[idx][3];
}

void VRHandRawRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    int32_t right = 1;
    RED4ext::GetParameter(aFrame, &right);
    aFrame->code++;
    if (!aOut) return;
    const int side = right ? 1 : 0;
    aOut->i = g_VRHandRawRot[side][0];
    aOut->j = g_VRHandRawRot[side][1];
    aOut->k = g_VRHandRawRot[side][2];
    aOut->r = g_VRHandRawRot[side][3];
}

void VRPalmModelRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    int32_t right = 1;
    RED4ext::GetParameter(aFrame, &right);
    aFrame->code++;
    if (!aOut) return;
    const volatile float* q = right ? g_VRPalmModelRotR : g_VRPalmModelRotL;
    aOut->i = q[0]; aOut->j = q[1]; aOut->k = q[2]; aOut->r = q[3];
}

void VRPlayerEngineRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    aFrame->code++;
    if (!aOut) return;
    aOut->i = CyberpunkVR_PlayerEntityQuat[0];
    aOut->j = CyberpunkVR_PlayerEntityQuat[1];
    aOut->k = CyberpunkVR_PlayerEntityQuat[2];
    aOut->r = CyberpunkVR_PlayerEntityQuat[3];
}

void VRViewWorldRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    aFrame->code++;
    if (aOut) {
        aOut->i = g_VRViewWorldRot[0];
        aOut->j = g_VRViewWorldRot[1];
        aOut->k = g_VRViewWorldRot[2];
        aOut->r = g_VRViewWorldRot[3];
    }
}

void VRSmokeMouthWorldRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    RED4EXT_UNUSED_PARAMETER(aContext); RED4EXT_UNUSED_PARAMETER(a4);
    aFrame->code++;
    if (aOut) {
        aOut->i = g_VRSmokeMouthWorldRot[0];
        aOut->j = g_VRSmokeMouthWorldRot[1];
        aOut->k = g_VRSmokeMouthWorldRot[2];
        aOut->r = g_VRSmokeMouthWorldRot[3];
    }
}

void GetLeftVRHandRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    aFrame->code++; EnsureSharedMemory(); RefreshHandsSnapshot();
    if (aOut) {
        if (g_handsStableValid) {
            aOut->i = SharedPose(4); aOut->j = SharedPose(5); aOut->k = SharedPose(6); aOut->r = SharedPose(7);
        } else { aOut->i = aOut->j = aOut->k = 0.0f; aOut->r = 1.0f; }
    }
}

void GetRightVRHandRot(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, RED4ext::Quaternion* aOut, int64_t a4) {
    aFrame->code++; EnsureSharedMemory(); RefreshHandsSnapshot();
    if (aOut) {
        if (g_handsStableValid) {
            aOut->i = SharedPose(12); aOut->j = SharedPose(13); aOut->k = SharedPose(14); aOut->r = SharedPose(15);
        } else { aOut->i = aOut->j = aOut->k = 0.0f; aOut->r = 1.0f; }
    }
}

