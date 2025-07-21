# fetchLibSndFile.cmake - Fetch and configure libsndfile

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

# Fetch libsndfile
FetchContent_Declare(
  libsndfile
  GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
  GIT_TAG        master  # Use latest master branch, or specify a specific tag/commit
  GIT_SHALLOW    TRUE  # For faster cloning
)

FetchContent_GetProperties(libsndfile)

if (NOT libsndfile_POPULATED)
    message(STATUS "Fetching libsndfile...")
    FetchContent_Populate(libsndfile)
    
    set(LIBSNDFILE_SOURCE_DIR "${libsndfile_SOURCE_DIR}")
    set(LIBSNDFILE_BINARY_DIR "${libsndfile_BINARY_DIR}")
    
    # Set libsndfile build options
    option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)
    option(BUILD_PROGRAMS "Build programs" OFF)
    option(BUILD_EXAMPLES "Build examples" OFF)
    option(ENABLE_CPACK "Enable CPack" OFF)
    option(BUILD_TESTING "Build tests" OFF)
    
    # Add libsndfile as a subdirectory
    if(EXISTS "${libsndfile_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(${libsndfile_SOURCE_DIR} ${libsndfile_BINARY_DIR})
        message(STATUS "Added libsndfile subdirectory")
    else()
        message(WARNING "libsndfile CMakeLists.txt not found at ${libsndfile_SOURCE_DIR}")
    endif()
    
    message(STATUS "libsndfile source: ${LIBSNDFILE_SOURCE_DIR}")
endif()

# Set up include directories for libsndfile
set(LIBSNDFILE_INCLUDE_DIRS 
    ${LIBSNDFILE_SOURCE_DIR}/include
)

# Include headers
include_directories(${LIBSNDFILE_INCLUDE_DIRS})

# Print status information
message(STATUS "LIBSNDFILE_SOURCE_DIR: ${LIBSNDFILE_SOURCE_DIR}")
message(STATUS "LIBSNDFILE_INCLUDE_DIRS: ${LIBSNDFILE_INCLUDE_DIRS}")

# Set variables for use in parent CMakeLists.txt
set(LIBSNDFILE_FOUND TRUE CACHE INTERNAL "libsndfile library found")
set(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE_INCLUDE_DIRS} CACHE INTERNAL "libsndfile include directories") 