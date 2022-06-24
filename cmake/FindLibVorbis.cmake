# Finds libvorbis and libvorbisenc include files and libraries. This module sets
# the following variables:
#
#  LIBVORBIS_FOUND        - Flag if libvorbis was found
#  LIBVORBIS_INCLUDE_DIRS - libvorbis include directories
#  LIBVORBIS_LIBRARIES    - libvorbis and libvorbisenc library paths

include(FindPackageHandleStandardArgs)

find_path(LIBVORBIS_INCLUDE_DIRS vorbis/vorbisenc.h)
find_library(LIBVORBIS_LIBRARY vorbis)
find_library(LIBVORBISENC_LIBRARY vorbisenc)

set(LIBVORBIS_LIBRARIES ${LIBVORBIS_LIBRARY} ${LIBVORBISENC_LIBRARY})

find_package_handle_standard_args(
    LibVorbis
    DEFAULT_MSG
    LIBVORBIS_LIBRARIES
    LIBVORBIS_INCLUDE_DIRS
)
