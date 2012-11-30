# Try to find XrdCl
# Once done, this will define
#
# XRDCL_FOUND       - system has XrdCl
# XRDCL_INCLUDE_DIR - the XrdCl include directory
# XRDCL_LIB_DIR     - the XrdCl library directory
#
# XRDCL_DIR may be defined as a hint for where to look

FIND_PATH(XRDCL_INCLUDE_DIR XrdCl/XrdClFile.hh
  HINTS
  ${XROOTD_DIR}
  $ENV{XROOTD_DIR}
  /usr
  /usr/local
  /opt/xrootd/
  PATH_SUFFIXES include/xrootd/
  PATHS /opt/xrootd/
)

FIND_LIBRARY(XRDCL_LIB XrdCl
  HINTS
  ${XROOTD_DIR}
  $ENV{XROOTD_DIR}
  /usr
  /usr/local
  /opt/xrootd/
  PATH_SUFFIXES lib
)

GET_FILENAME_COMPONENT( XRDCL_LIB_DIR ${XRDCL_LIB} PATH )

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XrdCl DEFAULT_MSG XRDCL_LIB_DIR XRDCL_INCLUDE_DIR )
