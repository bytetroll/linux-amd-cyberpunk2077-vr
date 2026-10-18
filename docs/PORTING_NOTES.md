# Porting notes

## Implemented corrections

- **Layer allocation:** the tested Proton OpenXR conversion used one element size for heterogeneous pointer arrays. The supplied source patch sizes each non-null element separately. The separately guarded binary workaround is narrower and applies only to the verified projection-first Win64 case.
- **Eye geometry:** the engine's resize hook must change the requested resolution without pretending that the active buffers already match it. The trampoline also preserves the original width/height branch behavior. This avoids mismatched MAIN/VRCAM image dimensions and severe eye-to-eye crop differences.
- **Native return alignment:** `RED4ext::Quaternion` requests 16-byte alignment, while the tested CET scratch return allocator can supply 8-byte-aligned storage on Windows. An optimized aligned store faulted in `VRBoneModelRot`. All eight native quaternion readers now write the same four floats using alignment-safe scalar stores. The regression fixture reproduces the old fault and checks all readers without running the game.
- **Grenade validation:** unavailable bone positions are non-null structs with `w=0`. Held-item transforms reject those values, non-finite coordinates and unusable quaternion norms. Valid pose arithmetic is preserved.
- **Menu input:** XInput import patching starts before the delayed engine-hook pass. In native menus the right VR trigger produces gamepad A after a release, with hysteresis and suppression of a held press when returning to gameplay. Existing face buttons and physical gamepad input remain available.
- **Body recovery:** a brief same-origin tracking interruption no longer discards an already-active roomscale catch-up. Recovery pauses velocity and accepts no unknown travel. Long gaps, explicit suspension, recentering and tracking discontinuities still cancel the request.
- **Runtime queue:** on Wine/SteamVR, the OpenXR session uses a separate D3D12 DIRECT queue. Rendering and capture remain on the game queue. Successful image waits signal from the runtime queue to the game queue; releases wait on the exact game-copy fence in the reverse direction. HUD preparation precedes eye release, and submission failures stop further image handoffs. Headset performance validation is pending.

## Open performance work

Direct per-call measurements put roughly 86% of the slow sampled XR cycles inside the two eye swapchain releases plus the HUD release. The tested Proton D3D12 bridge drains its submission queue at release. The game and XR copies currently share that queue. This explains a submission bottleneck; it does not by itself measure scene-rendering cost or guarantee a particular queue redesign's performance.

Moving the image-copy commands themselves to a dedicated queue would also require exact capture-producer fences and retention of capture-buffer read leases until the consumer finishes. The current change keeps those readers and writers on the game queue and isolates only the runtime queue. It retains the runtime's drain and explicitly transfers image availability in both directions. The game submission mutex is not held across runtime release calls.

Remaining symptoms include SteamVR fade/void on rapid movement and occasional body desynchronization. Stale body tracking can coincide with long submission cycles, but a fixed head/body transform defect has not been demonstrated.

## Publication scope

This repository starts with a curated source snapshot. It contains no private development history, local SDK downloads, build outputs, player configuration dumps, crash logs, saves, screenshots or extracted game archives. Required dependencies are obtained from their public upstream sources. See the build guide and third-party notices for the components intentionally supplied separately.
