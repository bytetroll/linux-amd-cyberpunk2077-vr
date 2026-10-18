# Linux AMD Cyberpunk 2077 VR

An experimental Linux/AMD port of [dariulone's Cyberpunk VR mod](https://github.com/dariulone/cyberpunk-vr-port), maintained by **Nathan Young (Bytetroll)**.

This is a playable development candidate for Cyberpunk 2077 running through Proton with SteamVR/OpenXR. The native mod is still a Windows RED4ext plugin; it is cross-compiled on Linux. The original stereo rendering, motion controls, avatar system and gameplay integrations are the upstream author's work.

## Status

Tested with Cyberpunk 2077 **2.31**, a **Valve Index**, **Radeon 8060S / RADV**, Proton Experimental and native Linux SteamVR. A longer gameplay retest, including the right-hand grenade grab that previously crashed, completed without that crash. This is limited hardware testing, not a claim of broad headset or GPU compatibility.

Included Linux/AMD work:

- Correct engine render-target resizing so both eye images use matching dimensions and projection geometry.
- Alignment-safe native quaternion returns for the CET bridge, fixing a reproducible fault in the grenade wrist-rotation read.
- Validation of unavailable/non-finite grenade bone poses before item transforms.
- Focused OpenXR frame-phase, layer and body-pose diagnostics.
- A version-guarded Proton OpenXR workaround and a source patch for mixed projection/quad layer allocation.
- A Linux cross-build and focused regression tests.

**Remaining issues:** uneven VR frame delivery, SteamVR void/fade during fast head turns, and some body/head desynchronization. Measurements attribute most long submission cycles to swapchain-release waits on the shared game queue. Queue isolation is not implemented in this candidate. Native frame generation is disabled for the Linux/AMD build; ray/path tracing and other hardware combinations are not validated here.

## Getting started

This repository distributes source, not a complete game-mod bundle. Game-derived archives, extracted game assets, proprietary SDK packages and restricted third-party scripts are excluded.

1. Obtain the original mod's [0.1.7 release and installation instructions](https://github.com/dariulone/cyberpunk-vr-port/releases/tag/0.1.7), together with its required dependencies and assets from their authors. See [third-party notices](THIRD_PARTY_NOTICES.md) for attribution and separately licensed components.
2. [Build the Linux/AMD plugin](BUILD.md). Keep the upstream archive assets and required sight-shader files from your legitimate mod installation.
3. With the game closed, back up the current plugin **outside** the game's plugin directories. Replace `red4ext/plugins/CyberpunkVR_Stereo/CyberpunkVR_Stereo.dll` with your build. Keep a single active VR plugin; retired `CyberpunkVR_Hands.dll` or backup DLLs in plugin folders can be loaded alongside it.
4. Copy the two files from `mods/cet/CyberpunkVRPort_TacticalBelt/grenade/` named `grenade.lua` and `hold_pose_validity.lua` to the corresponding installed CET mod directory. Preserve your player configuration. The other source modules are available for development; do not blindly replace an unrelated mod setup.
5. Follow the [Linux runtime notes](docs/LINUX_RUNTIME.md), including the version-specific Proton workaround. Start SteamVR before the game.
6. Stand upright facing forward and use **F7** to recenter. **F10** opens the VR overlay; holding both stick clicks also opens it. See the [upstream controls](https://github.com/dariulone/cyberpunk-vr-port#controls) for gameplay bindings.

A newly built DLL alone does not replace the upstream archives, dependencies, runtime setup or CET files. Existing game/mod installations should be backed up before testing.

## Development and reporting

[BUILD.md](BUILD.md) covers dependencies, compilation and tests. The new memory-alignment and grenade-pose regressions run without launching the game. [PORTING_NOTES.md](docs/PORTING_NOTES.md) records the implemented fixes and the remaining performance work.

Report reproducible problems through [GitHub Issues](https://github.com/Bytetroll/linux-amd-cyberpunk2077-vr/issues), including game/mod versions, GPU, headset and runtime. Review logs before attaching them: runtime logs may contain local paths or identifiers. Do not upload saves, credentials, crash-memory dumps or extracted game assets.

## Credits and license

Original mod: **[dariulone](https://github.com/dariulone/cyberpunk-vr-port)**. Linux/AMD port contributions: **Nathan Young**. Additional upstream contributors and dependency authors are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The mod's MIT-covered code and this port's contributions use the [MIT License](LICENSE), preserving the original copyright notice. Third-party components and the Proton source patch retain their own notices and licenses. Cyberpunk 2077 and its game assets belong to their respective owners. This community project is not affiliated with CD PROJEKT RED, Valve or AMD.
