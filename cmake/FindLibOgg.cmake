#-------------------------------------------------------------------------------
#
# Finds libogg include file and library. This module sets the following
# variables:
#
#  LIBOGG_FOUND        - Flag if libogg was found
#  LIBOGG_INCLUDE_DIRS - libogg include directory
#  LIBOGG_LIBRARIES    - libogg library path
#
#-------------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)

find_path(LIBOGG_INCLUDE_DIRS ogg/ogg.h)
find_library(LIBOGG_LIBRARIES ogg)

find_package_handle_standard_args(
    LibOgg
    DEFAULT_MSG
    LIBOGG_LIBRARIES
    LIBOGG_INCLUDE_DIRS
)

#-------------------------------------------------------------------------------
