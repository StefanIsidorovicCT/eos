# ----------------------------------------------------------------------
# File: CMakeLists.txt
# Author: Andreas-Joachim Peters - CERN
# ----------------------------------------------------------------------

# ************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2011 CERN/Switzerland                                  *
# *                                                                      *
# * This program is free software: you can redistribute it and/or modify *
# * it under the terms of the GNU General Public License as published by *
# * the Free Software Foundation, either version 3 of the License, or    *
# * (at your option) any later version.                                  *
# *                                                                      *
# * This program is distributed in the hope that it will be useful,      *
# * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
# * GNU General Public License for more details.                         *
# *                                                                      *
# * You should have received a copy of the GNU General Public License    *
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
# ************************************************************************

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${XROOTD_INCLUDE_DIR}
  ${XROOTD_PRIVATE_INCLUDE_DIR}
  ${Z_INCLUDE_DIRS}
  ${ZMQ_INCLUDE_DIRS}
  ${JSONCPP_INCLUDE_DIR}
  ${LDAP_INCLUDE_DIRS}
  ${XROOTD_INCLUDE_DIRS}
  ${NCURSES_INCLUDE_DIRS}
  ${SPARSEHASH_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/auth_plugin/)

#-------------------------------------------------------------------------------
# XrdEosMgm library
#-------------------------------------------------------------------------------
set(XRDEOSMGM_SRCS
  Access.cc
  IConfigEngine.cc
  FileConfigEngine.cc
  RedisConfigEngine.cc
  GeoTreeEngine.cc
  Messaging.cc
  VstMessaging.cc
  Policy.cc
  ProcInterface.cc
  proc/proc_fs.cc
  proc/admin/Access.cc
  proc/admin/Backup.cc
  proc/admin/Config.cc
  proc/admin/Debug.cc
  proc/admin/Fs.cc
  proc/admin/Fsck.cc
  proc/admin/GeoSched.cc
  proc/admin/Group.cc
  proc/admin/Io.cc
  proc/admin/Node.cc
  proc/admin/Ns.cc
  proc/admin/Quota.cc
  proc/admin/Rtlog.cc
  proc/admin/Space.cc
  proc/admin/Transfer.cc
  proc/admin/Vid.cc
  proc/admin/Vst.cc
  proc/user/Attr.cc
  proc/user/Archive.cc
  proc/user/Cd.cc
  proc/user/Chmod.cc
  proc/user/Chown.cc
  proc/user/File.cc
  proc/user/Fileinfo.cc
  proc/user/Find.cc
  proc/user/Fuse.cc
  proc/user/Ls.cc
  proc/user/Map.cc
  proc/user/Member.cc
  proc/user/Mkdir.cc
  proc/user/Motd.cc
  proc/user/Quota.cc
  proc/user/Recycle.cc
  proc/user/Rm.cc
  proc/user/Rmdir.cc
  proc/user/Version.cc
  proc/user/Who.cc
  proc/user/Whoami.cc
  Quota.cc
  Scheduler.cc
  Vid.cc
  FsView.cc
  VstView.cc
  XrdMgmOfsConfigure.cc
  XrdMgmOfsFile.cc
  XrdMgmOfsDirectory.cc
  XrdMgmOfs.cc
  DrainJob.cc
  Balancer.cc
  FileSystem.cc
  Egroup.cc
  Acl.cc
  Stat.cc
  Iostat.cc
  Fsck.cc
  txengine/TransferEngine.cc
  txengine/TransferFsDB.cc
  ZMQ.cc
  Master.cc
  Recycle.cc
  LRU.cc
  WFE.cc
  Workflow.cc
  http/HttpServer.cc
  http/HttpHandler.cc
  http/s3/S3Handler.cc
  http/s3/S3Store.cc
  http/webdav/WebDAVHandler.cc
  http/webdav/WebDAVResponse.cc
  http/webdav/PropFindResponse.cc
  http/webdav/PropPatchResponse.cc
  http/webdav/LockResponse.cc
  Converter.cc
  GroupBalancer.cc
  GeoBalancer.cc
  Features.cc
  geotree/SchedulingTreeTest.cc
  geotree/SchedulingSlowTree.cc
  geotree/SchedulingTreeCommon.cc)

add_library(XrdEosMgm MODULE ${XRDEOSMGM_SRCS})

