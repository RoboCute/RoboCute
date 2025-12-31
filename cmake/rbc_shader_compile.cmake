# RBC Shader Compilation Support
# Handles shader compilation using clangcxx_compiler

# Global flag to track if shader compilation targets have been created
if(NOT DEFINED _RBC_SHADER_COMPILE_TARGETS_CREATED)
    set(_RBC_SHADER_COMPILE_TARGETS_CREATED FALSE CACHE INTERNAL "Flag to track shader compile targets")
endif()

function(rbc_add_shader_compilation target_name)
    # Determine compiler path
    if(WIN32)
        set(COMPILER_NAME "clangcxx_compiler.exe")
    else()
        set(COMPILER_NAME "clangcxx_compiler")
    endif()
    
    set(COMPILER_PATH "${CMAKE_SOURCE_DIR}/build/tool/clangcxx_compiler/${COMPILER_NAME}")
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/rbc/shader")
    
    # Backends to compile
    set(BACKENDS "dx" "vk")
    
    foreach(BACKEND ${BACKENDS})
        set(CACHE_DIR "${SHADER_DIR}/.cache/${BACKEND}")
        set(OUT_DIR "${CMAKE_BINARY_DIR}/shader_build_${BACKEND}")
        set(TARGET_NAME "rbc_shader_compile_${BACKEND}")
        
        # Create output directory
        file(MAKE_DIRECTORY ${OUT_DIR})
        file(MAKE_DIRECTORY ${CACHE_DIR})
        
        # Only create the target once (shared across all callers)
        if(NOT TARGET ${TARGET_NAME})
            # Custom command to compile shaders
            add_custom_command(
                OUTPUT ${OUT_DIR}/.shader_compile_${BACKEND}_done
                COMMAND ${COMPILER_PATH}
                    --in=${SHADER_DIR}/src
                    --out=${OUT_DIR}
                    --cache_dir=${CACHE_DIR}
                    --backend=${BACKEND}
                    --include=${SHADER_DIR}/include
                COMMAND ${CMAKE_COMMAND} -E touch ${OUT_DIR}/.shader_compile_${BACKEND}_done
                DEPENDS ${COMPILER_PATH}
                COMMENT "Compiling shaders for ${BACKEND} backend"
                VERBATIM
            )
            
            # Add dependency to target
            add_custom_target(${TARGET_NAME}
                DEPENDS ${OUT_DIR}/.shader_compile_${BACKEND}_done
            )
        endif()
        
        # Add dependency to the calling target
        add_dependencies(${target_name} ${TARGET_NAME})
    endforeach()
endfunction()

function(rbc_add_shader_hostgen target_name)
    # Determine compiler path
    if(WIN32)
        set(COMPILER_NAME "clangcxx_compiler.exe")
    else()
        set(COMPILER_NAME "clangcxx_compiler")
    endif()
    
    set(COMPILER_PATH "${CMAKE_SOURCE_DIR}/build/tool/clangcxx_compiler/${COMPILER_NAME}")
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/rbc/shader")
    
    set(CACHE_DIR "${SHADER_DIR}/.cache/hostgen")
    set(OUT_DIR "${CMAKE_BINARY_DIR}/shader_build_hostgen")
    set(TARGET_NAME "rbc_shader_hostgen")
    
    # Create output directory
    file(MAKE_DIRECTORY ${OUT_DIR})
    file(MAKE_DIRECTORY ${CACHE_DIR})
    
    # Only create the target once (shared across all callers)
    if(NOT TARGET ${TARGET_NAME})
        # Custom command to compile shaders with hostgen
        add_custom_command(
            OUTPUT ${OUT_DIR}/.shader_hostgen_done
            COMMAND ${COMPILER_PATH}
                --in=${SHADER_DIR}/src
                --out=${OUT_DIR}
                --cache_dir=${CACHE_DIR}
                --hostgen=${SHADER_DIR}/host
                --include=${SHADER_DIR}/include
            COMMAND ${CMAKE_COMMAND} -E touch ${OUT_DIR}/.shader_hostgen_done
            DEPENDS ${COMPILER_PATH}
            COMMENT "Generating shader host code"
            VERBATIM
        )
        
        # Add dependency to target
        add_custom_target(${TARGET_NAME}
            DEPENDS ${OUT_DIR}/.shader_hostgen_done
        )
    endif()
    
    # Add dependency to the calling target
    add_dependencies(${target_name} ${TARGET_NAME})
    
    # Add host directory to include paths
    target_include_directories(${target_name} PRIVATE ${SHADER_DIR}/host)
endfunction()

