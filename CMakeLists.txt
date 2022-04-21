cmake_minimum_required(VERSION 3.19.3)
project(m1-transcode)
set(CMAKE_CXX_STANDARD 11)

if (NOT "$ENV{M1SDK_PATH}" STREQUAL "")
    set(MACH1SPATIAL_SDK_PATH "$ENV{M1SDK_PATH}" CACHE INTERNAL "Copied from environment variable")
    message("M1SDK_PATH is set: ${MACH1SPATIAL_SDK_PATH}")
    if(WIN32)
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/windows-x86)
    elseif(APPLE)
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/xcode)
    elseif(UNIX AND NOT APPLE)
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/linux)
    else()
        set(MACH1SPATIAL_LIBS_PATH ${MACH1SPATIAL_SDK_PATH}/mach1spatial-libs/linux)
    endif()
else()
  message(FATAL_ERROR "unexpected: the local M1SDK_PATH environment variable is not set or is empty!")
endif()

# Use pkg-config to get hints about paths
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBSNDFILE_PKGCONF sndfile)
endif(PKG_CONFIG_FOUND)

# - Try to find libsndfile
# Once done, this will define
#  LIBSNDFILE_FOUND - system has libsndfile
#  LIBSNDFILE_INCLUDE_DIRS - the libsndfile include directories
#  LIBSNDFILE_LIBRARIES - link these to use libsndfile

# Include dir
find_path(LIBSNDFILE_INCLUDE_DIR
        NAMES sndfile.h
        PATHS ${LIBSNDFILE_PKGCONF_INCLUDE_DIRS}
        )

# Library
find_library(LIBSNDFILE_LIBRARY
        NAMES sndfile libsndfile-1
        PATHS ${LIBSNDFILE_PKGCONF_LIBRARY_DIRS}
        )

include(FindPkgConfig)
find_package(PackageHandleStandardArgs)
# find_package(adm)
# find_package(bw64)
find_package_handle_standard_args(LibSndFile  DEFAULT_MSG  LIBSNDFILE_LIBRARY LIBSNDFILE_INCLUDE_DIR)

# Include subdirectories only if you manually installed
add_subdirectory(libs/libadm)
add_subdirectory(libs/libbw64)

# collect and add source files
set(SOURCES 
  src/main.cpp
  src/ADMParse.cpp
  ${MACH1SPATIAL_LIBS_PATH}/include/Mach1Transcode.cpp
  ${MACH1SPATIAL_LIBS_PATH}/include/Mach1AudioTimeline.cpp
  ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPFilters.cpp
  ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPDynamics.cpp
  ${MACH1SPATIAL_LIBS_PATH}/include/M1DSP/M1DSPUtilities.cpp
  )

# create the executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

if(LIBSNDFILE_FOUND)
    set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBRARY})
    set(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE_INCLUDE_DIR})
	mark_as_advanced(LIBSNDFILE_LIBRARY LIBSNDFILE_LIBRARIES LIBSNDFILE_INCLUDE_DIR LIBSNDFILE_INCLUDE_DIRS)
	pkg_search_module(SndFile sndfile)
else()
	set(LIBSNDFILE_INCLUDE_DIR ./libs/libsndfile/include)
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LIBSNDFILE_INCLUDE_DIR})
	set(LIBSNDFILE_INCLUDE_DIRS ${LIBSNDFILE_INCLUDE_DIR})
	if(WIN32)
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE "./libs/libsndfile/lib/vs/\$(Platform)")
		set(LIBSNDFILE_LIBRARY "sndfile.lib")
	elseif(APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -Wc++11-extensions")
        target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE "./libs/libsndfile/lib/osx")
        set(LIBSNDFILE_LIBRARY "sndfile.a")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        link_directories(${CMAKE_PROJECT_NAME} PRIVATE "./libs/libsndfile/lib/linux")
        set(LIBSNDFILE_LIBRARY "sndfile.a")
	endif()
	set(LIBSNDFILE_LIBRARIES ${LIBSNDFILE_LIBRARY})
endif(LIBSNDFILE_FOUND)

# link libraries
find_library(MACH1TRANSCODE_LIBRARY 
             NAMES Mach1TranscodeCAPI libMach1TranscodeCAPI libMach1TranscodeCAPI.a libMach1TranscodeCAPI.so Mach1TranscodeCAPI.lib
             PATHS ${MACH1SPATIAL_LIBS_PATH}/lib ${MACH1SPATIAL_LIBS_PATH}/windows-x86 ${MACH1SPATIAL_LIBS_PATH}/windows-x86_64
)

# include headers
set(MACH1SPATIAL_INCLUDES src libs ${MACH1SPATIAL_LIBS_PATH}/include ${MACH1SPATIAL_SDK_PATH}/public)
include_directories(${MACH1SPATIAL_INCLUDES})
include_directories(${LIBSNDFILE_INCLUDE_DIRS})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${MACH1TRANSCODE_LIBRARY})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${LIBSNDFILE_LIBRARIES})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE adm)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE bw64)

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION build)