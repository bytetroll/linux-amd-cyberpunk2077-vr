cmake_minimum_required(VERSION 3.24)

project(CyberpunkVRPort VERSION 0.1.7 LANGUAGES CXX C ASM_MASM)

# Used only by tools/check_thunk_parity.py, which fails the build if the MASM
# and GAS spellings of the detour thunks drift apart.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Git REQUIRED)

include(FetchContent)
FetchContent_Declare(
  openxr
  URL https://codeload.github.com/KhronosGroup/OpenXR-SDK/tar.gz/288d3a7ebc1ad959f62d51da75baa3d27438c499
  URL_HASH SHA256=7e95c7618b92237de74ce5ffebf2a604d84b450ef762ef314b75ddcd0dbe7bcd
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(DYNAMIC_LOADER OFF CACHE BOOL "Build the OpenXR loader as a shared library")
FetchContent_MakeAvailable(openxr)

# Dependencies are fetched into the build directory, never into the source tree.
FetchContent_Declare(
  imgui
  URL https://codeload.github.com/ocornut/imgui/tar.gz/cb16be3a3fc1f9cd146ae24d52b615f8a05fa93d
  URL_HASH SHA256=e866d1ae59a4c0941e08c5a14f2ec3957777fb61b768c75b003160926d268cf0
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)
FetchContent_Declare(
  minhook
  URL https://codeload.github.com/TsudaKageyu/minhook/tar.gz/d94c64d32ea37bc4f5ee47d580709f70c6fb6080
  URL_HASH SHA256=d9633061a5d8a21ac2597e6a206e2e541fa18c7618a0c9245cfe649ca0b99fa8
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(minhook)
FetchContent_Declare(
  red4extsdk
  URL https://codeload.github.com/marklove5102/RED4ext.SDK/tar.gz/d02a1f329744cd4e39c6f7f3fdf63053731e1aff
  URL_HASH SHA256=de5dfe8102b140ce62d0deb749c4082d86ca413bc015dd1606f959c8c0cee568
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  PATCH_COMMAND "${CMAKE_COMMAND}"
    "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE_DIR=<SOURCE_DIR>"
    "-DPATCH=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/red4ext-clang-linux.patch"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplySourcePatch.cmake"
)
FetchContent_MakeAvailable(red4extsdk)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)





# ============================================================================================
# CyberpunkVR_Stereo -- the RED4ext plugin. This is the whole mod.
# ============================================================================================
#
# It used to ship as a dxgi.dll proxy sitting next to the game exe, with DllMain and the seven
# DXGI factory exports forwarding to the system dxgi.dll. None of the code ever needed to BE
# the proxy -- it only needed a device, a queue and Present, and RED4ext hands over all three.
# The proxy is gone: no DllMain, no export forwarders, no LoadLibrary of the real dxgi.dll,
# and no second copy of every hook racing the first if both DLLs were installed by mistake.
#
# ONE PLUGIN, NOT TWO. CyberpunkVR_Hands.dll used to be a separate red4ext plugin, and this comment
# used to say it stayed that way. It does not: the glob below takes ALL of src/, which now includes the
# VRIK pose path (src/Hooks/AnimPose.cpp, src/Anim/, src/Natives/) that used to build into Hands. The
# single DLL registers all 219 natives and installs every hook.
#
# THE OLD CyberpunkVR_Hands.dll MUST BE REMOVED FROM red4ext\plugins. Both binaries register the same
# 219 natives and detour the same addresses, and two plugins detouring one address is the crash that
# reads as a fault at FFFFFFFFFFFFFFFF with no usable stack -- checked, not assumed: the deployed
# Hands.dll contains all 219 native name strings.
#
# The single file is also SMALLER than the two it replaces (2,199,552 against 1,291,776 + 1,425,920),
# because the code they shared is now emitted once.
# THE FILE LIST IS A GLOB, and deliberately so. This tree is mid-restructure: the monoliths are
# being split into one-hook-per-file units under src/Hooks, src/Anim and the rest, and a hand-kept
# list would be edited on every one of those splits and forgotten on one of them. CONFIGURE_DEPENDS
# makes the build re-glob when a file appears, so the usual objection to globbing does not apply.
file(GLOB_RECURSE CPVR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# Keep the folder shape in the IDE -- Hooks/, Runtimes/, Anim/, Utils/ are the reading order.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CPVR_SOURCES})

# ---------------------------------------------------------------------------
# Detour thunks. There are two hand-maintained spellings of the same ten entry
# points and they MUST stay in sync -- tools/check_thunk_parity.py fails the
# build if their symbol contract or opcode sequences diverge.
#
#   src/Hooks/*.asm  MASM, assembled by ml64 on MSVC. Carries PROC FRAME, so a
#                    Windows/MSVC build gets real .pdata/.xdata unwind records.
#   src/Hooks/*.s    GAS, assembled by GNU as everywhere else. No assembler
#                    available on Linux emits COFF unwind from hand-written
#                    assembly (for PE targets GCC emits .seh_* from the
#                    compiler and the assembler only consumes it), so this path
#                    has no unwind record. That is safe only because every C++
#                    callback a thunk invokes is exception-contained at its
#                    definition site -- see include/Stereo/ThunkCallbacks.hpp.
# ---------------------------------------------------------------------------
set(CPVR_THUNK_NAMES
    CameraPoseLeafThunks
    SwimmingActionLeafThunk
    WorldMarkerTextThunk
    PhotoCaptureClampThunk
    SurveillanceInputThunk
    StoryAttentionThunk
    RenderGraphCacheThunk
)

# NB: not `if(MSVC)` -- CMake sets that true for clang-cl too (MSVC simulate-id),
# and clang-cl on Linux has no ml64 to read the .asm files.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    foreach(thunk IN LISTS CPVR_THUNK_NAMES)
        list(APPEND CPVR_THUNKS "src/Hooks/${thunk}.asm")
    endforeach()
else()
    find_program(CPVR_MINGW_AS
        NAMES x86_64-w64-mingw32-as
        DOC "GNU assembler for x86_64-w64-mingw32, used for src/Hooks/*.s")
    if(NOT CPVR_MINGW_AS)
        message(FATAL_ERROR
            "x86_64-w64-mingw32-as not found. The detour thunks are assembled "
            "from src/Hooks/*.s, and this is the assembler that builds them on "
            "this toolchain: a COFF/pe target assembler that accepts the "
            "MASM-style .def function declarations and is scriptable from CMake. "
            "Install it with:\n"
            "    apt install binutils-mingw-w64-x86-64\n"
            "or point -DCPVR_MINGW_AS=/path/to/x86_64-w64-mingw32-as at it.")
    endif()

    set(CPVR_ASM_OBJ_DIR "${CMAKE_CURRENT_BINARY_DIR}/thunk-asm")
    file(MAKE_DIRECTORY "${CPVR_ASM_OBJ_DIR}")
    foreach(thunk IN LISTS CPVR_THUNK_NAMES)
        set(src "${CMAKE_CURRENT_SOURCE_DIR}/src/Hooks/${thunk}.s")
        set(obj "${CPVR_ASM_OBJ_DIR}/${thunk}.obj")
        add_custom_command(
            OUTPUT "${obj}"
            COMMAND "${CPVR_MINGW_AS}" -o "${obj}" "${src}"
            DEPENDS "${src}"
            COMMENT "Assembling thunk ${thunk}.s"
            VERBATIM)
        list(APPEND CPVR_THUNKS "${obj}")
    endforeach()
endif()

add_custom_target(cpvr_thunk_parity_check
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_thunk_parity.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking .asm/.s thunk parity")

add_library(cyberpunkvrport_stereo SHARED
    ${CPVR_SOURCES}
    ${CPVR_THUNKS}
    externals/im3d/im3d.cpp
)
add_dependencies(cyberpunkvrport_stereo cpvr_thunk_parity_check)
include(cmake/WorldMarkerShaders.cmake)
cvr_world_marker_shaders(cyberpunkvrport_stereo)
# Native frame generation needs the FidelityFX SDK (optical flow + frame
# interpolation) and NVIDIA's optical-flow header. Its current inputs come
# from the NVIDIA-only Streamline and nvngx_dlss paths, and
# Settings::backend defaults to Backend::Nvidia. Turning this OFF drops the
# FidelityFX dependency (whose CMake assumes a Visual Studio generator and a
# NuGet-restored WinPixEventRuntime.dll) and compiles Generator.cpp as a stub,
# so the cvr::framegen API its callers use stays intact and Generate() simply
# reports that it produced nothing.
option(CVR_NATIVE_FRAMEGEN "Build native frame generation (FidelityFX; NVIDIA-only at runtime)" OFF)
if(CVR_NATIVE_FRAMEGEN)
    include(cmake/NativeFramegen.cmake)
endif()
target_compile_definitions(cyberpunkvrport_stereo PRIVATE
    CVR_NATIVE_FRAMEGEN=$<BOOL:${CVR_NATIVE_FRAMEGEN}>)
target_compile_features(cyberpunkvrport_stereo PRIVATE cxx_std_20)
target_compile_definitions(cyberpunkvrport_stereo PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX
    XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D12
    CPVR_RED4EXT_PLUGIN=1
    _CRT_SECURE_NO_WARNINGS=1
)
target_link_libraries(cyberpunkvrport_stereo PRIVATE
    openxr_loader
    imgui
    minhook
    d3d12.lib
    d3dcompiler
    dxgi.lib        # the SYSTEM import lib -- bare "dxgi" would resolve to our own target
    psapi
    RED4ext::SDK
)
# Use the JSON parser already selected for the statically linked OpenXR loader.
if(BUILD_WITH_SYSTEM_JSONCPP)
    target_link_libraries(cyberpunkvrport_stereo PRIVATE JsonCpp::JsonCpp)
else()
    target_include_directories(cyberpunkvrport_stereo PRIVATE ${openxr_SOURCE_DIR}/src/external/jsoncpp/include)
endif()
target_include_directories(cyberpunkvrport_stereo PRIVATE
    # ONE root for our own headers. Every internal include reads "Area/Name.hpp", so a header's
    # path states which area owns it -- which is the whole point of the layout. The old set of
    # eleven roots let any header be reached by its bare name from anywhere, and that is precisely
    # what let the monoliths grow: nothing about an include said where the thing belonged.
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/externals/streamline/include
    # --- third-party / vendored ---
    ${CMAKE_CURRENT_SOURCE_DIR}/externals/im3d
    ${minhook_SOURCE_DIR}/include
    ${minhook_SOURCE_DIR}/src
    ${minhook_SOURCE_DIR}/src/hde
)
if(MSVC)
    target_compile_options(cyberpunkvrport_stereo PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /EHsc>)
    # SYMBOLS IN THE RELEASE BUILD. A crash landed inside this DLL and there was nothing to read it
    # with: no PDB here, no cdb or windbg on the machine, and dumpbin only opens PE files. Placing
    # CyberpunkVR_Stereo.dll+0x459AF took disassembling the whole image and matching the faulting
    # instruction by hand. That is affordable once.
    #
    # /Zi and /DEBUG do not change optimisation -- /OPT:REF and /OPT:ICF are restored explicitly
    # because /DEBUG turns them off by default, and without them the linker would stop folding and
    # stripping, which WOULD change the binary. The PDB ships nowhere; it stays in the build tree
    # so a dump from this machine resolves instantly and a tester's dump resolves against the build
    # that produced their DLL.
    target_compile_options(cyberpunkvrport_stereo PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(cyberpunkvrport_stereo PRIVATE
        $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
endif()
set_target_properties(cyberpunkvrport_stereo PROPERTIES
    PREFIX ""
    OUTPUT_NAME CyberpunkVR_Stereo
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/red4ext/plugins/CyberpunkVR_Stereo"
)

# ============================================================================================
# Tools. Separate targets, never deployed with the plugin.
# ============================================================================================
# xr_probe_layer -- an OpenXR API layer that records what any VR application asks the runtime for
# and what it submits back. Used to read Quest 3 geometry out of R.E.A.L. VR (a dxgi.dll proxy)
# without touching its internals. Build it explicitly:
#     cmake --build build --config Release --target xr_probe_layer
option(CVR_BUILD_XR_PROBE "Build optional OpenXR development probe tools" OFF)
if(CVR_BUILD_XR_PROBE)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/xr_probe/CMakeLists.txt")
        message(FATAL_ERROR "The optional xr_probe sources are not included in this source export")
    endif()
    add_subdirectory(tools/xr_probe)
endif()
