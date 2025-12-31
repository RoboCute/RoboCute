# RBC CMake Configuration
# Implements lc_basic_settings equivalent functionality

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    if(MSVC)
        add_compile_options(/Zc:preprocessor /wd4244)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(/GS /Gd)
        else()
            add_compile_options(/GS- /Gd)
        endif()
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-stdlib=libc++)
        add_link_options(-stdlib=libc++)
    endif()
    # Add -fPIC for static libraries
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
elseif(APPLE)
    # macOS-specific settings
    add_compile_options(-no-pie)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wno-invalid-specialization)
    endif()
endif()

# Architecture-specific settings
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-mfma)
    endif()
endif()

# Function to apply lc_basic_settings equivalent to a target
function(rbc_apply_basic_settings target)
    set(options ENABLE_EXCEPTION RTTI)
    set(oneValueArgs PROJECT_KIND OPTIMIZE)
    set(multiValueArgs "")
    cmake_parse_arguments(RBC_BS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Note: PROJECT_KIND is informational only - library type must be set at creation time
    # This parameter is kept for compatibility but does not modify the target type
    # The caller should create the target with the correct type (SHARED/STATIC) before calling this function
    
    # Exception handling
    if(RBC_BS_ENABLE_EXCEPTION)
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fexceptions>
        )
    else()
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/EHsc->
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>
        )
    endif()
    
    # RTTI
    if(RBC_BS_RTTI)
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/GR>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-frtti>
        )
    else()
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/GR->
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-rtti>
        )
    endif()
    
    # Optimization
    if(RBC_BS_OPTIMIZE)
        if(RBC_BS_OPTIMIZE STREQUAL "none")
            set_target_properties(${target} PROPERTIES
                CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O0"
            )
        elseif(RBC_BS_OPTIMIZE STREQUAL "aggressive")
            set_target_properties(${target} PROPERTIES
                CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3"
            )
        endif()
    endif()
    
    # Windows runtime (MD/MDd)
    if(WIN32)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    
    # Linux: fPIC for static libraries
    if(UNIX AND NOT APPLE)
        if(RBC_BS_PROJECT_KIND STREQUAL "static")
            set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()
endfunction()

# Function to set precompiled header
function(rbc_set_precompiled_header target header_file)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        target_precompile_headers(${target} PRIVATE ${header_file})
    endif()
endfunction()

# Function to enable unity build
function(rbc_enable_unity_build target batch_size)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
        if(batch_size)
            set_target_properties(${target} PROPERTIES UNITY_BUILD_BATCH_SIZE ${batch_size})
        endif()
    endif()
endfunction()

