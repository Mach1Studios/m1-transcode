#-------------------------------------------------------------------------------
#
# Finds libsndfile include file and library. This module sets the following
# variables:
#
#  LIBSNDFILE_FOUND        - Flag if libsndfile was found
#  LIBSNDFILE_INCLUDE_DIRS - libsndfile include directories
#  LIBSNDFILE_LIBRARIES    - libsndfile library paths
#
#-------------------------------------------------------------------------------

include(FindPackageHandleStandardArgs)

find_path(LIBSNDFILE_INCLUDE_DIRS sndfile.h)
find_library(LIBSNDFILE_LIBRARIES sndfile)

find_package(LibFLAC REQUIRED)
find_package(LibVorbis REQUIRED)
find_package(LibOgg REQUIRED)
find_package(LibOpus REQUIRED)
find_package(LibMp3Lame REQUIRED)
find_package(MPG123 REQUIRED)

list(
    APPEND
    LIBSNDFILE_LIBRARIES
    ${LIBFLAC_LIBRARIES}
    ${LIBVORBIS_LIBRARIES}
    ${LIBOGG_LIBRARIES}
    ${LIBOPUS_LIBRARIES}
    ${libmp3lame_LIBRARIES}
    ${MPG123_LIBRARIES}
)

list(
    APPEND
    LIBSNDFILE_INCLUDE_DIRS
    ${LIBFLAC_INCLUDE_DIRS}
    ${LIBVORBIS_INCLUDE_DIRS}
    ${LIBOGG_INCLUDE_DIRS}
    ${LIBOPUS_INCLUDE_DIRS}
    ${libmp3lame_INCLUDE_DIRS}
    ${MPG123_INCLUDE_DIR}
)

find_package_handle_standard_args(
    LibSndFile
    DEFAULT_MSG
    LIBSNDFILE_LIBRARIES
    LIBSNDFILE_INCLUDE_DIRS
)

#-------------------------------------------------------------------------------
