cmake_minimum_required(VERSION 3.24)
project(LinuxAmdVRPortableTests LANGUAGES CXX)
enable_testing()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED)

set(CPVR_TEST_OPENXR_INCLUDE_DIR "" CACHE PATH "Optional existing OpenXR include directory")
if(NOT CPVR_TEST_OPENXR_INCLUDE_DIR)
    include(FetchContent)
    FetchContent_Declare(openxr_headers
        URL https://codeload.github.com/KhronosGroup/OpenXR-SDK/tar.gz/288d3a7ebc1ad959f62d51da75baa3d27438c499
        URL_HASH SHA256=7e95c7618b92237de74ce5ffebf2a604d84b450ef762ef314b75ddcd0dbe7bcd
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR headers-only)
    FetchContent_MakeAvailable(openxr_headers)
    set(CPVR_TEST_OPENXR_INCLUDE_DIR "${openxr_headers_SOURCE_DIR}/include")
endif()

foreach(test fog viewport features main sky cascade posevalidity layerdiagnostics)
    add_executable(parity_${test} "../render_parity_tests/${test}.cpp")
    target_compile_features(parity_${test} PRIVATE cxx_std_20)
    target_include_directories(parity_${test} PRIVATE ../../include "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
    if(NOT WIN32)
        # These pure math tests do not call the Windows hook declarations.
        target_compile_definitions(parity_${test} PRIVATE __fastcall=)
    endif()
    target_link_libraries(parity_${test} PRIVATE Threads::Threads)
    add_test(NAME parity_${test} COMMAND parity_${test})
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    add_executable(settings_resolution ../render_parity_tests/settingsresolution.cpp)
    target_compile_features(settings_resolution PRIVATE cxx_std_20)
    target_include_directories(settings_resolution PRIVATE ../../include)
    add_test(NAME settings_resolution COMMAND settings_resolution)
    add_test(NAME quaternion_return_abi
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_bone_rotation_abi.py")
endif()
add_test(NAME grenade_hold
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_grenade_hold.py")
add_test(NAME thunk_source_parity
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../check_thunk_parity.py")

add_subdirectory(../body_rotation_tests body_rotation)
target_include_directories(body_rotation_tests PRIVATE "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
