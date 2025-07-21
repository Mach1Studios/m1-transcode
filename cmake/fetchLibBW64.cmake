# fetchLibBW64.cmake - Fetch and configure libbw64

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# Fetch libbw64
FetchContent_Declare(
  libbw64
  GIT_REPOSITORY https://github.com/irt-open-source/libbw64.git
  GIT_TAG        master  # Use latest master branch, or specify a specific tag/commit
  GIT_SHALLOW    TRUE  # For faster cloning
)

FetchContent_GetProperties(libbw64)

if (NOT libbw64_POPULATED)
    message(STATUS "Fetching libbw64...")
    FetchContent_Populate(libbw64)
    
    set(LIBBW64_SOURCE_DIR "${libbw64_SOURCE_DIR}")
    set(LIBBW64_BINARY_DIR "${libbw64_BINARY_DIR}")
    
    # Set libbw64 build options
    option(BW64_EXAMPLES "Build libbw64 examples" OFF)
    option(BW64_UNIT_TESTS "Build libbw64 unit tests" OFF)
    option(BW64_PACKAGE_AND_INSTALL "Enable packaging and installation" OFF)
    
    # Add libbw64 as a subdirectory
    if(EXISTS "${libbw64_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(${libbw64_SOURCE_DIR} ${libbw64_BINARY_DIR})
        message(STATUS "Added libbw64 subdirectory")
    else()
        message(WARNING "libbw64 CMakeLists.txt not found at ${libbw64_SOURCE_DIR}")
    endif()
    
    message(STATUS "libbw64 source: ${LIBBW64_SOURCE_DIR}")
endif()

# Set up include directories for libbw64
set(LIBBW64_INCLUDE_DIRS 
    ${LIBBW64_SOURCE_DIR}/include
)

# Include headers
include_directories(${LIBBW64_INCLUDE_DIRS})

# Print status information
message(STATUS "LIBBW64_SOURCE_DIR: ${LIBBW64_SOURCE_DIR}")
message(STATUS "LIBBW64_INCLUDE_DIRS: ${LIBBW64_INCLUDE_DIRS}")

# Set variables for use in parent CMakeLists.txt
set(LIBBW64_FOUND TRUE CACHE INTERNAL "libbw64 library found")
set(LIBBW64_INCLUDE_DIRS ${LIBBW64_INCLUDE_DIRS} CACHE INTERNAL "libbw64 include directories") 