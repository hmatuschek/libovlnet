# - Find libovlnet
#
#  OVLNET_INCLUDE_DIRS - where to find ovlnet.h
#  OVLNET_LIBRARIES    - List of libraries when using libovlnet.
#  OVLNET_FOUND        - True if libovlnet found.

if(OVLNET_INCLUDE_DIRS)
  # Already in cache, be silent
  set(OVLNET_FIND_QUIETLY TRUE)
endif(OVLNET_INCLUDE_DIRS)

find_path(OVLNET_INCLUDE_DIRS ovlnet.hh PATH_SUFFIXES ovlnet)
find_library(OVLNET_LIBRARY NAMES ovlnet)

set(OVLNET_LIBRARIES ${OVLNET_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set OVLNET_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OvlNet DEFAULT_MSG OVLNET_LIBRARIES OVLNET_INCLUDE_DIRS)

mark_as_advanced(OVLNET_LIBRARIES OVLNET_INCLUDE_DIRS)
