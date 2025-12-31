# RBC OIDN Plugin

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(oidn_plugin SHARED)

# Apply basic settings
rbc_apply_basic_settings(oidn_plugin PROJECT_KIND shared)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(oidn_plugin PRIVATE ${SOURCES})

# Public interface: headers that users of this library need
target_include_directories(oidn_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Public dependencies: users of this library also need these (header-only)
target_link_libraries(oidn_plugin PUBLIC
    oidn_interface
)

# Private dependencies: only needed for building this library
# Link LuisaCompute targets - try both alias and actual names
if(TARGET lc-vstl)
    target_link_libraries(oidn_plugin PRIVATE lc-vstl)
elseif(TARGET luisa-compute-vstl)
    target_link_libraries(oidn_plugin PRIVATE luisa-compute-vstl)
endif()

if(TARGET lc-runtime)
    target_link_libraries(oidn_plugin PRIVATE lc-runtime)
elseif(TARGET luisa-compute-runtime)
    target_link_libraries(oidn_plugin PRIVATE luisa-compute-runtime)
endif()

if(TARGET lc-backends-dummy)
    target_link_libraries(oidn_plugin PRIVATE lc-backends-dummy)
elseif(TARGET luisa-compute-backends)
    target_link_libraries(oidn_plugin PRIVATE luisa-compute-backends)
endif()

# LMDB is needed because luisa-compute-vstl uses it (OBJECT library PRIVATE deps don't propagate)
if(TARGET luisa-compute-ext-lmdb)
    target_link_libraries(oidn_plugin PRIVATE luisa-compute-ext-lmdb)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(oidn_plugin PRIVATE luisa-compute-include)
endif()

# Link OIDN libraries
# Note: These should be available from build/download/oidn-2.3.3-windows-x64
set(OIDN_BASE_DIR ${CMAKE_SOURCE_DIR}/build/download/oidn-2.3.3-windows-x64)
set(OIDN_LIB_PATH ${OIDN_BASE_DIR}/OpenImageDenoise.lib)
set(OIDN_CORE_LIB_PATH ${OIDN_BASE_DIR}/OpenImageDenoise_core.lib)

if(EXISTS ${OIDN_LIB_PATH} AND EXISTS ${OIDN_CORE_LIB_PATH})
    message(STATUS "Found OIDN libraries: ${OIDN_LIB_PATH}, ${OIDN_CORE_LIB_PATH}")
    target_link_libraries(oidn_plugin PRIVATE ${OIDN_LIB_PATH} ${OIDN_CORE_LIB_PATH})
    
    # On Windows, copy OIDN DLLs to output directory for runtime loading
    if(WIN32)
        # Get the output directory for the target
        get_cmake_property(IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
        if(IS_MULTI_CONFIG)
            # For multi-config generators (Visual Studio), copy DLLs for each configuration
            foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
                string(TOUPPER ${CONFIG} CONFIG_UPPER)
                set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}}")
                if(OUTPUT_DIR)
                    # Copy main OIDN DLLs - ensure directory exists first
                    add_custom_command(TARGET oidn_plugin POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${OIDN_BASE_DIR}/OpenImageDenoise.dll"
                            "${OIDN_BASE_DIR}/OpenImageDenoise_core.dll"
                            "${OUTPUT_DIR}/"
                        COMMENT "Copying OIDN DLLs to ${OUTPUT_DIR}"
                    )
                    # Copy device-specific DLLs (may be needed depending on backend)
                    add_custom_command(TARGET oidn_plugin POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${OIDN_BASE_DIR}/OpenImageDenoise_device_cpu.dll"
                            "${OIDN_BASE_DIR}/OpenImageDenoise_device_cuda.dll"
                            "${OUTPUT_DIR}/"
                        COMMENT "Copying OIDN device DLLs to ${OUTPUT_DIR}"
                    )
                    # Copy TBB DLLs (required by OIDN)
                    add_custom_command(TARGET oidn_plugin POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${OIDN_BASE_DIR}/tbb12.dll"
                            "${OIDN_BASE_DIR}/tbbbind.dll"
                            "${OIDN_BASE_DIR}/tbbbind_2_0.dll"
                            "${OIDN_BASE_DIR}/tbbbind_2_5.dll"
                            "${OUTPUT_DIR}/"
                        COMMENT "Copying TBB DLLs to ${OUTPUT_DIR}"
                    )
                endif()
            endforeach()
        else()
            # For single-config generators
            set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
            if(OUTPUT_DIR)
                add_custom_command(TARGET oidn_plugin POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${OIDN_BASE_DIR}/OpenImageDenoise.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_core.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_device_cpu.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_device_cuda.dll"
                        "${OUTPUT_DIR}/"
                    COMMENT "Copying OIDN DLLs to ${OUTPUT_DIR}"
                )
                add_custom_command(TARGET oidn_plugin POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${OIDN_BASE_DIR}/tbb12.dll"
                        "${OIDN_BASE_DIR}/tbbbind.dll"
                        "${OIDN_BASE_DIR}/tbbbind_2_0.dll"
                        "${OIDN_BASE_DIR}/tbbbind_2_5.dll"
                        "${OUTPUT_DIR}/"
                    COMMENT "Copying TBB DLLs to ${OUTPUT_DIR}"
                )
            endif()
        endif()
        
        # Also set debugger environment for Visual Studio debugging
        set_target_properties(oidn_plugin PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "PATH=${OIDN_BASE_DIR};$ENV{PATH}"
        )
    endif()
