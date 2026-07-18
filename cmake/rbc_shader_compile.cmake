# RBC Shader Compilation Support
# Handles shader compilation using clangcxx_compiler

function(_rbc_load_shader_backends output_variable)
    set(MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rbc/shader/shader_variants.json")
    if(NOT EXISTS "${MANIFEST_PATH}")
        message(FATAL_ERROR "Shader variant manifest not found: ${MANIFEST_PATH}")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MANIFEST_PATH}")
    file(READ "${MANIFEST_PATH}" MANIFEST_JSON)
    string(JSON BACKEND_COUNT ERROR_VARIABLE JSON_ERROR LENGTH "${MANIFEST_JSON}" backends)
    if(NOT JSON_ERROR STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Cannot read shader backends from ${MANIFEST_PATH}: ${JSON_ERROR}")
    endif()
    if(BACKEND_COUNT LESS 1)
        message(FATAL_ERROR "Shader variant manifest must declare at least one backend")
    endif()

    math(EXPR LAST_BACKEND_INDEX "${BACKEND_COUNT} - 1")
    set(BACKENDS)
    foreach(BACKEND_INDEX RANGE ${LAST_BACKEND_INDEX})
        string(JSON BACKEND_TYPE TYPE "${MANIFEST_JSON}" backends ${BACKEND_INDEX})
        if(NOT BACKEND_TYPE STREQUAL "STRING")
            message(FATAL_ERROR "Shader backend at index ${BACKEND_INDEX} must be a string")
        endif()
        string(JSON BACKEND GET "${MANIFEST_JSON}" backends ${BACKEND_INDEX})
        if(NOT BACKEND MATCHES "^[a-z][a-z0-9_-]*$")
            message(FATAL_ERROR "Invalid shader backend in ${MANIFEST_PATH}: ${BACKEND}")
        endif()
        if(BACKEND IN_LIST BACKENDS)
            message(FATAL_ERROR "Duplicate shader backend in ${MANIFEST_PATH}: ${BACKEND}")
        endif()
        list(APPEND BACKENDS "${BACKEND}")
    endforeach()

    set(${output_variable} "${BACKENDS}" PARENT_SCOPE)
endfunction()

function(rbc_add_shader_compilation target_name)
    find_program(RBC_SHADER_UV_EXECUTABLE NAMES uv REQUIRED)
    _rbc_load_shader_backends(BACKENDS)
    
    foreach(BACKEND ${BACKENDS})
        set(OUT_DIR "${CMAKE_BINARY_DIR}/shader_build_${BACKEND}")
        set(TARGET_NAME "rbc_shader_compile_${BACKEND}")

        # Only create the target once (shared across all callers)
        if(NOT TARGET ${TARGET_NAME})
            # The driver owns dependency tracking, staging, and cache validation.
            add_custom_target(${TARGET_NAME}
                COMMAND ${RBC_SHADER_UV_EXECUTABLE} run shader-build build
                    --project-root ${CMAKE_SOURCE_DIR}
                    --build-root ${CMAKE_BINARY_DIR}
                    --backend ${BACKEND}
                BYPRODUCTS
                    ${OUT_DIR}/shader_manifest.json
                COMMENT "Compiling shaders for ${BACKEND} backend"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                VERBATIM
            )
        endif()
        
        # Add dependency to the calling target
        add_dependencies(${target_name} ${TARGET_NAME})
    endforeach()
endfunction()

function(rbc_add_shader_hostgen target_name)
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/rbc/shader")
    set(TARGET_NAME "rbc_shader_hostgen")
    find_program(RBC_SHADER_UV_EXECUTABLE NAMES uv REQUIRED)

    # Only create the target once (shared across all callers)
    if(NOT TARGET ${TARGET_NAME})
        add_custom_target(${TARGET_NAME}
            COMMAND ${RBC_SHADER_UV_EXECUTABLE} run shader-build build
                --project-root ${CMAKE_SOURCE_DIR}
                --build-root ${CMAKE_BINARY_DIR}
                --hostgen-only
                --host-out ${SHADER_DIR}/host
            COMMENT "Generating shader host code"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM
        )
    endif()

    # Host code and runtime binaries are one shader build contract.
    rbc_add_shader_compilation(${TARGET_NAME})
    
    # Add dependency to the calling target
    add_dependencies(${target_name} ${TARGET_NAME})
    
    # Add host directory to include paths
    target_include_directories(${target_name} PRIVATE ${SHADER_DIR}/host)
endfunction()

