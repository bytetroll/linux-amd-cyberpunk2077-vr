# Porting notes

## Implemented corrections

- **Layer allocation:** the tested Proton OpenXR conversion used one element size for heterogeneous pointer arrays. The supplied source patch sizes each non-null element separately. The separately guarded binary workaround is narrower and applies only to the verified projection-first Win64 case.
- **Eye geometry:** the engine's resize hook must change the requested resolution without pretending that the active buffers already match it. The trampoline also preserves the original width/height branch behavior. This avoids mismatched MAIN/VRCAM image dimensions and severe eye-to-eye crop differences.
- **Native return alignment:** `RED4ext::Quaternion` requests 16-byte alignment, while the tested CET scratch return allocator can supply 8-byte-aligned storage on Windows. An optimized aligned store faulted in `VRBoneModelRot`. All eight native quaternion readers now write the same four floats using alignment-safe scalar stores. The regression fixture reproduces the old fault and checks all readers without running the game.
- **Grenade validation:** unavailable bone positions are non-null structs with `w=0`. Held-item transforms reject those values, non-finite coordinates and unusable quaternion norms. Valid pose arithmetic is preserved.

## Open performance work

Direct per-call measurements put roughly 86% of the slow sampled XR cycles inside the two eye swapchain releases plus the HUD release. The tested Proton D3D12 bridge drains its submission queue at release. The game and XR copies currently share that queue. This explains a submission bottleneck; it does not by itself measure scene-rendering cost or guarantee a particular queue redesign's performance.

A dedicated XR queue requires exact producer fence/value provenance, protection against capture-buffer overwrite until the XR consumer finishes, correct HUD source selection, and a review of frame-generation, depth, overlay and shutdown paths. Merely switching queues, bypassing the runtime drain, or holding the existing global image-submission mutex across a drain can violate ordering or introduce deadlocks. None of those shortcuts is included here.

Remaining symptoms include SteamVR fade/void on rapid movement and occasional body desynchronization. Stale body tracking can coincide with long submission cycles, but a fixed head/body transform defect has not been demonstrated.

## Publication scope

This repository starts with a curated source snapshot. It contains no private development history, local SDK downloads, build outputs, player configuration dumps, crash logs, saves, screenshots or extracted game archives. Required dependencies are obtained from their public upstream sources. See the build guide and third-party notices for the components intentionally supplied separately.
