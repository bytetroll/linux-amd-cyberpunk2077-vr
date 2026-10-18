get_filename_component(directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${directory}")
set(header "#pragma once\nnamespace cvr::markers::bytecode {\n")
# dxc parses MSVC-style, so any argument starting with "/" is taken as an option.
# An absolute POSIX path is therefore never seen as the input file ("Required
# input file argument is missing"), and dxc has no "--" end-of-options marker.
# Pass the bare filename from its own directory instead; identical on Windows.
get_filename_component(source_dir "${SOURCE}" DIRECTORY)
get_filename_component(source_name "${SOURCE}" NAME)
foreach(variant Sprite Procedural)
    set(binary "${directory}/${variant}.dxil")
    execute_process(COMMAND "${DXC}" -T vs_6_0 -E "${variant}Main" -O3 -Fo "${binary}" "${source_name}"
        WORKING_DIRECTORY "${source_dir}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "World marker ${variant} shader failed: ${output}${errors}")
    endif()
    file(READ "${binary}" bytes HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
    string(APPEND header "alignas(16) inline constexpr unsigned char ${variant}[] = {${bytes}};\n")
endforeach()
string(APPEND header "}\n")
file(WRITE "${OUTPUT}" "${header}")
