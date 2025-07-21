# fetchLibADM.cmake - Fetch and configure libadm

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# Fetch libadm
FetchContent_Declare(
  libadm
  GIT_REPOSITORY https://github.com/ebu/libadm.git
  GIT_TAG        master  # Use latest master branch, or specify a specific tag/commit
  GIT_SHALLOW    TRUE  # For faster cloning
)

FetchContent_GetProperties(libadm)

if (NOT libadm_POPULATED)
    message(STATUS "Fetching libadm...")
    FetchContent_Populate(libadm)
    
    set(LIBADM_SOURCE_DIR "${libadm_SOURCE_DIR}")
    set(LIBADM_BINARY_DIR "${libadm_BINARY_DIR}")
    
    # Set libadm build options
    option(ADM_EXAMPLES "Build libadm examples" OFF)
    option(ADM_UNIT_TESTS "Build libadm unit tests" OFF)
    option(ADM_PACKAGE_AND_INSTALL "Enable packaging and installation" OFF)
    
    # Add libadm as a subdirectory
    if(EXISTS "${libadm_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(${libadm_SOURCE_DIR} ${libadm_BINARY_DIR})
        message(STATUS "Added libadm subdirectory")
    else()
        message(WARNING "libadm CMakeLists.txt not found at ${libadm_SOURCE_DIR}")
    endif()
    
    message(STATUS "libadm source: ${LIBADM_SOURCE_DIR}")
endif()

# Set up include directories for libadm
set(LIBADM_INCLUDE_DIRS 
    ${LIBADM_SOURCE_DIR}/include
)

# Include headers
include_directories(${LIBADM_INCLUDE_DIRS})

# Print status information
message(STATUS "LIBADM_SOURCE_DIR: ${LIBADM_SOURCE_DIR}")
message(STATUS "LIBADM_INCLUDE_DIRS: ${LIBADM_INCLUDE_DIRS}")

# Set variables for use in parent CMakeLists.txt
set(LIBADM_FOUND TRUE CACHE INTERNAL "libadm library found")
set(LIBADM_INCLUDE_DIRS ${LIBADM_INCLUDE_DIRS} CACHE INTERNAL "libadm include directories") 