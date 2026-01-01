# CMake script to install resources after build
# This script extracts 7z archives and copies shader build results to the output directory

# Find 7z executable
function(find_7z_executable result_var)
    set(possible_paths
        "C:/Program Files/7-Zip/7z.exe"
        "C:/Program Files (x86)/7-Zip/7z.exe"
    )
    
    # Try to find 7z in PATH
    find_program(SEVEN_ZIP_EXE
        NAMES 7z 7za
        PATHS ${possible_paths}
        DOC "Path to 7z executable"
    )
    
    if(SEVEN_ZIP_EXE)
        set(${result_var} ${SEVEN_ZIP_EXE} PARENT_SCOPE)
    else()
        set(${result_var} "" PARENT_SCOPE)
    endif()
endfunction()

# Extract 7z archive
function(extract_7z_archive archive_path extract_to)
    find_7z_executable(SEVEN_ZIP)
    
    if(NOT SEVEN_ZIP)
        message(WARNING "7z executable not found. Cannot extract ${archive_path}")
        message(WARNING "Please install 7-Zip from: https://www.7-zip.org/")
        return()
    endif()
    
    if(NOT EXISTS "${archive_path}")
        message(WARNING "Archive not found: ${archive_path}")
        return()
    endif()
    
    # Create destination directory
    file(MAKE_DIRECTORY "${extract_to}")
    
    # Extract archive
    message(STATUS "Extracting ${archive_path} to ${extract_to}...")
    execute_process(
        COMMAND "${SEVEN_ZIP}" x "${archive_path}" "-o${extract_to}" -y
        RESULT_VARIABLE result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(result EQUAL 0)
        message(STATUS "Successfully extracted ${archive_path}")
    else()
        message(WARNING "Failed to extract ${archive_path}")
    endif()
endfunction()

# Main installation function
function(rbc_install_resources)
    # Project root directory
    set(PROJECT_ROOT "${CMAKE_SOURCE_DIR}")
    
    # Determine output directory using generator expressions for multi-config support
    # For single-config generators, CMAKE_RUNTIME_OUTPUT_DIRECTORY is used directly
    # For multi-config generators (Visual Studio, Xcode), we need $<CONFIG>
    get_cmake_property(IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
    
    if(IS_MULTI_CONFIG)
        # Multi-config generator: output goes to bin/$<CONFIG>/
        if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
            set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
        else()
            set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
        endif()
    else()
        # Single-config generator
        if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
            set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
        endif()
    endif()
    
    # Python script path
    set(PYTHON_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/rbc_install_resources_post_build.py")
    
    # Find uv executable
    find_program(UV_EXECUTABLE
        NAMES uv
        DOC "uv executable for running Python scripts in project environment"
    )
    
    if(NOT UV_EXECUTABLE)
        message(WARNING "uv executable not found. Resource installation will be skipped.")
        message(WARNING "Please install uv from: https://docs.astral.sh/uv/")
        message(WARNING "Or ensure uv is in PATH.")
        message(WARNING "You can manually run: uv run python ${PYTHON_SCRIPT} ${PROJECT_ROOT} <output_dir>")
        return()
    endif()
    
    # Create custom target for resource installation
    add_custom_target(rbc_install_resources ALL
        COMMENT "Installing RoboCute resources..."
        VERBATIM
    )
    
    # Run Python script using uv to ensure correct environment
    # Use a wrapper script to handle errors gracefully and prevent build failure
    if(WIN32)
        set(WRAPPER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/rbc_run_install_wrapper.bat")
        if(NOT EXISTS "${WRAPPER_SCRIPT}")
            message(WARNING "Wrapper script not found: ${WRAPPER_SCRIPT}")
            message(WARNING "Resource installation will be skipped.")
            return()
        endif()
        add_custom_command(TARGET rbc_install_resources POST_BUILD
            COMMAND "${WRAPPER_SCRIPT}"
                "${UV_EXECUTABLE}"
                "${PYTHON_SCRIPT}"
                "${PROJECT_ROOT}"
                "$<SHELL_PATH:${OUTPUT_DIR}>"
            WORKING_DIRECTORY "${PROJECT_ROOT}"
            COMMENT "Installing resources (extracting archives and copying shaders)..."
            VERBATIM
        )
    else()
        set(WRAPPER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/rbc_run_install_wrapper.sh")
        if(NOT EXISTS "${WRAPPER_SCRIPT}")
            message(WARNING "Wrapper script not found: ${WRAPPER_SCRIPT}")
            message(WARNING "Resource installation will be skipped.")
            return()
        endif()
        add_custom_command(TARGET rbc_install_resources POST_BUILD
            COMMAND /bin/bash
                "${WRAPPER_SCRIPT}"
                "${UV_EXECUTABLE}"
                "${PYTHON_SCRIPT}"
                "${PROJECT_ROOT}"
                "$<SHELL_PATH:${OUTPUT_DIR}>"
            WORKING_DIRECTORY "${PROJECT_ROOT}"
            COMMENT "Installing resources (extracting archives and copying shaders)..."
            VERBATIM
        )
    endif()
    
    message(STATUS "Resource installation target 'rbc_install_resources' configured")
    message(STATUS "Output directory: ${OUTPUT_DIR}")
    message(STATUS "Using uv: ${UV_EXECUTABLE}")
    message(STATUS "Python script: ${PYTHON_SCRIPT}")
endfunction()

