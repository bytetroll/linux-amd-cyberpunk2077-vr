#!/bin/sh
# Build locally; this script does not install files into the game or Proton.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${BUILD_DIR:-"$repo/build-win"}
sdk=${XWIN_ROOT:-"$HOME/.xwin-cache/splat"}
dxc=${DXC:-}
if [ -z "$dxc" ]; then dxc=$(command -v dxc || true); fi
if [ ! -d "$sdk/crt/include" ] || [ ! -d "$sdk/sdk/include" ]; then
    echo "Missing xwin SDK at $sdk; see BUILD.md." >&2
    exit 1
fi
if [ -z "$dxc" ] || [ ! -x "$dxc" ]; then
    echo "Set DXC to the executable from a Linux DirectX Shader Compiler release." >&2
    exit 1
fi
for tool in cmake ninja git python3 clang-cl lld-link llvm-ml llvm-rc x86_64-w64-mingw32-as; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing build tool: $tool (see BUILD.md)." >&2
        exit 1
    fi
done
cmake -S "$repo" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$repo/cmake/clang-cl-linux.cmake" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DXWIN_ROOT="$sdk" -DCVR_DXC_EXECUTABLE="$dxc" \
    -DCVR_NATIVE_FRAMEGEN=OFF "$@"
cmake --build "$build_dir" --target cyberpunkvrport_stereo --parallel "${BUILD_JOBS:-4}"
echo "Built: $build_dir/bin/red4ext/plugins/CyberpunkVR_Stereo/CyberpunkVR_Stereo.dll"
