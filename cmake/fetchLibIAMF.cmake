# fetchLibIAMF.cmake - Fetch and configure libiamf and iamf-tools for IAMF/Eclipsa support

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# Fetch libiamf (reference decoder)
FetchContent_Declare(
  libiamf
  GIT_REPOSITORY https://github.com/AOMediaCodec/libiamf.git
  GIT_TAG        v1.1.0  # Use stable release version
  GIT_SHALLOW    TRUE  # For faster cloning
)

# Fetch iamf-tools (encoder and tools)
FetchContent_Declare(
  iamf-tools
  GIT_REPOSITORY https://github.com/AOMediaCodec/iamf-tools.git
  GIT_TAG        v1.0.0  # Use stable release version
  GIT_SHALLOW    TRUE  # For faster cloning
)

FetchContent_GetProperties(libiamf)
FetchContent_GetProperties(iamf-tools)

# Populate libiamf
if (NOT libiamf_POPULATED)
    message(STATUS "Fetching libiamf...")
    FetchContent_Populate(libiamf)
    
    set(LIBIAMF_SOURCE_DIR "${libiamf_SOURCE_DIR}")
    set(LIBIAMF_BINARY_DIR "${libiamf_BINARY_DIR}")
    
    # Set libiamf build options
    option(LIBIAMF_BUILD_TESTS "Build libiamf tests" OFF)
    option(LIBIAMF_BUILD_EXAMPLES "Build libiamf examples" OFF)
    
    # Add libiamf as a subdirectory if it has CMakeLists.txt
    if(EXISTS "${libiamf_SOURCE_DIR}/code/CMakeLists.txt")
        # Force static library build for better compatibility
        set(BUILD_SHARED_LIBS_BACKUP ${BUILD_SHARED_LIBS})
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static library for IAMF" FORCE)
        
        # Disable codec dependencies that we don't need for basic validation
        set(CODEC_CAP OFF CACHE BOOL "Disable codec capability check" FORCE)
        
        add_subdirectory(${libiamf_SOURCE_DIR}/code ${libiamf_BINARY_DIR})
        
        # Restore original BUILD_SHARED_LIBS setting
        set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_BACKUP} CACHE BOOL "Restore original setting" FORCE)
    endif()
    
    message(STATUS "libiamf source: ${LIBIAMF_SOURCE_DIR}")
endif()

# Populate iamf-tools
if (NOT iamf-tools_POPULATED)
    message(STATUS "Fetching iamf-tools...")
    FetchContent_Populate(iamf-tools)
    
    set(IAMFTOOLS_SOURCE_DIR "${iamf-tools_SOURCE_DIR}")
    set(IAMFTOOLS_BINARY_DIR "${iamf-tools_BINARY_DIR}")
    
    # Set iamf-tools build options
    option(IAMFTOOLS_BUILD_TESTS "Build iamf-tools tests" OFF)
    option(IAMFTOOLS_BUILD_EXAMPLES "Build iamf-tools examples" ON)
    
    # iamf-tools uses Bazel by default, but we can try to integrate key components
    # For now, let's set up include paths and prepare for manual integration
    
    message(STATUS "iamf-tools source: ${IAMFTOOLS_SOURCE_DIR}")
endif()

# Set up include directories for IAMF libraries
set(IAMF_INCLUDE_DIRS 
    ${LIBIAMF_SOURCE_DIR}/code/include
    ${LIBIAMF_SOURCE_DIR}/code/src
    ${IAMFTOOLS_SOURCE_DIR}/iamf
    ${IAMFTOOLS_SOURCE_DIR}
)

# Try to find any built IAMF libraries
find_library(LIBIAMF_LIBRARY
    NAMES iamf libiamf
    PATHS ${LIBIAMF_BINARY_DIR} ${LIBIAMF_BINARY_DIR}/lib
    NO_DEFAULT_PATH
)

# Set up preprocessor definitions for IAMF support
add_definitions(-DHAVE_LIBIAMF=1)
add_definitions(-DIAMF_ECLIPSA_SUPPORT=1)

# Include IAMF headers
include_directories(${IAMF_INCLUDE_DIRS})

# Function to create aliases for the IAMF library target
function(setup_iamf_targets)
    # If the actual iamf target exists (from libiamf), create an alias
    if(TARGET iamf)
        # Create an alias for easier reference
        if(NOT TARGET iamf_simple)
            add_library(iamf_simple ALIAS iamf)
        endif()
        message(STATUS "Using official libiamf target")
        set(IAMF_LIBRARIES iamf CACHE INTERNAL "IAMF library target")
    else()
        message(WARNING "libiamf target not found - IAMF support may be limited")
    endif()
endfunction()

# Call the function to setup targets (only after add_subdirectory)
if(EXISTS "${libiamf_SOURCE_DIR}/code/CMakeLists.txt")
    setup_iamf_targets()
endif()

# Print status information
message(STATUS "IAMF_INCLUDE_DIRS: ${IAMF_INCLUDE_DIRS}")
if(LIBIAMF_LIBRARY)
    message(STATUS "libiamf library found: ${LIBIAMF_LIBRARY}")
elseif(TARGET iamf_simple)
    message(STATUS "Created simple IAMF target from sources")
else()
    message(STATUS "IAMF will be header-only integration")
endif()

# Set variables for use in parent CMakeLists.txt
set(IAMF_FOUND TRUE CACHE INTERNAL "IAMF library found")
set(IAMF_LIBRARIES ${LIBIAMF_LIBRARY} CACHE INTERNAL "IAMF library")
set(IAMF_INCLUDE_DIRS ${IAMF_INCLUDE_DIRS} CACHE INTERNAL "IAMF include directories") 