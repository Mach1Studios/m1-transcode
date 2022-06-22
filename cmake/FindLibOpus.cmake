#-------------------------------------------------------------------------------
#
# Finds libopus include file and library. This module sets the following
# variables:
#
#  LIBOPUS_FOUND        - Flag if libopus was found
#  LIBOPUS_INCLUDE_DIRS - libopus include directory
#  LIBOPUS_LIBRARIES    - libopus library path
#
#-------------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)

find_path(LIBOPUS_INCLUDE_DIRS opus/opus.h)
find_library(LIBOPUS_LIBRARIES opus)

find_package_handle_standard_args(
    LibOpus
    DEFAULT_MSG
    LIBOPUS_LIBRARIES
    LIBOPUS_INCLUDE_DIRS
)

#-------------------------------------------------------------------------------