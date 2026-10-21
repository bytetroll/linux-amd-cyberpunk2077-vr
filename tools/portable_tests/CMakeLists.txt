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

foreach(test fog viewport features main sky cascade posevalidity layerdiagnostics queuehandoff)
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
add_test(NAME belt_reach
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_reach.py")
add_test(NAME holster
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_holster.py")
add_test(NAME weapon_carry
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_weapon_carry.py")
add_test(NAME reload_slide_handoff
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../belt_tests/run_reload_handoff.py")
add_test(NAME thunk_source_parity
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../check_thunk_parity.py")

add_executable(roomscale_recovery ../roomscale_tests/recovery.cpp)
target_compile_features(roomscale_recovery PRIVATE cxx_std_20)
target_include_directories(roomscale_recovery PRIVATE ../../include)
add_test(NAME roomscale_recovery COMMAND roomscale_recovery)

add_executable(physical_stance ../roomscale_tests/stance.cpp)
target_compile_features(physical_stance PRIVATE cxx_std_20)
target_include_directories(physical_stance PRIVATE ../../include)
add_test(NAME physical_stance COMMAND physical_stance)

add_executable(locomotion_heading ../roomscale_tests/locomotion_heading.cpp)
target_compile_features(locomotion_heading PRIVATE cxx_std_20)
target_include_directories(locomotion_heading PRIVATE ../../include)
add_test(NAME locomotion_heading COMMAND locomotion_heading)

add_executable(native_menu_input ../input_tests/native_menu.cpp)
target_compile_features(native_menu_input PRIVATE cxx_std_20)
target_include_directories(native_menu_input PRIVATE ../../include)
add_test(NAME native_menu_input COMMAND native_menu_input)

add_executable(scanner_input ../input_tests/scanner.cpp)
target_compile_features(scanner_input PRIVATE cxx_std_20)
target_include_directories(scanner_input PRIVATE ../../include)
add_test(NAME scanner_input COMMAND scanner_input)

add_executable(hub_menu_input ../input_tests/hub_menu.cpp)
target_compile_features(hub_menu_input PRIVATE cxx_std_20)
target_include_directories(hub_menu_input PRIVATE ../../include)
add_test(NAME hub_menu_input COMMAND hub_menu_input)

add_executable(menu_pointer ../input_tests/menu_pointer.cpp)
target_compile_features(menu_pointer PRIVATE cxx_std_20)
target_include_directories(menu_pointer PRIVATE ../../include "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
add_test(NAME menu_pointer COMMAND menu_pointer)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(menu_pointer_dispatch ../input_tests/menu_pointer_dispatch.cpp)
    target_compile_features(menu_pointer_dispatch PRIVATE cxx_std_20)
    target_include_directories(menu_pointer_dispatch BEFORE PRIVATE ../input_tests/win32_stub)
    target_include_directories(menu_pointer_dispatch PRIVATE ../../include "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
    target_link_libraries(menu_pointer_dispatch PRIVATE Threads::Threads)
    add_test(NAME menu_pointer_dispatch COMMAND menu_pointer_dispatch)
endif()

add_executable(finger_tracking ../finger_tests/fingertracking.cpp ../../src/Runtimes/FingerTracking.cpp)
target_compile_features(finger_tracking PRIVATE cxx_std_20)
target_include_directories(finger_tracking PRIVATE ../../include "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
target_link_libraries(finger_tracking PRIVATE Threads::Threads)
add_test(NAME finger_tracking COMMAND finger_tracking)

add_subdirectory(../body_rotation_tests body_rotation)
target_include_directories(body_rotation_tests PRIVATE "${CPVR_TEST_OPENXR_INCLUDE_DIR}")