else()
    message(WARNING "OIDN libraries not found at:")
    message(WARNING "  ${OIDN_LIB_PATH}")
    message(WARNING "  ${OIDN_CORE_LIB_PATH}")
    message(WARNING "Please ensure OIDN is installed at: ${OIDN_BASE_DIR}")
endif()

# OIDN Checker executable
add_executable(oidn_checker
    ${CMAKE_CURRENT_SOURCE_DIR}/oidn_checker.cpp
)
rbc_apply_basic_settings(oidn_checker PROJECT_KIND binary)

# Link LuisaCompute targets - try both alias and actual names
if(TARGET lc-volk)
    target_link_libraries(oidn_checker PRIVATE lc-volk)
elseif(TARGET luisa-compute-ext-volk)
    target_link_libraries(oidn_checker PRIVATE luisa-compute-ext-volk)
endif()

if(TARGET lc-runtime)
    target_link_libraries(oidn_checker PRIVATE lc-runtime)
elseif(TARGET luisa-compute-runtime)
    target_link_libraries(oidn_checker PRIVATE luisa-compute-runtime)
endif()

if(TARGET lc-backends-dummy)
    target_link_libraries(oidn_checker PRIVATE lc-backends-dummy)
elseif(TARGET luisa-compute-backends)
    target_link_libraries(oidn_checker PRIVATE luisa-compute-backends)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(oidn_checker PRIVATE luisa-compute-include)
endif()

target_link_libraries(oidn_checker PRIVATE
    oidn_plugin
)

# On Windows, also copy OIDN DLLs for oidn_checker executable
if(WIN32 AND EXISTS ${OIDN_LIB_PATH} AND EXISTS ${OIDN_CORE_LIB_PATH})
    get_cmake_property(IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
    if(IS_MULTI_CONFIG)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}}")
            if(OUTPUT_DIR)
                add_custom_command(TARGET oidn_checker POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${OIDN_BASE_DIR}/OpenImageDenoise.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_core.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_device_cpu.dll"
                        "${OIDN_BASE_DIR}/OpenImageDenoise_device_cuda.dll"
                        "${OUTPUT_DIR}/"
                    COMMENT "Copying OIDN DLLs for oidn_checker to ${OUTPUT_DIR}"
                )
                add_custom_command(TARGET oidn_checker POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${OIDN_BASE_DIR}/tbb12.dll"
                        "${OIDN_BASE_DIR}/tbbbind.dll"
                        "${OIDN_BASE_DIR}/tbbbind_2_0.dll"
                        "${OIDN_BASE_DIR}/tbbbind_2_5.dll"
                        "${OUTPUT_DIR}/"
                    COMMENT "Copying TBB DLLs for oidn_checker to ${OUTPUT_DIR}"
                )
            endif()
        endforeach()
    else()
        set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        if(OUTPUT_DIR)
            add_custom_command(TARGET oidn_checker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OIDN_BASE_DIR}/OpenImageDenoise.dll"
                    "${OIDN_BASE_DIR}/OpenImageDenoise_core.dll"
                    "${OIDN_BASE_DIR}/OpenImageDenoise_device_cpu.dll"
                    "${OIDN_BASE_DIR}/OpenImageDenoise_device_cuda.dll"
                    "${OUTPUT_DIR}/"
                COMMENT "Copying OIDN DLLs for oidn_checker to ${OUTPUT_DIR}"
            )
            add_custom_command(TARGET oidn_checker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OIDN_BASE_DIR}/tbb12.dll"
                    "${OIDN_BASE_DIR}/tbbbind.dll"
                    "${OIDN_BASE_DIR}/tbbbind_2_0.dll"
                    "${OIDN_BASE_DIR}/tbbbind_2_5.dll"
                    "${OUTPUT_DIR}/"
                COMMENT "Copying TBB DLLs for oidn_checker to ${OUTPUT_DIR}"
            )
        endif()
    endif()
endif()

