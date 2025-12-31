# RBC Interface Target Implementation
# This module implements the xmake interface_target pattern in CMake
#
# Usage:
#   rbc_interface_target(<name>
#       INTERFACE_CALLBACK <function_name>
#       IMPL_CALLBACK <function_name>
#       [NO_LINK]
#   )
#
# The function creates:
#   - <name>_int: INTERFACE library for headers and dependencies
#   - <name>_impl: SHARED/STATIC library with source files
#   - <name>: Alias target that links to impl

function(rbc_interface_target target_name)
    set(options NO_LINK)
    set(oneValueArgs INTERFACE_CALLBACK IMPL_CALLBACK)
    set(multiValueArgs "")
    cmake_parse_arguments(RBC_IT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT RBC_IT_INTERFACE_CALLBACK)
        message(FATAL_ERROR "rbc_interface_target: INTERFACE_CALLBACK is required")
    endif()
    
    if(NOT RBC_IT_IMPL_CALLBACK)
        message(FATAL_ERROR "rbc_interface_target: IMPL_CALLBACK is required")
    endif()
    
    # Create interface target (corresponds to _int__)
    add_library(${target_name}_int INTERFACE)
    
    # Call interface callback to configure the interface target
    # Use cmake_language(EVAL CODE ...) to call function by name stored in variable
    # This works around the limitation that cmake_language(CALL) may not work with variables
    cmake_language(EVAL CODE "${RBC_IT_INTERFACE_CALLBACK}(${target_name}_int)")
    
    # Create implementation target (corresponds to _impl__)
    # We'll determine the type (SHARED/STATIC) in the impl callback
    # For now, create as SHARED by default (can be changed in impl callback)
    add_library(${target_name}_impl SHARED)
    
    # Call impl callback FIRST to set PRIVATE EXPORT definitions
    # This ensures that when compiling implementation sources, EXPORT is used
    # Note: The impl callback should not change the library type - it's set at creation time
    cmake_language(EVAL CODE "${RBC_IT_IMPL_CALLBACK}(${target_name}_impl)")
    
    # Link interface to implementation AFTER impl callback
    # The INTERFACE IMPORT definition from interface target will propagate,
    # but PRIVATE EXPORT definition set in impl callback takes precedence for
    # compiling this target's own sources
    target_link_libraries(${target_name}_impl PRIVATE ${target_name}_int)
    
    # Create alias target (corresponds to main target name)
    if(NOT RBC_IT_NO_LINK)
        add_library(${target_name} ALIAS ${target_name}_impl)
    else()
        # For NO_LINK case, create an interface library that links to interface
        add_library(${target_name} INTERFACE)
        target_link_libraries(${target_name} INTERFACE ${target_name}_int)
    endif()
endfunction()

# Helper function to add interface dependencies
# Usage: rbc_add_interface_depend(target dep_target)
function(rbc_add_interface_depend target dep_target)
    target_link_libraries(${target} INTERFACE ${dep_target}_int)
endfunction()

