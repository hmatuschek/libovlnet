# - Find libopus
#
#  OPUS_INCLUDE_DIRS - where to find opus.h
#  OPUS_LIBRARIES    - List of libraries when using libopus.
#  OPUS_FOUND        - True if libopus found.

if(OPUS_INCLUDE_DIRS)
  # Already in cache, be silent
  set(OPUS_FIND_QUIETLY TRUE)
endif(OPUS_INCLUDE_DIRS)

find_path(OPUS_INCLUDE_DIRS opus.h PATH_SUFFIXES opus)
find_library(OPUS_LIBRARY NAMES opus)

set(OPUS_LIBRARIES ${OPUS_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set OPUS_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus DEFAULT_MSG OPUS_LIBRARIES OPUS_INCLUDE_DIRS)

mark_as_advanced(OPUS_LIBRARIES OPUS_INCLUDE_DIRS)
