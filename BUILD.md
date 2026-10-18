# Building the source

This builds the Windows x64 RED4ext plugin on Linux. It does not install into the game, change Proton, or produce a complete playable mod package. The published tree contains source, scripts, and test fixtures; it does not contain game assets, dependency caches, Windows SDK files, or compiled DLLs.

## Linux prerequisites

Use an x86-64 Linux host with:

- CMake 3.24 or newer, Ninja, Git, and Python 3.
- LLVM's `clang-cl`, `lld-link`, `llvm-ml`, and `llvm-rc` on `PATH`.
- GNU's `x86_64-w64-mingw32-as` on `PATH` for the Windows assembly thunks.
- A Microsoft CRT and Windows SDK obtained with [xwin](https://github.com/Jake-Shadle/xwin).
- The Linux executable from [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler/releases), including its accompanying shared libraries.

For example, the compiler and utility packages on Debian/Ubuntu are `cmake ninja-build git python3 clang lld llvm binutils-mingw-w64-x86-64`. Package versions vary by distribution. Install xwin and DXC separately from their upstream releases. DXC must be the executable that runs on the build host, not the Windows `dxc.exe`.

Create the local SDK directory after reviewing the Microsoft license accepted by xwin:

```sh
xwin --accept-license splat --output "$HOME/.xwin-cache/splat"
```

Keep xwin's default case normalization enabled. Windows SDK headers contain case-insensitive includes that need those aliases on Linux. Use an SDK directory path without spaces: this toolchain puts SDK paths into compiler and linker flag strings. The SDK and CRT remain outside this repository and are subject to their own terms.

The clean build was checked with Clang/LLVM 21.1.8, CMake 4.2.3, Ninja 1.13.2, Python 3.14.4, xwin 0.10.0, and DXC 1.9.2609.5. Other versions have not all been tested. The fetched C++ dependencies are pinned; the local compiler, SDK, and DXC versions also affect the resulting binary.

## Build the plugin

From the repository root:

```sh
XWIN_ROOT="$HOME/.xwin-cache/splat" \
DXC="$HOME/tools/dxc/bin/dxc" \
./scripts/build-linux.sh
```

Set `DXC` to your actual Linux DXC path. `BUILD_DIR` defaults to `build-win` and `BUILD_JOBS` to `4`; both can be overridden. Extra script arguments are passed to the CMake configure step.

The equivalent explicit commands are:

```sh
cmake -S . -B build-win -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-linux.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DXWIN_ROOT="$HOME/.xwin-cache/splat" \
  -DCVR_DXC_EXECUTABLE="$HOME/tools/dxc/bin/dxc" \
  -DCVR_NATIVE_FRAMEGEN=OFF
cmake --build build-win --target cyberpunkvrport_stereo --parallel 4
```

The DLL is written to:

```text
build-win/bin/red4ext/plugins/CyberpunkVR_Stereo/CyberpunkVR_Stereo.dll
```

DXC compiles and embeds the included `src/Render/WorldMarkers.hlsl` during the build. No precompiled shader is needed for this target. Native frame generation is disabled by default for this port; the optional `CVR_NATIVE_FRAMEGEN=ON` path is outside the validated Linux build. The optional `xr_probe` target is disabled and its separate diagnostic sources are not part of this export.

## Dependencies and local patch

CMake downloads these public archives into the build directory and checks their SHA-256 hashes. No Git submodule initialization is required.

| Dependency | Pinned revision |
| --- | --- |
| OpenXR-SDK | `288d3a7ebc1ad959f62d51da75baa3d27438c499` (1.0.34) |
| Dear ImGui | `cb16be3a3fc1f9cd146ae24d52b615f8a05fa93d` (1.90.9) |
| MinHook | `d94c64d32ea37bc4f5ee47d580709f70c6fb6080` |
| RED4ext.SDK | `d02a1f329744cd4e39c6f7f3fdf63053731e1aff` from `marklove5102/RED4ext.SDK` |

The small patch in `cmake/patches/red4ext-clang-linux.patch` is applied only to the build-local SDK checkout. It adds the handle conversion and complete-type includes used by this source and corrects `RTTI` include casing. The toolchain enables delayed template parsing for the SDK's MSVC-style headers. The patch step verifies the pinned source before applying and accepts an already-applied patch when reconfiguring.

The OpenXR loader and MinHook are linked statically by default. The plugin still needs the normal Windows/Proton runtime libraries and a working OpenXR runtime. A clean build does not validate a particular Proton, GPU driver, headset, or game version.

## Portable regression tests

These tests run on the Linux host without the game or Windows SDK. Install a host C++ compiler, `clang++`, and the Lua 5.4 shared library (for example `liblua5.4-0` on Debian/Ubuntu), alongside CMake, Ninja, and Python.

```sh
cmake -S tools/portable_tests -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel 4
ctest --test-dir build-host --output-on-failure
```

The tests fetch only the pinned OpenXR headers. To reuse headers already downloaded by the plugin build, add `-DCPVR_TEST_OPENXR_INCLUDE_DIR="$PWD/build-win/_deps/openxr-src/include"` to configure.

The Linux x86-64 suite contains 37 CTest entries covering rendering math, settings-resolution branch replay, layer diagnostics, body rotation, assembly-source parity, and the grenade and quaternion-return regressions. The two belt fixtures also run directly:

```sh
python3 tools/belt_tests/run_grenade_hold.py
python3 tools/belt_tests/run_bone_rotation_abi.py
```

The grenade fixture checks 21 cases, including invalid bone data and recovery into valid holds. The quaternion fixture checks 177 cases against checked-in pre-fix source excerpts. It intentionally runs the old misaligned return-store in an isolated child process and requires that child to fault; core files are disabled for that child. Neither fixture reads Git history or game files. Use a Debug host-test build so the assertion-based C++ checks remain enabled.

Additional older test directories are retained as source reference. Some require Windows/D3D12 or an independently supplied game executable and are not part of the portable suite.

## Source build versus playable package

The compiled DLL alone is insufficient to play. A compatible installation also requires the supporting mod loaders, scripts, configuration, and original VR mod resources described in the project README. In particular, this source export does not include packaged game `.archive` resources, extracted game data, the existing `openvr_api.dll`, or the game-derived replacement shader binaries. Missing optional replacement shaders disable those replacement effects; missing required VR camera/body resources cannot be solved by compiling the DLL.

Use the installation instructions separately from these build commands. This build script does not package, download, or copy those runtime assets.
