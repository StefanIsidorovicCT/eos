# Try to find uuid
# Once done, this will define
#
# REDOX_FOUND        - system has redox i.e. redis C++ client
# REDOX_INCLUDE_DIRS - redox include directories
# REDOX_LIBRARIES    - libraries needed to use redox

if(REDOX_INCLUDE_DIRS AND REDOX_LIBRARIES)
  set(REDOX_FIND_QUIETLY TRUE)
else()
  find_path(
    REDOX_INCLUDE_DIR
    NAMES redox.hpp redoxSet.hpp redoxHash.hpp
    HINTS ${REDOX_ROOT_DIR}
    PATH_SUFFIXES include)

  find_library(
    REDOX_LIBRARY
    NAMES redox
    HINTS ${REDOX_ROOT_DIR}
    PATH_SUFFIXES ${LIBRARY_PATH_PREFIX})

  set(REDOX_INCLUDE_DIRS ${REDOX_INCLUDE_DIR})
  set(REDOX_LIBRARIES ${REDOX_LIBRARY})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    redox DEFAULT_MSG REDOX_LIBRARY REDOX_INCLUDE_DIR)

  mark_as_advanced(REDOX_INCLUDE_DIR REDOX_LIBRARY)
endif()

# If Redox found set compilation definition, otherwise set the variables to
# strings so that they can be used in CMakeLists.txt without additional checks.
if(REDOX_FOUND)
  add_definitions(-DREDOX_FOUND)
else()
  set(REDOX_INCLUDE_DIR  "")
  set(REDOX_INCLUDE_DIRS "")
  set(REDOX_LIBRARY      "")
  set(REDOX_LIBRARIES    "")
endif()
