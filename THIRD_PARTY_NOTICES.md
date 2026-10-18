# Third-party notices and provenance

This is an independent Linux/AMD port of [CyberpunkVR Port by dariulone](https://github.com/dariulone/cyberpunk-vr-port). The original project and this port's own changes are covered by the [MIT license](LICENSE), with the original 2026 dariulone copyright retained and a separate 2026 Nathan Young copyright for the port's changes. Third-party material keeps the licenses described below.

The original project also credits [iPowerTech](https://github.com/iPowerTech/cyberpunk-vr-port) for driving controls, [dabinn](https://github.com/dabinn/cyberpunk-vr-port) for aiming and renderer work, [Ajson44](https://github.com/Ajson44/cyberpunk-vr-port) for story/camera research, and [tig3rmast3r / OFXR Bridge](https://github.com/tig3rmast3r/OFXR-Bridge) for frame-generation research. These acknowledgments preserve upstream attribution; they do not imply endorsement of this port.

## Dependencies

| Component | License and copyright | Scope and preserved notice |
|---|---|---|
| [im3d](https://github.com/john-chapman/im3d) | MIT; Copyright (c) 2016–2025 John Chapman | Vendored source; [full license](externals/im3d/LICENSE). |
| [RED4ext.SDK](https://github.com/marklove5102/RED4ext.SDK) | MIT; Copyright (c) 2020–present Octavian Dima | [SDK license](licenses/RED4ext-SDK-MIT.txt) and [third-party AMD D3D12MemoryAllocator notice](licenses/RED4ext-SDK-THIRD-PARTY.txt). Preserve the SDK's own license files when fetching or vendoring it. |
| [MinHook](https://github.com/TsudaKageyu/minhook) | BSD-2-Clause; Copyright (C) 2009–2017 Tsuda Kageyu | [Full notice](licenses/MinHook-BSD.txt), including the HDE32/HDE64 notices for Copyright (c) 2008–2009 Vyacheslav Patkov. |
| [NVIDIA Streamline ABI headers](https://github.com/NVIDIA-RTX/Streamline/tree/2122257e0fce486f91b385aa63b9a09b0a34b363/include) | MIT for the included header declarations; Copyright (c) 2022–2024 NVIDIA CORPORATION | [License](externals/streamline/LICENSE) and original per-file notices retained. This covers the included headers, not the entire Streamline SDK or any NVIDIA binary. |
| [NVIDIA Optical Flow interface headers](https://developer.nvidia.com/opticalflow-sdk) | MIT for these headers; Copyright (c) 2018–2023 NVIDIA Corporation (`nvOpticalFlowCommon.h`) and 2020–2023 (`nvOpticalFlowD3D12.h`) | [License](externals/nvidia-optical-flow/LICENSE) and original per-header notices retained. No NVIDIA runtime is included. |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55) | MIT; Copyright (C) 2024 Advanced Micro Devices, Inc. | Optional build dependency; [license](externals/fidelityfx/LICENSE.txt). Native frame generation is disabled for the documented AMD build. |
| [OpenXR SDK release-1.0.34](https://github.com/KhronosGroup/OpenXR-SDK/tree/release-1.0.34) | Apache-2.0 | Build dependency; [license](licenses/OpenXR-SDK-Apache-2.0.txt). Retain the fetched source's notices and its dependency licenses in any redistribution containing those components. |
| [Dear ImGui v1.90.9](https://github.com/ocornut/imgui/tree/v1.90.9) | MIT; Copyright (c) 2014–2024 Omar Cornut | Build dependency; [full license](licenses/Dear-ImGui-MIT.txt). |

Dependency licenses apply independently of the root MIT license. A future binary distribution must include the notices required by every component it incorporates.

## Proton OpenXR source patch

`patches/proton-openxr-layer-allocation.patch` modifies the following files from [ValveSoftware/Proton commit 7a94df49e4695fc101fd47d614166e69c43d9669](https://github.com/ValveSoftware/Proton/tree/7a94df49e4695fc101fd47d614166e69c43d9669). It is supplied as source changes; no Proton or Wine binary is included.

- [`wineopenxr/make_openxr`](https://github.com/ValveSoftware/Proton/blob/7a94df49e4695fc101fd47d614166e69c43d9669/wineopenxr/make_openxr) is **LGPL-2.1-or-later**, as specified by its file header. Copyright 2017–2018 Roderick Colenbrander; Copyright 2022 Jacek Caban for CodeWeavers. The corresponding generator changes are distributed under those same terms. The [complete LGPL 2.1 text](licenses/LGPL-2.1.txt) is included.
- [`wineopenxr/openxr_thunks.c`](https://github.com/ValveSoftware/Proton/blob/7a94df49e4695fc101fd47d614166e69c43d9669/wineopenxr/openxr_thunks.c) is generated from the OpenXR registry and carries **Apache-2.0 OR MIT**, Copyright (c) 2017–2026 The Khronos Group Inc. The MIT option is used for those generated-file portions; the [full Khronos MIT notice](licenses/OpenXR-Registry-MIT.txt) is included.

Copyright (c) 2026 Nathan Young applies to this port's patch changes, subject to the corresponding file licenses above. Those portions are not relicensed solely under the root MIT license. Applying or distributing a modified Proton build does not remove the underlying projects' license obligations.

## External assets and separately installed mods

The source export excludes game archives, meshes, textures, extracted game resources, third-party mod assets, prebuilt runtime libraries and toolchain SDK caches. The root MIT license grants no rights to CD PROJEKT RED's game content or other authors' excluded assets. Obtain required mod assets and dependencies from their original distribution pages, including the [upstream CyberpunkVR Port releases](https://github.com/dariulone/cyberpunk-vr-port/releases).

Optional decompiled game shaders and their compiled DXIL files are also excluded. The project's authored `src/Render/WorldMarkers.hlsl` remains part of the source build. `tools/proton_openxr_hotfix.py` is this port's own MIT-licensed helper; the separate Proton source patch retains the file-specific licenses described above.

Rivarez's [Control lower and holster weapons](https://www.nexusmods.com/cyberpunk2077/mods/11060) is a separate dependency, not bundled here. The original author's permissions prohibit uploading that file elsewhere and require permission for modification or asset use. Accordingly, `ControlLowerAndHolsterWeapons.reds` and the acknowledged derivative `vrport_weapon_up.reds` are excluded from this export. Install the original only from its author's page; this repository does not supply a replacement for the omitted derivative.

Cyberpunk 2077 and its assets remain the property of their respective owners. This community project is not affiliated with or endorsed by CD PROJEKT RED, Valve, AMD, NVIDIA, Khronos, or the upstream contributors.
