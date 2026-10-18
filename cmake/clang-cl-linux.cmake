# Cross-compile the Windows plugin from Linux with clang-cl + lld-link + llvm-ml.
#
# This toolchain targets the MSVC ABI (x86_64-pc-windows-msvc) using the MSVC CRT and
# Windows SDK headers/libs fetched by xwin, so no Windows machine or VM is needed.
#
#   xwin --accept-license splat --output ~/.xwin-cache/splat
#   cmake -B build-win -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-linux.cmake \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build-win
#
# Override the SDK location with -DXWIN_ROOT=/path/to/splat.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED XWIN_ROOT)
    set(XWIN_ROOT "$ENV{HOME}/.xwin-cache/splat")
endif()
if(NOT EXISTS "${XWIN_ROOT}/crt/include")
    message(FATAL_ERROR
        "xwin SDK not found at ${XWIN_ROOT}. Run:\n"
        "  xwin --accept-license splat --output ${XWIN_ROOT}")
endif()

set(CMAKE_C_COMPILER        clang-cl)
set(CMAKE_CXX_COMPILER      clang-cl)
set(CMAKE_ASM_MASM_COMPILER llvm-ml)
set(CMAKE_LINKER            lld-link)
set(CMAKE_RC_COMPILER       llvm-rc)

# clang-cl defaults to the host triple; say the target explicitly.
set(_target "--target=x86_64-pc-windows-msvc")

# -imsvc marks these as system includes, so SDK headers do not emit warnings
# under the project's /W4. Order matters: CRT first, then ucrt/um/shared.
set(_incs
    "-imsvc${XWIN_ROOT}/crt/include"
    "-imsvc${XWIN_ROOT}/sdk/include/ucrt"
    "-imsvc${XWIN_ROOT}/sdk/include/um"
    "-imsvc${XWIN_ROOT}/sdk/include/shared"
    "-imsvc${XWIN_ROOT}/sdk/include/winrt")
if(EXISTS "${XWIN_ROOT}/sdk/include/cppwinrt")
    list(APPEND _incs "-imsvc${XWIN_ROOT}/sdk/include/cppwinrt")
endif()
string(JOIN " " _incs_str ${_incs})

# The Windows SDK headers cross-include each other with inconsistent casing,
# which only matters on a case-sensitive filesystem. xwin normalises the files;
# this keeps clang from erroring on the remaining mismatches.
set(_compat "-Wno-nonportable-include-path -Wno-microsoft-include")

set(CMAKE_C_FLAGS_INIT   "${_target} ${_incs_str} ${_compat}")
set(CMAKE_CXX_FLAGS_INIT "-fdelayed-template-parsing ${_target} ${_incs_str} ${_compat}")

# llvm-ml speaks MASM; the seven .asm thunks are ml64 syntax.
set(CMAKE_ASM_MASM_FLAGS_INIT "-m64")

# xwin splats only the release CRT unless --include-debug-libs is passed, so a
# default Debug try-compile asks for msvcrtd.lib and the link fails before the
# project is even configured. Pin the dynamic RELEASE runtime for every config;
# the plugin ships against it anyway. Re-splat with --include-debug-libs if you
# ever genuinely want /MDd.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# llvm-rc does not inherit the C/CXX -imsvc paths, so a .rc that includes a SDK
# header (OpenXR's loader.rc wants winres.h) fails to find it. Give RC its own.
set(CMAKE_RC_FLAGS_INIT
    "/I${XWIN_ROOT}/sdk/include/um /I${XWIN_ROOT}/sdk/include/shared /I${XWIN_ROOT}/crt/include")

set(_libs
    "/libpath:${XWIN_ROOT}/crt/lib/x86_64"
    "/libpath:${XWIN_ROOT}/sdk/lib/um/x86_64"
    "/libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86_64")
string(JOIN " " _libs_str ${_libs})

set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_libs_str}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_libs_str}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_libs_str}")

# Look for headers/libs in the SDK, but run build tools from the host.
set(CMAKE_FIND_ROOT_PATH "${XWIN_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
