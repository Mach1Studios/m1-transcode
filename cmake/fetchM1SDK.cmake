# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

# Mach1 Spatial SDK
# Use these options to block dependencies we do not need
set(BUILD_M1DECODE OFF)
set(BUILD_M1DECODEPOSITIONAL OFF)
set(BUILD_MACH1SPATIAL_COMBINED_LIB OFF) # We do not need the combined lib
set(M1S_BUILD_EXAMPLES OFF)
set(M1S_BUILD_TESTS OFF)
set(M1S_BUILD_SIGNAL_SUITE OFF)

include(FetchContent)

# Fetch the full m1-sdk repository
FetchContent_Declare(
  m1-sdk
  GIT_REPOSITORY https://github.com/Mach1Studios/m1-sdk.git
  GIT_TAG        main  # Use latest main branch, or specify a specific tag/commit
  GIT_SHALLOW    TRUE  # For faster cloning
)

FetchContent_GetProperties(m1-sdk)

if (NOT m1-sdk_POPULATED)
    FetchContent_Populate(m1-sdk)
    
    # Set the path to the m1-sdk source
    set(MACH1SPATIAL_SDK_PATH "${m1-sdk_SOURCE_DIR}")
    message(STATUS "M1-SDK source path: ${MACH1SPATIAL_SDK_PATH}")
    
    # Add the libmach1spatial subdirectory as shown in the example
    set(M1_LIB_PATH ${MACH1SPATIAL_SDK_PATH}/libmach1spatial)
    if(EXISTS "${M1_LIB_PATH}/CMakeLists.txt")
        #list(INSERT CMAKE_MODULE_PATH 0 ${m1-sdk_BINARY_DIR}/cmake) # Prepend the global CMake modules directory
        add_subdirectory(${M1_LIB_PATH} ${m1-sdk_BINARY_DIR}/libmach1spatial)
        message(STATUS "Added libmach1spatial subdirectory")
    else()
        message(WARNING "libmach1spatial CMakeLists.txt not found at ${M1_LIB_PATH}")
    endif()
endif()

# Set up include directories
set(MACH1SPATIAL_INCLUDES 
    ${M1_LIB_PATH}/api_transcode/include
    ${M1_LIB_PATH}/api_common/include
    ${M1_LIB_PATH}/deps
    ${M1_LIB_PATH}/deps/yaml 
    ${M1_LIB_PATH}/deps/pugixml/src
)

# Include headers
include_directories(${MACH1SPATIAL_INCLUDES})

# Set up M1 source files that need to be included
set(MACH1SPATIAL_SOURCES
    ${M1_LIB_PATH}/deps/M1DSP/M1DSPUtilities.h
    ${M1_LIB_PATH}/deps/M1DSP/M1DSPFilters.h
    ${M1_LIB_PATH}/deps/M1DSP/M1DSPUtilities.cpp
    ${M1_LIB_PATH}/deps/M1DSP/M1DSPFilters.cpp
    ${M1_LIB_PATH}/api_common/include/Mach1AudioTimeline.cpp
    ${M1_LIB_PATH}/api_common/src/Mach1AudioTimelineCAPI.cpp
    ${M1_LIB_PATH}/api_common/src/Mach1AudioTimelineCore.h
    ${M1_LIB_PATH}/api_common/src/Mach1AudioTimelineCore.cpp
    ${M1_LIB_PATH}/deps/yaml/yaml/Yaml.cpp
    ${M1_LIB_PATH}/deps/pugixml/src/pugixml.cpp
)

# Print status information
message(STATUS "MACH1SPATIAL_SDK_PATH: ${MACH1SPATIAL_SDK_PATH}")
message(STATUS "M1_LIB_PATH: ${M1_LIB_PATH}")

# Note: The M1Transcode target will be available from the libmach1spatial subdirectory
# and should be linked using: target_link_libraries(${PROJECT_NAME} PRIVATE M1Transcode)
