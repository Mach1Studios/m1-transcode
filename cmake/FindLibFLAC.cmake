#-------------------------------------------------------------------------------
#
# Finds libflac include file and library. This module sets the following
# variables:
#
#  LIBFLAC_FOUND        - Flag if libflac was found
#  LIBFLAC_INCLUDE_DIRS - libflac include directories
#  LIBFLAC_LIBRARIES    - libflac library paths
#
#-------------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)

find_path(LIBFLAC_INCLUDE_DIRS FLAC/all.h)
find_library(LIBFLAC_LIBRARIES FLAC)

find_package_handle_standard_args(
    LibFLAC
    DEFAULT_MSG
    LIBFLAC_LIBRARIES
    LIBFLAC_INCLUDE_DIRS
)

#-------------------------------------------------------------------------------