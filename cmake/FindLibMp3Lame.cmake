#-------------------------------------------------------------------------------
#
# Finds libmp3lame include file and library. This module sets the following
# variables:
#
#  libmp3lame_FOUND - system has libmp3lame
#  libmp3lame_INCLUDE_DIRS - the libmp3lame include directories
#  libmp3lame_LIBRARIES - link these to use libmp3lame

include(FindPackageHandleStandardArgs)

# Include dir
find_path(libmp3lame_INCLUDE_DIR
          NAMES lame/lame.h
          PATHS ${libmp3lame_PKGCONF_INCLUDE_DIRS}
          )

# Finally the library itself
find_library(libmp3lame_LIBRARY
             NAMES libmp3lame
             PATHS ${libmp3lame_PKGCONF_LIBRARY_DIRS}
             )

find_package_handle_standard_args(
   libmp3lame
   DEFAULT_MSG
   libmp3lame_LIBRARIES
   libmp3lame_INCLUDE_DIRS
)

#-------------------------------------------------------------------------------
