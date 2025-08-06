#----------------
# Setup
#----------------

cmake_minimum_required (VERSION 3.5)
project(m1-transcode)
set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: default is Release")
endif()

message(STATUS "CMAKE_VERSION=${CMAKE_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Prepend our CMake modules directory
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)
message(STATUS "CMAKE_MODULE_PATH='${CMAKE_MODULE_PATH}'")

#----------------
# Version
#----------------

# Read version number from VERSION file and split into its component parts.

file(STRINGS "VERSION" VERSION)

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" VERSION_PARTS ${VERSION})

set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})
set(VERSION_PATCH ${CMAKE_MATCH_3})

message(STATUS "Building version ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

#----------------
# Dependencies
#----------------

# Force searching for static deps
if(WIN32)
    list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .lib .a)
else()
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)

# download m1-sdk
include(cmake/fetchM1SDK.cmake)

# Fetch and configure libsndfile
include(cmake/fetchLibSndFile.cmake)

# Fetch and configure libadm
include(cmake/fetchLibADM.cmake)

# Fetch and configure libbw64
include(cmake/fetchLibBW64.cmake)

# Add IAMF support for Eclipsa file authoring
option(ENABLE_IAMF_SUPPORT "Enable IAMF/Eclipsa file support" ON)
if(ENABLE_IAMF_SUPPORT)
    include(cmake/fetchLibIAMF.cmake)
endif()

#----------------
# Compiler flags
#----------------

set(COMMON_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -DBOOST_FILESYSTEM_NO_DEPRECATED")
set(CMAKE_C_FLAGS ${COMMON_FLAGS})

if(APPLE)
    set(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}")
elseif(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

message(STATUS "CMAKE_CXX_COMPILER_VERSION='${CMAKE_CXX_COMPILER_VERSION}'")
message(STATUS "CMAKE_CXX_FLAGS='${CMAKE_CXX_FLAGS}'")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG='${CMAKE_CXX_FLAGS_DEBUG}'")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE='${CMAKE_CXX_FLAGS_RELEASE}'")
message(STATUS "CMAKE_CXX_COMPILE_OBJECT='${CMAKE_CXX_COMPILE_OBJECT}'")

#----------------
# Sources
#----------------

# collect and add source files
set(SOURCES
    src/main.cpp
    src/adapters/ADMParse.cpp
)

# Add IAMF integration sources if enabled
if(ENABLE_IAMF_SUPPORT AND IAMF_FOUND)
    set(SOURCES ${SOURCES}
        src/adapters/iamf.cpp
    )
endif()

# create the executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

# Add IAMF validation tool with IAMF library support
add_executable(iamf_validator iamf_validator.cpp)
target_include_directories(iamf_validator PRIVATE ${PROJECT_INCLUDE_DIRS})
target_compile_definitions(iamf_validator PRIVATE ${PROJECT_COMPILE_DEFINITIONS})

# Link IAMF libraries to the validator
if(ENABLE_IAMF_SUPPORT AND IAMF_FOUND)
    # Try linking to the actual iamf target first
    if(TARGET iamf)
        target_link_libraries(iamf_validator PRIVATE iamf)
        message(STATUS "Linking iamf_validator with iamf target")
    elseif(TARGET iamf_simple)
        target_link_libraries(iamf_validator PRIVATE iamf_simple)
        message(STATUS "Linking iamf_validator with iamf_simple target")
    elseif(IAMF_LIBRARIES)
        target_link_libraries(iamf_validator PRIVATE ${IAMF_LIBRARIES})
        message(STATUS "Linking iamf_validator with IAMF_LIBRARIES: ${IAMF_LIBRARIES}")
    else()
        message(WARNING "No IAMF library target found")
    endif()
    
    # Add IAMF include directories
    if(IAMF_INCLUDE_DIRS)
        target_include_directories(iamf_validator PRIVATE ${IAMF_INCLUDE_DIRS})
    endif()
else()
    message(WARNING "IAMF support not enabled or not found - iamf_validator may not work correctly")
endif()

message(STATUS "Added iamf_validator with IAMF library dependencies")

# Add M1-SDK sources directly to target (following example pattern)
if(DEFINED MACH1SPATIAL_SOURCES)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MACH1SPATIAL_SOURCES})
endif()

#----------------
# Link
#----------------

# include headers
include_directories(${LIBSNDFILE_INCLUDE_DIRS})
include_directories(${LIBADM_INCLUDE_DIRS})
include_directories(${LIBBW64_INCLUDE_DIRS})

set(PROJECT_INCLUDE_DIRS src)
if(DEFINED MACH1SPATIAL_INCLUDES)
    list(APPEND PROJECT_INCLUDE_DIRS ${MACH1SPATIAL_INCLUDES})
endif()
include_directories(${PROJECT_INCLUDE_DIRS})

# Link libraries
set(LIBS
    sndfile
    adm
    bw64
)

# Add IAMF libraries if enabled
if(ENABLE_IAMF_SUPPORT AND IAMF_FOUND)
    if(TARGET iamf_simple)
        set(LIBS ${LIBS} iamf_simple)
    elseif(IAMF_LIBRARIES)
        set(LIBS ${LIBS} ${IAMF_LIBRARIES})
    endif()
endif()

# Link M1Transcode targets if available (from m1-sdk)
if(TARGET M1Transcode)
    set(LIBS ${LIBS} M1Transcode)
    message(STATUS "Linking with M1Transcode target")
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBS})

#----------------
# Install
#----------------

message(STATUS "CMAKE_INSTALL_PREFIX='${CMAKE_INSTALL_PREFIX}'")
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION build)

#-------------------------------------------------------------------------------
