include(ExternalProject)
set(CVR_FIDELITYFX_ROOT "${CMAKE_BINARY_DIR}/fidelityfx-native" CACHE PATH "Pinned FidelityFX SDK source checkout")
if(NOT EXISTS "${CVR_FIDELITYFX_ROOT}/sdk/CMakeLists.txt")
    FetchContent_Declare(cvr_framegen_fidelityfx
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK.git
        GIT_TAG c6efa6bf7f2027b3ec94f28578bb5965eabb9e55
        SOURCE_DIR "${CVR_FIDELITYFX_ROOT}")
    FetchContent_GetProperties(cvr_framegen_fidelityfx)
    if(NOT cvr_framegen_fidelityfx_POPULATED)
        FetchContent_Populate(cvr_framegen_fidelityfx)
    endif()
endif()
set(cvr_ffx_libs)
foreach(component backend_dx12 frameinterpolation opticalflow)
    list(APPEND cvr_ffx_libs "${CVR_FIDELITYFX_ROOT}/sdk/bin/ffx_sdk/ffx_${component}_x64.lib")
endforeach()
set(_cvr_ffx_stdlibs
    "/libpath:${XWIN_ROOT}/crt/lib/x86_64 /libpath:${XWIN_ROOT}/sdk/lib/um/x86_64 /libpath:${XWIN_ROOT}/sdk/lib/ucrt/x86_64 kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib")
ExternalProject_Add(cvr_framegen_sdk
    SOURCE_DIR "${CVR_FIDELITYFX_ROOT}/sdk"
    BINARY_DIR "${CMAKE_BINARY_DIR}/fidelityfx-native-build"
    DOWNLOAD_COMMAND ""
    CMAKE_ARGS -DFFX_ALL=OFF -DFFX_OF=ON -DFFX_FI=ON
        -DFFX_API_BACKEND=DX12_X64 -DFFX_BUILD_AS_DLL=OFF
        # Cross-compiling with Ninja: the SDK's platform test (sdk/CMakeLists.txt)
        # accepts CMAKE_EXE_LINKER_FLAGS == "/machine:x64" as an alternative to a
        # Visual Studio platform spec, which Ninja cannot carry. Must be exactly
        # that string for the STREQUAL to match; the SDK builds static libs, so no
        # library paths are needed on the exe linker line.
        # toolchain.cmake:56 does set(CMAKE_GENERATOR_PLATFORM "x64" ... FORCE),
        # which overrides any -D we pass. That file guards itself with
        # "if(_TOOLCHAIN_) return()", so defining it makes the SDK defer to ours.
        -D_TOOLCHAIN_=ON
        -DCMAKE_GENERATOR_PLATFORM=
        "-DCMAKE_EXE_LINKER_FLAGS=/machine:x64"
        "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/clang-cl-linux.cmake"
        -DCMAKE_BUILD_TYPE=Release
        # The only thing in this sub-build that links an EXE is CMake's own
        # compiler ABI test, and a try_compile does not inherit the -D cache
        # vars above, so it links without the SDK library paths and fails. The
        # SDK itself produces only static libraries, which never link. Skip it.
        -DCMAKE_C_COMPILER_WORKS=1
        -DCMAKE_CXX_COMPILER_WORKS=1
        # EXE_LINKER_FLAGS must stay EXACTLY "/machine:x64" for the STREQUAL above,
        # so the library search paths cannot ride there. STANDARD_LIBRARIES is
        # appended to every link line, so the paths go there instead (keeping
        # CMake's usual default libs, which overriding this variable would drop).
        "-DCMAKE_C_STANDARD_LIBRARIES=${_cvr_ffx_stdlibs}"
        "-DCMAKE_CXX_STANDARD_LIBRARIES=${_cvr_ffx_stdlibs}"
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config Release --parallel 4
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${cvr_ffx_libs})
add_dependencies(cyberpunkvrport_stereo cvr_framegen_sdk)
target_include_directories(cyberpunkvrport_stereo PRIVATE
    "${CVR_FIDELITYFX_ROOT}/sdk/include" "${CMAKE_SOURCE_DIR}/externals/nvidia-optical-flow")
target_link_libraries(cyberpunkvrport_stereo PRIVATE ${cvr_ffx_libs})
