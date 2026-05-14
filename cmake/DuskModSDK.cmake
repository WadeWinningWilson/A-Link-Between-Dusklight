# add_dusk_mod(<target> SOURCES <file>... MOD_JSON <mod.json> [RES_DIR <res>])
set(DUSK_MODS_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/mods" CACHE PATH "Directory to write .dusk packages into")

function(add_dusk_mod target_name)
    cmake_parse_arguments(ARG "" "MOD_JSON;RES_DIR" "SOURCES" ${ARGN})
    if(NOT ARG_MOD_JSON)
        message(FATAL_ERROR "add_dusk_mod: MOD_JSON is required")
    endif()

    add_library(${target_name} SHARED ${ARG_SOURCES})
    set_target_properties(${target_name} PROPERTIES PREFIX "" WINDOWS_EXPORT_ALL_SYMBOLS ON)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_link_libraries(${target_name} PRIVATE dusklight_game_headers)

    if(APPLE)
        target_link_options(${target_name} PRIVATE -undefined dynamic_lookup)
    elseif(UNIX)
        target_link_options(${target_name} PRIVATE -Wl,--allow-shlib-undefined)
    elseif(WIN32)
        target_link_libraries(${target_name} PRIVATE dusklight_game)
        if(MSVC)
            target_link_options(${target_name} PRIVATE /INCREMENTAL:NO)
            set_target_properties(${target_name} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        endif()
    endif()


    set(_stage "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_stage")
    set(_out   "${DUSK_MODS_OUTPUT_DIR}/${target_name}.dusk")
    file(MAKE_DIRECTORY "${_stage}")  # must exist before POST_BUILD on Windows

    set(_zip_args "$<TARGET_FILE_NAME:${target_name}>" mod.json)
    set(_extra_cmds "")
    if(ARG_RES_DIR)
        list(APPEND _zip_args res)
        set(_extra_cmds COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_RES_DIR}" "${_stage}/res")
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage}" "${DUSK_MODS_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target_name}>" "${_stage}/$<TARGET_FILE_NAME:${target_name}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_MOD_JSON}" "${_stage}/mod.json"
        ${_extra_cmds}
        COMMAND ${CMAKE_COMMAND} -E tar cvf "${_out}" --format=zip ${_zip_args}
        WORKING_DIRECTORY "${_stage}"
        COMMENT "Packaging ${target_name} -> ${_out}"
    )
endfunction()