#-------------------------------------------------------------------------------
# Add dependecy to EosAuthProto so we guarantee that the protocol buffer files
# are generated when we try to build XrdEosMgm
#-------------------------------------------------------------------------------
add_dependencies(XrdEosMgm EosAuthProto)

target_compile_definitions(
  XrdEosMgm PUBLIC -DDAEMONUID=${DAEMONUID} -DDAEMONGID=${DAEMONGID})

if (MICROHTTPD_FOUND)
  target_compile_definitions(
    XrdEosMgm PUBLIC -DEOS_MICRO_HTTPD=1)
endif()

target_link_libraries(
  XrdEosMgm PUBLIC
  eosCommon
  eosCommonServer
  EosNsCommon
  EosPluginManager
  eosCapability-Static
  XrdMqClient-Static
  EosAuthProto
  ${Z_LIBRARY}
  ${ZMQ_LIBRARIES}
  ${LDAP_LIBRARIES}
  ${UUID_LIBRARIES}
  ${NCURSES_LIBRARY}
  ${GLIBC_DL_LIBRARY}
  ${REDOX_LIBRARIES}
  ${LIBEV_LIBRARIES}
  ${XROOTD_CL_LIBRARY}
  ${XROOTD_POSIX_LIBRARY}
  ${XROOTD_UTILS_LIBRARY}
  ${OPENSSL_CRYPTO_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${JSONCPP_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

#-------------------------------------------------------------------------------
# Create executables for testing the scheduling part
#-------------------------------------------------------------------------------
add_executable(testmgmview FsView.cc tests/MgmViewTest.cc)

add_executable(
  testschedulingtree
  geotree/SchedulingTreeTest.cc
  geotree/SchedulingSlowTree.cc
  geotree/SchedulingTreeCommon.cc)

target_compile_definitions(
  testmgmview PUBLIC -DEOSMGMFSVIEWTEST)

target_link_libraries(
  testmgmview
  eosCommon
  eosCommonServer
  XrdMqClient-Static
  eosCapability-Static
  ${Z_LIBRARY}
  ${ZMQ_LIBRARIES}
  ${UUID_LIBRARIES}
  ${NCURSES_LIBRARY}
  ${GLIBC_DL_LIBRARY}
  ${XROOTD_CL_LIBRARY}
  ${XROOTD_UTILS_LIBRARY}
  ${OPENSSL_CRYPTO_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${JSONCPP_LIBRARIES})

target_link_libraries(
  testschedulingtree
  eosCommon
  ${XROOTD_UTILS_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT})

#-------------------------------------------------------------------------------
# Create executables for testing the MGM configuration
#-------------------------------------------------------------------------------
if(CPPUNIT_FOUND AND REDOX_FOUND AND Linux)
  add_library(XrdEosMgm-Static STATIC ${XRDEOSMGM_SRCS})

  target_compile_definitions(
    XrdEosMgm-Static PUBLIC
    -DDAEMONUID=${DAEMONUID} -DDAEMONGID=${DAEMONGID})

  target_link_libraries(
    XrdEosMgm-Static PUBLIC
    EosNsCommon-Static
    eosCommon-Static
    eosCommonServer
    EosPluginManager
    eosCapability-Static
    XrdMqClient-Static
    EosAuthProto
    ${Z_LIBRARY}
    ${ZMQ_LIBRARIES}
    ${LDAP_LIBRARIES}
    ${UUID_LIBRARIES}
    ${NCURSES_LIBRARY}
    ${GLIBC_DL_LIBRARY}
    ${LIBEV_LIBRARIES}
    ${REDOX_LIBRARIES}
    ${XROOTD_CL_LIBRARY}
    ${XROOTD_POSIX_LIBRARY}
    ${XROOTD_UTILS_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${JSONCPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${PROTOBUF_LIBRARIES})

  set_target_properties(
    XrdEosMgm-Static
    PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

  add_executable(
    EosMgmConfigTest
    tests/ConfigEngineTest.cc)

  target_link_libraries(
    EosMgmConfigTest
    XrdEosMgm-Static
    ${CPPUNIT_LIBRARY})

endif()

install(
  TARGETS XrdEosMgm
  LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})

install(
  PROGRAMS eos-repair-tool
  DESTINATION ${CMAKE_INSTALL_FULL_SBINDIR}
  PERMISSIONS OWNER_READ OWNER_EXECUTE
	      GROUP_READ GROUP_EXECUTE
	      WORLD_READ WORLD_EXECUTE)
