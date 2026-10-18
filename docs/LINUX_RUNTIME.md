# Linux runtime notes

## Tested path

The candidate was tested with Cyberpunk 2077 2.31, Proton Experimental, native Linux SteamVR, a Valve Index, Radeon 8060S/RADV, and ray/path tracing and frame generation disabled. It uses the Windows D3D12/OpenXR bridge through Proton; it is not a native Linux game binary.

Install the upstream mod's dependencies first, including its RED4ext/CET/redscript requirements and their Proton setup. Follow each dependency author's instructions. This source repository does not bundle those runtime installers or game archives.

## Proton mixed-layer workaround

The tested Proton module allocated a heterogeneous layer array using the first layer's size. A projection layer followed by a larger HUD quad could therefore overwrite memory before SteamVR received it. Correct game-side poses alone did not solve that runtime defect.

`tools/proton_openxr_hotfix.py` is a reversible workaround for **one exact x86-64 module build**. It covers Win64 projection-first layer arrays, not every possible layer ordering or ABI. The broader generator/source correction is in `patches/proton-openxr-layer-allocation.patch`; see its third-party license notices.

Find the `wineopenxr.so` inside the Proton installation actually used by the game. The tool requires an explicit path and defaults to inspection:

```sh
python3 tools/proton_openxr_hotfix.py status --module "/path/to/Proton - Experimental/files/lib/wine/x86_64-unix/wineopenxr.so"
```

Only if it reports the recognized original module, close the game and other clients using that Proton module, then run:

```sh
python3 tools/proton_openxr_hotfix.py apply --module "/path/to/Proton - Experimental/files/lib/wine/x86_64-unix/wineopenxr.so"
```

The script checks the full original SHA256, instruction bytes and patched SHA256; saves and verifies an original backup; checks for active clients; and replaces the file atomically. It refuses unknown builds. It stores the backup in your XDG state directory by default, outside this checkout; `--backup` can select another location. It does not download or distribute Proton binaries.

To restore that exact original after closing the game:

```sh
python3 tools/proton_openxr_hotfix.py restore --module "/path/to/Proton - Experimental/files/lib/wine/x86_64-unix/wineopenxr.so"
```

Steam updates may replace the module. Re-run `status` after an update. **Do not bypass the version guards or apply the byte offset to a different build.** A newer build may already fix the underlying problem and needs separate verification.

## Controller menus and runtime queue

The native menu uses gamepad A for selection and loading-screen skips. The Index's right A button retains that behavior; the right trigger also confirms while native menu mode is active. Release the trigger before selecting. A trigger held through a menu transition is suppressed until release so it cannot immediately fire a weapon. This uses the game's existing cursor/navigation path; it does not add a tracked-controller pointer to the native menu.

The Linux quality pass binds OpenXR to a separate runtime queue when both Wine and SteamVR are detected. Capture and rendering stay on the game queue, with explicit fence handoffs before image writes and releases. This follows the availability requirements of [OpenXR's D3D12 binding](https://registry.khronos.org/OpenXR/specs/1.1-khr/html/xrspec.html#XR_KHR_D3D12_enable). Its performance benefit still requires measurement in the headset.

The startup log reports `[xrqueue] mode=dedicated-runtime` when active. For a controlled comparison, use this Steam launch option to select the previous shared-queue path, then restart the game:

```text
CVR_XR_SHARED_QUEUE=1 %command%
```

Remove that environment assignment to return to automatic selection. Preserve any other existing launch options. Native Windows and other OpenXR runtimes retain the shared-queue path by default. If dedicated-queue creation fails, startup falls back to the shared queue; a later synchronization failure stops layer submissions and requires a game restart.

## SteamVR settings used in testing

The tested setup used these two keys in SteamVR's `steamvr` configuration section:

```json
{
  "enableLinuxVulkanAsync": true,
  "disableLinuxWaitForPresent": true
}
```

They are SteamVR-wide settings, not per-game graphics settings. Record the existing values before changing them. If editing `steamvr.vrsettings` manually, close SteamVR first, back up the file, and merge only these two keys into the existing `steamvr` object. Do not replace the whole file with this snippet. Restart SteamVR after changes; restore the prior values or prior absence to undo them.

Asynchronous reprojection improved head-turn behavior in testing. The present-wait fallback removed repeated `WaitForPendingPresent` errors and reduced dropped compositor scans, but neither eliminated all image-submission stalls. These are tested settings for this setup, not universal recommendations for every SteamVR version or GPU.

Relevant Valve Linux issue discussions: [asynchronous support](https://github.com/ValveSoftware/SteamVR-for-Linux/issues/886#issuecomment-4300955176), [present-wait fallback](https://github.com/ValveSoftware/SteamVR-for-Linux/issues/948#issuecomment-5702019959).

## Body alignment

Recenter while upright, stationary and facing forward. Hybrid body rotation intentionally allows some head/body separation and changes its torso inference while looking down. Small offsets should be checked after recentering before applying manual camera trims. F10 exposes the mod's controls.

## Diagnostics

The mod emits bounded layer, frame-phase and body logs without enabling the old broad DEBUG tracing. `grab_trace` is an optional CET belt setting for first-hold breadcrumbs. Submission timings measure CPU wall time, including queue waits; they are not isolated GPU rendering costs. Review and sanitize runtime logs before sharing them publicly.
