#----------------
# Setup
#----------------

cmake_minimum_required (VERSION 3.1..3.18)
project(m1-transcode)
set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: default is Release")
endif()

message(STATUS "CMAKE_VERSION=${CMAKE_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(DEFINED USE_EXTERNAL_M1SDK)
    set(USE_EXTERNAL_M1SDK 1)
    message(STATUS "Using preinstalled external M1-SDK via env vars")
    message(STATUS "Use the `M1SDK_PATH` var to set the path")
endif()

# Prepend our CMake modules directory
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)
message(STATUS "CMAKE_MODULE_PATH='${CMAKE_MODULE_PATH}'")

if (USE_EXTERNAL_M1SDK)
    if(NOT "$ENV{M1SDK_PATH}" STREQUAL "")
        set(MACH1SPATIAL_SDK_PATH "$ENV{M1SDK_PATH}" CACHE PATH "Copied from environment variable")
    else()
        message(FATAL_ERROR "unexpected: the local M1SDK_PATH environment variable is not set or is empty!")
    endif()
    message(STATUS "MACH1SPATIAL_SDK_PATH is set: ${MACH1SPATIAL_SDK_PATH}")

    if(WIN32 OR MSVC OR MINGW)
        if (NOT "$ENV{VCPKG_PATH}" STREQUAL "")
            set(VCPKG_PATH "$ENV{VCPKG_PATH}" CACHE INTERNAL "Copied from environment variable")
            message(STATUS "VCPKG_PATH is set: ${VCPKG_PATH}")
            include(${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                # 64 bits
                set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/vs-15-2017-x86_64)
            elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
                # 32 bits
                set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/vs-15-2017-x86)
            endif()
        else()
            message(FATAL_ERROR "unexpected: the local VCPKG_PATH environment variable is not set or is empty!")
        endif()
    elseif(APPLE)
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/xcode)
    elseif(UNIX AND NOT APPLE)
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/linux)
    else()
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/linux)
    endif()
    message(STATUS "MACH1SPATIAL_LIBS_PATH is set: ${MACH1SPATIAL_LIBS_PATH}")
endif()

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

#find_package(SndFile REQUIRED)
#if(LIBSNDFILE_FOUND)
#    message(STATUS "LIBSNDFILE_INCLUDE_DIRS='${LIBSNDFILE_INCLUDE_DIRS}'")
#    message(STATUS "LIBSNDFILE_LIBRARIES=${LIBSNDFILE_LIBRARIES}")
#    include_directories(${LIBSNDFILE_INCLUDE_DIRS})
#else()
    # Include subdirectories only if you manually installed them
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_PROGRAMS OFF)
    set(BUILD_EXAMPLES OFF)
    set(ENABLE_CPACK OFF)
    set(BUILD_TESTING OFF)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/libsndfile)
#endif(LIBSNDFILE_FOUND)

#find_package(adm)
#if(ADM_FOUND)
#    message(STATUS "ADM_INCLUDE_DIRS='${ADM_INCLUDE_DIRS}'")
#    message(STATUS "ADM_LIBRARIES=${ADM_LIBRARIES}")
#    include_directories(${ADM_INCLUDE_DIRS})
#else()
    # Include subdirectories only if you manually installed them
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/libadm)
#endif()

#find_package(bw64)
#if(BW64_FOUND)
#    message(STATUS "BW64_INCLUDE_DIRS='${BW64_INCLUDE_DIRS}'")
#    message(STATUS "BW64_LIBRARIES=${BW64_LIBRARIES}")
#    include_directories(${BW64_INCLUDE_DIRS})
#else()
    # Include subdirectories only if you manually installed them
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/libbw64)
#endif()

#----------------
# Compiler flags
#----------------

set(COMMON_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -DBOOST_FILESYSTEM_NO_DEPRECATED")
set(CMAKE_C_FLAGS ${COMMON_FLAGS})

if(APPLE)
    set(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}")
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
    src/ADMParse.cpp
)

