# Finds mp3lame include file and library. This module sets the following
# variables:
#
#  mp3lame_FOUND - system has libmp3lame
#  mp3lame_INCLUDE_DIRS - the libmp3lame include directories
#  mp3lame_LIBRARIES - link these to use libmp3lame

find_path(mp3lame_INCLUDE_DIR
  NAMES lame/lame.h
  DOC "LAME include directory")
mark_as_advanced(mp3lame_INCLUDE_DIR)

find_library(mp3lame_LIBRARY
  NAMES mp3lame mp3lame-static
  DOC "LAME library"
)
mark_as_advanced(mp3lame_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  mp3lame
  DEFAULT_MSG
  mp3lame_LIBRARY
  mp3lame_INCLUDE_DIR
)

if(mp3lame_FOUND)
  set(mp3lame_LIBRARIES "${mp3lame_LIBRARY}")
  set(mp3lame_INCLUDE_DIRS "${mp3lame_INCLUDE_DIR}")

  if(NOT TARGET mp3lame::mp3lame)
    add_library(mp3lame::mp3lame UNKNOWN IMPORTED)
    set_target_properties(mp3lame::mp3lame
      PROPERTIES
        IMPORTED_LOCATION "${mp3lame_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${mp3lame_INCLUDE_DIR}"
    )
  endif()
endif()