if (USE_EXTERNAL_M1SDK)
    set(SOURCES ${SOURCES}
        ${MACH1SPATIAL_LIBS_PATH}/include/Mach1Transcode.cpp
        ${MACH1SPATIAL_LIBS_PATH}/include/Mach1AudioTimeline.cpp
        ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPFilters.cpp
        ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPDynamics.cpp
        ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPUtilities.cpp
    )
else()
    set(SOURCES ${SOURCES}
        ${MACH1SPATIAL_LIBS_PATH}/xcode/include/Mach1Transcode.cpp
        ${MACH1SPATIAL_LIBS_PATH}/xcode/include/Mach1AudioTimeline.cpp
        ${MACH1SPATIAL_LIBS_PATH}/xcode/include/M1DSP/M1DSPFilters.cpp
        ${MACH1SPATIAL_LIBS_PATH}/xcode/include/M1DSP/M1DSPDynamics.cpp
        ${MACH1SPATIAL_LIBS_PATH}/xcode/include/M1DSP/M1DSPUtilities.cpp
    )
endif()
message(STATUS "M1SDK Lib Path: '${MACH1SPATIAL_LIBS_PATH}'")

# create the executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

#----------------
# Link
#----------------

# include headers
if (USE_EXTERNAL_M1SDK)
    set(MACH1SPATIAL_INCLUDE_DIRS src libs ${MACH1SPATIAL_LIBS_PATH}/include)
    include_directories(${MACH1SPATIAL_INCLUDE_DIRS})
endif()
include_directories(${LIBSNDFILE_INCLUDE_DIRS})

# link libraries
if (USE_EXTERNAL_M1SDK)
    if(WIN32 OR MSVC OR MINGW)
        message(STATUS "Adding windows OS flags")
        add_compile_definitions(M1_STATIC)
        # Change `PATH` to reflect `/MT` or `/MD` as needed!
        set(MACH1TRANSCODE_LIBRARY_DEBUG ${MACH1SPATIAL_LIBS_PATH}/lib/Static/MD/Debug/Mach1TranscodeCAPId.lib)
        set(MACH1TRANSCODE_LIBRARY_RELEASE ${MACH1SPATIAL_LIBS_PATH}/lib/Static/MD/Release/Mach1TranscodeCAPI.lib)
        set(MACH1TRANSCODE_LIBRARY debug ${MACH1TRANSCODE_LIBRARY_DEBUG} optimized ${MACH1TRANSCODE_LIBRARY_RELEASE})
        set(MACH1TRANSCODE_LIBRARIES debug ${MACH1TRANSCODE_LIBRARY_DEBUG} optimized ${MACH1TRANSCODE_LIBRARY_RELEASE})
    else()
        if(APPLE)
            set(MACH1SPATIAL_LIBS_NIX_PATH ${MACH1SPATIAL_LIBS_PATH}/xcode)
        else()
            set(MACH1SPATIAL_LIBS_NIX_PATH ${MACH1SPATIAL_LIBS_PATH}/linux)
        endif()
        find_library(MACH1TRANSCODE_LIBRARY
            NAMES Mach1TranscodeCAPI libMach1TranscodeCAPI
            HINTS ${MACH1SPATIAL_LIBS_NIX_PATH}/lib
        )
        set(MACH1TRANSCODE_LIBRARIES ${MACH1TRANSCODE_LIBRARY})
    endif()
endif()
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${MACH1TRANSCODE_LIBRARY})
message(STATUS "Mach1Transcode Library: '${MACH1TRANSCODE_LIBRARY}'")

set(LIBS
    sndfile
    adm
    bw64
)

if (USE_EXTERNAL_M1SDK)
    set(LIBS ${LIBS} ${MACH1TRANSCODE_LIBRARIES})
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBS})

#----------------
# Install
#----------------

message(STATUS "CMAKE_INSTALL_PREFIX='${CMAKE_INSTALL_PREFIX}'")
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION build)

#-------------------------------------------------------------------------------
