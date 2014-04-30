// ----------------------------------------------------------------------
// File: XrdMgmOfs.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

/*----------------------------------------------------------------------------*/
#include "common/Mapping.hh"
#include "common/FileId.hh"
#include "common/LayoutId.hh"
#include "common/Path.hh"
#include "common/Timing.hh"
#include "common/StringConversion.hh"
#include "common/SecEntity.hh"
#include "common/StackTrace.hh"
#include "namespace/Constants.hh"
#include "mgm/Access.hh"
#include "mgm/FileSystem.hh"
#include "mgm/XrdMgmOfs.hh"
#include "mgm/XrdMgmOfsFile.hh"
#include "mgm/XrdMgmOfsTrace.hh"
#include "mgm/XrdMgmOfsSecurity.hh"
#include "mgm/Policy.hh"
#include "mgm/Quota.hh"
#include "mgm/Acl.hh"
#include "mgm/txengine/TransferEngine.hh"
#include "mgm/Recycle.hh"
#include "mgm/Macros.hh"
#include "namespace/IView.hh"
#include "namespace/IFileMDSvc.hh"
#include "namespace/IContainerMDSvc.hh"
#include "namespace/views/HierarchicalView.hh"
#include "namespace/accounting/FileSystemView.hh"
#include "namespace/persistency/ChangeLogContainerMDSvc.hh"
#include "namespace/persistency/ChangeLogFileMDSvc.hh"
/*----------------------------------------------------------------------------*/
#include "XrdVersion.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsAio.hh"
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/

#ifdef __APPLE__
#define ECOMM 70
#endif

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif


/*----------------------------------------------------------------------------*/

/******************************************************************************/
/******************************************************************************/
/* MGM File Interface                                                         */
/******************************************************************************/
/******************************************************************************/

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::open (const char *inpath,
                     XrdSfsFileOpenMode open_mode,
                     mode_t Mode,
                     const XrdSecEntity *client,
                     const char *ininfo)
/*----------------------------------------------------------------------------*/
/*
 * @brief open a given file with the indicated mode
 * 
 * @param inpath path to open
 * @param open_mode SFS_O_RDONLY,SFS_O_WRONLY,SFS_O_RDWR,SFS_O_CREAT,SFS_TRUNC
 * @param Mode posix access mode bits to be assigned
 * @param client XRootD authentication object
 * @param ininfo CGI
 * @return SFS_OK on succes, otherwise SFS_ERROR on error or redirection
 * 
 * Mode may also contain SFS_O_MKPATH if one desires to automatically create
 * all missing directories for a file (if possible).
 *
 */
/*----------------------------------------------------------------------------*/
{
  static const char *epname = "open";
  const char *tident = error.getErrUser();
  errno = 0;

  EXEC_TIMING_BEGIN("Open");
  SetLogId(logId, tident);

  eos::common::Mapping::IdMap(client, ininfo, tident, vid);
  gOFS->MgmStats.Add("IdMap", vid.uid, vid.gid, 1);

  SetLogId(logId, vid, tident);

  NAMESPACEMAP;
  BOUNCE_ILLEGAL_NAMES;
  BOUNCE_NOT_ALLOWED;

  int open_flag = 0;
  int isRW = 0;
  int isRewrite = 0;
  bool isCreation = false;
  
  // flag indicating parallel IO access
  bool isPio = false;

  // flag indicating access with reconstruction
  bool isPioReconstruct = false;

  // flag indicating FUSE file access
  bool isFuse = false;

  // flag indiciating an atomic upload where a file get's a hidden unique name and is renamed when it is closed
  bool isAtomicUpload = false;

  // list of filesystem IDs to reconstruct
  std::vector<unsigned int> PioReconstructFsList;

  // list of filesystem IDs usable for replacmenet
  std::vector<unsigned int> PioReplacementFsList;

  // of RAIN files

  int crOpts = (Mode & SFS_O_MKPTH) ? XRDOSS_mkpath : 0;

  // Set the actual open mode and find mode
  //
  if (open_mode & SFS_O_CREAT) open_mode = SFS_O_CREAT;
  else if (open_mode & SFS_O_TRUNC) open_mode = SFS_O_TRUNC;



  switch (open_mode & (SFS_O_RDONLY | SFS_O_WRONLY | SFS_O_RDWR |
                       SFS_O_CREAT | SFS_O_TRUNC))
  {
  case SFS_O_CREAT: open_flag = O_RDWR | O_CREAT | O_EXCL;
    crOpts |= XRDOSS_new;
    isRW = 1;
    break;
  case SFS_O_TRUNC: open_flag |= O_RDWR | O_CREAT | O_TRUNC;
    isRW = 1;
    break;
  case SFS_O_RDONLY: open_flag = O_RDONLY;
    isRW = 0;
    break;
  case SFS_O_WRONLY: open_flag = O_WRONLY;
    isRW = 1;
    break;
  case SFS_O_RDWR: open_flag = O_RDWR;
    isRW = 1;
    break;
  default: open_flag = O_RDONLY;
    isRW = 0;
    break;
  }

  XrdOucString pinfo = info ? info : "";
  eos::common::StringConversion::MaskTag(pinfo, "cap.msg");
  eos::common::StringConversion::MaskTag(pinfo, "cap.sym");
  eos::common::StringConversion::MaskTag(pinfo, "authz");

  if (isRW)
  {
    eos_info("op=write trunc=%d path=%s info=%s",
             open_mode & SFS_O_TRUNC, path, pinfo.c_str());
  }
  else
  {
    eos_info("op=read path=%s info=%s", path, pinfo.c_str());
  }

  ACCESSMODE_R;
  if (isRW)
  {
    SET_ACCESSMODE_W;
  }

  if (ProcInterface::IsWriteAccess(path, pinfo.c_str()))
  {
    SET_ACCESSMODE_W;
  }

  MAYSTALL;
  MAYREDIRECT;

  openOpaque = new XrdOucEnv(info);

  {
    // figure out if this is FUSE access
    const char* val = 0;
    if ( (val = openOpaque->Get("eos.app")) ) {
      XrdOucString application = val;
      if (application == "fuse") {
	isFuse=true;
      }
    }
  }
  // ---------------------------------------------------------------------------
  // PIO MODE CONFIGURATION
  // ---------------------------------------------------------------------------
  // PIO mode return's a vector of URLs to a client and the client contact's
  // directly these machines and run's the RAIN codec on client side.
  // The default mode return's one gateway machine and this machine run's the
  // RAIN codec.
  // On the fly reconstruction is done using PIO mode when the reconstruction
  // action is defined ('eos.pio.action=reconstruct'). The client can specify
  // a list of filesystem's which should be excluded. In case they are used
  // in the layout the stripes on the explicitly referenced filesystems and
  // all other unavailable filesystems get reconstructed into stripes on 
  // new machines.
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // discover PIO mode
  // ---------------------------------------------------------------------------
  XrdOucString sPio = (openOpaque) ? openOpaque->Get("eos.cli.access") : "";
  if (sPio == "pio")
  {
    isPio = true;
  }

  // ---------------------------------------------------------------------------
  // discover PIO reconstruction mode
  // ---------------------------------------------------------------------------
  XrdOucString sPioRecover = (openOpaque) ?
    openOpaque->Get("eos.pio.action") : "";
  if (sPioRecover == "reconstruct")
  {
    isPioReconstruct = true;
  }

  {
    // -------------------------------------------------------------------------
    // discover PIO reconstruction filesystems (stripes to be replaced)
    // -------------------------------------------------------------------------
    std::string sPioRecoverFs = (openOpaque) ?
      (openOpaque->Get("eos.pio.recfs") ? openOpaque->Get("eos.pio.recfs") : "")
      : "";
    std::vector<std::string> fsToken;
    eos::common::StringConversion::Tokenize(sPioRecoverFs, fsToken, ",");

    if (openOpaque->Get("eos.pio.recfs") && !fsToken.size())
    {
      // -----------------------------------------------------------------------
      // if there is a list announced there should be atleast one filesystem
      // mentioned for reconstruction
      // -----------------------------------------------------------------------
      return Emsg(epname, error, EINVAL, "open - you specified a list of"
                  " reconstruction filesystems but the list is empty", path);
    }

    for (size_t i = 0; i < fsToken.size(); i++)
    {
      errno = 0;
      unsigned int rfs = (unsigned int) strtol(fsToken[i].c_str(), 0, 10);
      XrdOucString srfs = "";
      srfs += (int) rfs;
      if (errno || (srfs != fsToken[i].c_str()))
      {
        return Emsg(epname,
                    error,
                    EINVAL,
                    "open - you specified a list of "
                    "reconstruction filesystems but "
                    "the list contains non numerical or illegal id's",
                    path);
      }
      // store in the reconstruction filesystem list
      PioReconstructFsList.push_back(rfs);
    }
  }

  int rcode = SFS_ERROR;

  XrdOucString redirectionhost = "invalid?";

  XrdOucString targethost = "";
  int targetport = atoi(gOFS->MgmOfsTargetPort.c_str());

  int ecode = 0;
  unsigned long fmdlid = 0;
  unsigned long long cid = 0;

  eos_debug("mode=%x create=%x truncate=%x", open_mode, SFS_O_CREAT, SFS_O_TRUNC);

  // proc filter
  if (ProcInterface::IsProcAccess(path))
  {
    if (gOFS->Authorization &&
        (vid.prot != "sss") &&
        (vid.host != "localhost") &&
        (vid.host != "localhost.localdomain"))
    {
      return Emsg(epname, error, EPERM, "execute proc command - you don't have"
                  " the requested permissions for that operation (1)", path);
    }

    gOFS->MgmStats.Add("OpenProc", vid.uid, vid.gid, 1);

    if (!ProcInterface::Authorize(path, info, vid, client))
    {
      return Emsg(epname, error, EPERM, "execute proc command - you don't have "
                  "the requested permissions for that operation (2)", path);
    }
    else
    {
      procCmd = new ProcCommand();
      procCmd->SetLogId(logId, vid, tident);
      return procCmd->open(path, info, vid, &error);
    }
  }

  gOFS->MgmStats.Add("Open", vid.uid, vid.gid, 1);

  eos_debug("authorize start");

  if (open_flag & O_CREAT)
  {
    AUTHORIZE(client, openOpaque, AOP_Create, "create", inpath, error);
  }
  else
  {
    AUTHORIZE(client, openOpaque, (isRW ? AOP_Update : AOP_Read), "open",
              inpath, error);
    isRewrite = true;
  }

  eos_debug("msg=\"authorize done\"");

  eos::common::Path cPath(path);

  // prevent any access to a recycling bin for writes
  if (isRW && cPath.GetFullPath().beginswith(Recycle::gRecyclingPrefix.c_str()))
  {
    return Emsg(epname, error, EPERM,
                "open file - nobody can write to a recycling bin",
                cPath.GetParentPath());
  }

  // check if we have to create the full path
  if (Mode & SFS_O_MKPTH)
  {
    eos_debug("msg=\"SFS_O_MKPTH was requested\"");

    XrdSfsFileExistence file_exists;
    int ec = gOFS->_exists(cPath.GetParentPath(), file_exists, error, vid, 0);

    // check if that is a file
    if ((!ec) && (file_exists != XrdSfsFileExistNo) &&
        (file_exists != XrdSfsFileExistIsDirectory))
    {
      return Emsg(epname, error, ENOTDIR,
                  "open file - parent path is not a directory",
                  cPath.GetParentPath());
    }
    // if it does not exist try to create the path!
    if ((!ec) && (file_exists == XrdSfsFileExistNo))
    {
      ec = gOFS->_mkdir(cPath.GetParentPath(), Mode, error, vid, info);
      if (ec)
      {
        gOFS->MgmStats.Add("OpenFailedPermission", vid.uid, vid.gid, 1);
        return SFS_ERROR;
      }
    }
  }

  bool isSharedFile = gOFS->VerifySharePath(path,openOpaque);

  // get the directory meta data if exists
  eos::ContainerMD* dmd = 0;
  eos::ContainerMD::XAttrMap attrmap;
  Acl acl;
  bool stdpermcheck = false;

  int versioning=0;

  uid_t d_uid = vid.uid;
  gid_t d_gid = vid.gid;


  {
    eos::common::RWMutexReadLock lock(gOFS->eosViewRWMutex);
    // -------------------------------------------------------------------------
    try
    {
      dmd = gOFS->eosView->getContainer(cPath.GetParentPath());
      // get the attributes out
      eos::ContainerMD::XAttrMap::const_iterator it;
      for (it = dmd->attributesBegin(); it != dmd->attributesEnd(); ++it)
      {
        attrmap[it->first] = it->second;
      }
      if (dmd)
      {
        fmd = dmd->findFile(cPath.GetName());
        if (!fmd)
        {
          if (dmd->findContainer(cPath.GetName()))
          {
            errno = EISDIR;
          }
          else
          {
            errno = ENOENT;
          }
        }
        else
        {
          fileId = fmd->getId();
          fmdlid = fmd->getLayoutId();
          cid = fmd->getContainerId();
        }
        d_uid = dmd->getCUid();
        d_gid = dmd->getCGid();
      }
      else
        fmd = 0;

    }
    catch (eos::MDException &e)
    {
      dmd = 0;
      errno = e.getErrno();
      eos_debug("msg=\"exception\" ec=%d emsg=\"%s\"\n",
                e.getErrno(), e.getMessage().str().c_str());
    };

    // -------------------------------------------------------------------------
    // check permissions
    // -------------------------------------------------------------------------
    if (!dmd)
    {
      MAYREDIRECT_ENOENT;

      if (cPath.GetSubPath(2))
      {
        eos_info("info=\"checking l2 path\" path=%s", cPath.GetSubPath(2));
        // ---------------------------------------------------------------------
        // check if we have a redirection setting at level 2 in the namespace
        // ---------------------------------------------------------------------
        try
        {
          dmd = gOFS->eosView->getContainer(cPath.GetSubPath(2));
          // get the attributes out
          eos::ContainerMD::XAttrMap::const_iterator it;
          for (it = dmd->attributesBegin(); it != dmd->attributesEnd(); ++it)
          {
            attrmap[it->first] = it->second;
          }
        }
        catch (eos::MDException &e)
        {
          dmd = 0;
          errno = e.getErrno();
          eos_debug("msg=\"exception\" ec=%d emsg=%s\n",
                    e.getErrno(), e.getMessage().str().c_str());
        };
        // ---------------------------------------------------------------------
        if (attrmap.count("sys.redirect.enoent"))
        {
          // there is a redirection setting here
          redirectionhost = "";
          redirectionhost = attrmap["sys.redirect.enoent"].c_str();
          int portpos = 0;
          if ((portpos = redirectionhost.find(":")) != STR_NPOS)
          {
            XrdOucString port = redirectionhost;
            port.erase(0, portpos + 1);
            ecode = atoi(port.c_str());
            redirectionhost.erase(portpos);
          }
          else
          {
            ecode = 1094;
          }
          rcode = SFS_REDIRECT;
          error.setErrInfo(ecode, redirectionhost.c_str());
          gOFS->MgmStats.Add("RedirectENOENT", vid.uid, vid.gid, 1);

          XrdOucString predirectionhost = redirectionhost.c_str();
          eos::common::StringConversion::MaskTag(predirectionhost, "cap.msg");
          eos::common::StringConversion::MaskTag(predirectionhost, "cap.sym");
          eos::common::StringConversion::MaskTag(pinfo, "authz");

          eos_info("info=\"redirecting\" hostport=%s:%d", predirectionhost.c_str(), ecode);
          return rcode;
        }
      }
      gOFS->MgmStats.Add("OpenFailedENOENT", vid.uid, vid.gid, 1);
      return Emsg(epname, error, errno, "open file", path);
    }

    // -------------------------------------------------------------------------
    // Check for sys.ownerauth entries, which let people operate as the owner of 
    // the directory
    // -------------------------------------------------------------------------
    if (attrmap.count("sys.owner.auth"))
    {
      attrmap["sys.owner.auth"] += ",";
      std::string ownerkey = vid.prot.c_str();
      ownerkey += ":";
      if (vid.prot == "gsi")
      {
        ownerkey += vid.dn.c_str();
      }
      else
      {
        ownerkey += vid.name.c_str();
      }
      if ((attrmap["sys.owner.auth"].find(ownerkey)) != std::string::npos)
      {
        eos_info("msg=\"client authenticated as directory owner\" path=\"%s\"uid=\"%u=>%u\" gid=\"%u=>%u\"", path, vid.uid, vid.gid, d_uid, d_gid);
        // yes the client can operate as the owner, we rewrite the virtual 
        // identity to the directory uid/gid pair
        vid.uid = d_uid;
        vid.gid = d_gid;
      }
    }

    // -------------------------------------------------------------------------
    // ACL and permission check
    // -------------------------------------------------------------------------
    acl.Set(attrmap.count("sys.acl") ? attrmap["sys.acl"] : std::string(""),
            attrmap.count("user.acl") ? attrmap["user.acl"] : std::string(""),
            vid,
	    attrmap.count("sys.eval.useracl"));
    eos_info("acl=%d r=%d w=%d wo=%d egroup=%d shared=%d",
             acl.HasAcl(), acl.CanRead(), acl.CanWrite(), acl.CanWriteOnce(),
             acl.HasEgroup(), 
	     isSharedFile);
    if (acl.HasAcl())
    {
      if (isRW)
      {
        // write case
        if ((!acl.CanWrite()) && (!acl.CanWriteOnce()))
        {
          // we have to check the standard permissions
          stdpermcheck = true;
        }
      }
      else
      {
        // read case
        if ((!acl.CanRead()))
        {
          // we have to check the standard permissions
          stdpermcheck = true;
        }
      }
    }
    else
    {
      stdpermcheck = true;
    }

    if ( ( (!isSharedFile) || (isSharedFile && isRW) ) &&  stdpermcheck 
	 && (!dmd->access(vid.uid,
			  vid.gid,
			  (isRW) ? W_OK | X_OK : R_OK | X_OK)))
    {
      errno = EPERM;
      gOFS->MgmStats.Add("OpenFailedPermission", vid.uid, vid.gid, 1);
      return Emsg(epname, error, errno, "open file", path);
    }

    // -------------------------------------------------------------------------
    // store the in-memory modification time
    // we get the current time, but we don't update the creation time
    // -------------------------------------------------------------------------
    gOFS->UpdateNowInmemoryDirectoryModificationTime(dmd->getId());
    // -------------------------------------------------------------------------
  }


  // set the versioning depth if it is defined

  if (attrmap.count("sys.versioning"))
  {
    versioning = atoi(attrmap["sys.versioning"].c_str());
  }
  else
  {
    if (attrmap.count("user.versioning"))
    {
      versioning = atoi(attrmap["user.versioning"].c_str());
    }
  }

  if ( attrmap.count("sys.forced.atomic") )
  {
    isAtomicUpload = atoi(attrmap["sys.forced.atomic"].c_str());
  }
  else
  {
    if ( attrmap.count("user.forced.atomic") )
    {
      isAtomicUpload = atoi(attrmap["user.forced.atomic"].c_str());
    } else {
      if (openOpaque->Get("eos.atomic"))
      {
	isAtomicUpload = true;
      }
    }
  }
    
  if (isRW)
  {
    if (isRewrite &&
        (
         (eos::common::LayoutId::GetLayoutType(fmdlid) ==
          eos::common::LayoutId::kRaidDP) ||

         (eos::common::LayoutId::GetLayoutType(fmdlid) ==
          eos::common::LayoutId::kArchive) ||

         (eos::common::LayoutId::GetLayoutType(fmdlid) ==
          eos::common::LayoutId::kRaid6)
         )
        &&
        (vid.uid > 3)
        )
    {
      gOFS->MgmStats.Add("OpenFailedNoUpdate", vid.uid, vid.gid, 1);
      // -----------------------------------------------------------------------
      // unpriviledged users are not allowed to open RAIN files for update
      // -----------------------------------------------------------------------
      return Emsg(epname, error, EPERM,
                  "update RAIN layout file - "
                  "you have to be a priviledged user for updates");
    }

    if ((open_mode & SFS_O_TRUNC) && fmd)
    {
      // check if this directory is write-once for the mapped user
      if (acl.HasAcl())
      {
        if (acl.CanWriteOnce())
        {
          gOFS->MgmStats.Add("OpenFailedNoUpdate", vid.uid, vid.gid, 1);
          // this is a write once user
          return Emsg(epname, error, EEXIST,
                      "overwrite existing file - you are write-once user");
        }
        else
        {
          if ((!stdpermcheck) && (!acl.CanWrite()))
          {
            return Emsg(epname, error, EPERM,
                        "overwrite existing file - you have no write permission");
          }
        }
      }

      if (versioning)
      {
        // handle the versioning for a specific file ID
        if (gOFS->Version(fileId, error, vid, versioning))
        {
          return Emsg(epname, error, errno, "version file", path);
        }
      }
      else
      {
        // drop the old file and create a new truncated one
        if (gOFS->_rem(path, error, vid, info))
        {
          return Emsg(epname, error, errno, "remove file for truncation", path);
        }
      }

      // invalidate the record
      fmd = 0;
      gOFS->MgmStats.Add("OpenWriteTruncate", vid.uid, vid.gid, 1);
    }
    else
    {
      if (!(fmd) && ((open_flag & O_CREAT)))
      {
        gOFS->MgmStats.Add("OpenWriteCreate", vid.uid, vid.gid, 1);
      }
      else
      {
        if (acl.HasAcl())
        {
          if (acl.CanWriteOnce())
          {
            // this is a write once user
            return Emsg(epname, error, EEXIST,
                        "overwrite existing file - you are write-once user");
          }
          else
          {
            if ((!stdpermcheck) && (!acl.CanWrite()))
            {
              return Emsg(epname, error, EPERM,
                          "overwrite existing file - you have no write permission");
            }
          }
        }

        gOFS->MgmStats.Add("OpenWrite", vid.uid, vid.gid, 1);
      }
    }


    // -------------------------------------------------------------------------
    // write case
    // -------------------------------------------------------------------------
    if ((!fmd))
    {
      if (!(open_flag & O_CREAT))
      {
        // write open of not existing file without creation flag
        return Emsg(epname, error, errno, "open file without creation flag", path);
      }
      else
      {
        // creation of a new file
        {
          // -------------------------------------------------------------------
          eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
          try
          {
	    std::string creation_path=path;
            // we create files with the uid/gid of the parent directory
	    if (isAtomicUpload) 
	    {
	      eos::common::Path cPath(path);
	      creation_path = cPath.GetAtomicPath();
	    }

	    fmd = gOFS->eosView->createFile(creation_path.c_str(), vid.uid, vid.gid);
            fileId = fmd->getId();
            fmdlid = fmd->getLayoutId();
            cid = fmd->getContainerId();
          }
          catch (eos::MDException &e)
          {
            fmd = 0;
            errno = e.getErrno();
            eos_debug("msg=\"exception\" ec=%d emsg=\"%s\"\n",
                      e.getErrno(), e.getMessage().str().c_str());
          };
          // -------------------------------------------------------------------
        }

        if (!fmd)
        {
          // creation failed
          gOFS->MgmStats.Add("OpenFailedCreate", vid.uid, vid.gid, 1);
          return Emsg(epname, error, errno, "create file", path);
        }
        isCreation = true;
      }
    }
    else
    {
      // we attached to an existing file
      if (open_flag & O_EXCL)
      {
        gOFS->MgmStats.Add("OpenFailedExists", vid.uid, vid.gid, 1);
        return Emsg(epname, error, EEXIST, "create file", path);
      }

      if (acl.HasAcl())
      {
        if (!acl.CanUpdate())
        {
          // the ACL has !u set - we don't allow to do file updates
          gOFS->MgmStats.Add("OpenFailedNoUpdate", vid.uid, vid.gid, 1);
          return Emsg(epname, error, EPERM, "update file - fobidden by ACL",
                      path);
        }
      }
    }
  }
  else
  {
    if (!fmd)
    {
      // check if there is a redirect or stall for missing entries
      MAYREDIRECT_ENOENT;
      MAYSTALL_ENOENT;
    }

    if ((!fmd) && (attrmap.count("sys.redirect.enoent")))
    {
      // there is a redirection setting here
      redirectionhost = "";
      redirectionhost = attrmap["sys.redirect.enoent"].c_str();
      int portpos = 0;
      if ((portpos = redirectionhost.find(":")) != STR_NPOS)
      {
        XrdOucString port = redirectionhost;
        port.erase(0, portpos + 1);
        ecode = atoi(port.c_str());
        redirectionhost.erase(portpos);
      }
      else
      {
        ecode = 1094;
      }
      rcode = SFS_REDIRECT;
      error.setErrInfo(ecode, redirectionhost.c_str());
      gOFS->MgmStats.Add("RedirectENOENT", vid.uid, vid.gid, 1);
      return rcode;
    }
    if ((!fmd))
    {
      gOFS->MgmStats.Add("OpenFailedENOENT", vid.uid, vid.gid, 1);
      return Emsg(epname, error, errno, "open file", path);
    }
    if (isSharedFile)
      gOFS->MgmStats.Add("OpenShared", vid.uid, vid.gid, 1);
    else
      gOFS->MgmStats.Add("OpenRead", vid.uid, vid.gid, 1);

  }

  // ---------------------------------------------------------------------------
  // construct capability
  // ---------------------------------------------------------------------------
  XrdOucString capability = "";

  if (isPioReconstruct) 
  {
    capability += "&mgm.access=update";
  } 
  else 
  {
    if (isRW)
    {
      if (isRewrite)
      {
	capability += "&mgm.access=update";
      }
      else
      {
	capability += "&mgm.access=create";
      }
    }
    else
    {
      capability += "&mgm.access=read";
    }
  }

  // ---------------------------------------------------------------------------
  // forward some allowed user opaque tags
  // ---------------------------------------------------------------------------
  unsigned long layoutId = (isCreation) ? eos::common::LayoutId::kPlain : fmdlid;
  // the client can force to read a file on a defined file system
  unsigned long forcedFsId = 0;

  // the client can force to place a file in a specified group of a space
  long forcedGroup = -1;

  // this is the filesystem defining the client access point in the selection 
  // vector - for writes it is always 0, for reads it comes out of the 
  // FileAccess function
  unsigned long fsIndex = 0;
  XrdOucString space = "default";

  unsigned long newlayoutId = 0;
  // select space and layout according to policies
  Policy::GetLayoutAndSpace(path,
                            attrmap,
                            vid,
                            newlayoutId,
                            space,
                            *openOpaque,
                            forcedFsId,
                            forcedGroup);


  eos::common::RWMutexReadLock vlock(FsView::gFsView.ViewMutex); // lock order 1
  eos::common::RWMutexReadLock lock(Quota::gQuotaMutex); // lock order 2

  SpaceQuota* quotaspace = Quota::GetSpaceQuota(space.c_str(), false);

  if (!quotaspace)
  {
    gOFS->MgmStats.Add("OpenFailedQuota", vid.uid, vid.gid, 1);
    return Emsg(epname, error, EINVAL, "get quota space ", space.c_str());
  }

  unsigned long long external_mtime = 0;
  unsigned long long external_ctime = 0;

  if (openOpaque->Get("eos.ctime"))
  {
    external_ctime = strtoull(openOpaque->Get("eos.ctime"), 0, 10);
  }

  if (openOpaque->Get("eos.mtime"))
  {
    external_mtime = strtoull(openOpaque->Get("eos.mtime"), 0, 10);
  }

  if (isCreation || ((open_mode == SFS_O_TRUNC) && (!fmd->getNumLocation())))
  {
    eos_info("blocksize=%llu lid=%x",
             eos::common::LayoutId::GetBlocksize(newlayoutId), newlayoutId);
    layoutId = newlayoutId;

    {
      eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
      eos::FileMD* fmdnew = 0;
      try
      {
        fmdnew = gOFS->eosView->getFile(path);
      }
      catch (eos::MDException &e)
      {
        if ((!isAtomicUpload) && (fmdnew != fmd))
        {
          // file has been recreated in the meanwhile
          return Emsg(epname, error, EEXIST, "open file (file recreated)", path);
        }
      }
      // -----------------------------------------------------------------------   
      // set the layout and commit new meta data 
      fmd->setLayoutId(layoutId);
      // -------------------------------------------------------------------------
      // if specified set an external modification/creation time 
      // -------------------------------------------------------------------------
      if (external_mtime)
      {
        eos::FileMD::ctime_t mtime;
        mtime.tv_sec = external_mtime;
        mtime.tv_nsec = 0;
        fmd->setMTime(mtime);
      }
      if (external_ctime)
      {
        eos::FileMD::ctime_t ctime;
        ctime.tv_sec = external_ctime;
        ctime.tv_nsec = 0;
        fmd->setCTime(ctime);
      }
      try
      {
        gOFS->eosView->updateFileStore(fmd);

        SpaceQuota* space = Quota::GetResponsibleSpaceQuota(path);
        if (space)
        {
          eos::QuotaNode* quotanode = 0;
          quotanode = space->GetQuotaNode();
          if (quotanode)
          {
            quotanode->addFile(fmd);
          }
        }
      }
      catch (eos::MDException &e)
      {
        errno = e.getErrno();
        std::string errmsg = e.getMessage().str();
        eos_debug("msg=\"exception\" ec=%d emsg=\"%s\"\n",
                  e.getErrno(), e.getMessage().str().c_str());
        gOFS->MgmStats.Add("OpenFailedQuota", vid.uid, vid.gid, 1);
        return Emsg(epname, error, errno, "open file", errmsg.c_str());
      }
      // -----------------------------------------------------------------------
    }
  }

  capability += "&mgm.ruid=";
  capability += (int) vid.uid;
  capability += "&mgm.rgid=";
  capability += (int) vid.gid;
  capability += "&mgm.uid=";
  capability += (int) vid.uid_list[0];
  capability += "&mgm.gid=";
  capability += (int) vid.gid_list[0];
  capability += "&mgm.path=";
  capability += path;
  capability += "&mgm.manager=";
  capability += gOFS->ManagerId.c_str();
  capability += "&mgm.fid=";
  XrdOucString hexfid;
  eos::common::FileId::Fid2Hex(fileId, hexfid);
  capability += hexfid;

  XrdOucString sizestring;
  capability += "&mgm.cid=";
  capability += eos::common::StringConversion::GetSizeString(sizestring, cid);

  // add the mgm.sec information to the capability
  capability += "&mgm.sec=";
  capability += eos::common::SecEntity::ToKey(client,
                                              openOpaque->Get("eos.app")).c_str();

  if (attrmap.count("user.tag"))
  {
    capability += "&mgm.container=";
    capability += attrmap["user.tag"].c_str();
  }

  // the size which will be reserved with a placement of one replica 
  // for that file
  unsigned long long bookingsize;
  bool hasClientBookingSize = false;
  unsigned long long targetsize = 0;
  unsigned long long minimumsize = 0;
  unsigned long long maximumsize = 0;

  if (attrmap.count("sys.forced.bookingsize"))
  {
    // we allow only a system attribute not to get fooled by a user
    bookingsize = strtoull(attrmap["sys.forced.bookingsize"].c_str(), 0, 10);
  }
  else
  {
    if (attrmap.count("user.forced.bookingsize"))
    {
      bookingsize = strtoull(attrmap["user.forced.bookingsize"].c_str(), 0, 10);
    }
    else
    {
      bookingsize = 1024ll; // 1k as default
      if (openOpaque->Get("eos.bookingsize"))
      {
        bookingsize = strtoull(openOpaque->Get("eos.bookingsize"), 0, 10);
        hasClientBookingSize = true;
      }
      else
      {
        if (openOpaque->Get("oss.asize"))
        {
          bookingsize = strtoull(openOpaque->Get("oss.asize"), 0, 10);
          hasClientBookingSize = true;
        }
      }
    }
  }

  if (attrmap.count("sys.forced.minsize"))
  {
    minimumsize = strtoull(attrmap["sys.forced.minsize"].c_str(), 0, 10);
  }

  if (attrmap.count("sys.forced.maxsize"))
  {
    maximumsize = strtoull(attrmap["sys.forced.maxsize"].c_str(), 0, 10);
  }

  if (openOpaque->Get("oss.asize"))
  {
    targetsize = strtoull(openOpaque->Get("oss.asize"), 0, 10);
  }

  if (openOpaque->Get("eos.targetsize"))
  {
    targetsize = strtoull(openOpaque->Get("eos.targetsize"), 0, 10);
  }

  eos::mgm::FileSystem* filesystem = 0;

  std::vector<unsigned int> selectedfs;
  // file systems which are unavailable during a read operation
  std::vector<unsigned int> unavailfs;
  // file systems which have been replaced with a new reconstructed stripe
  std::vector<unsigned int> replacedfs;

  std::vector<unsigned int>::const_iterator sfs;

  int retc = 0;

  // ---------------------------------------------------------------------------
  if (isCreation || ((open_mode == SFS_O_TRUNC) && (!fmd->getNumLocation())))
  {
    // -------------------------------------------------------------------------
    // place a new file 
    // -------------------------------------------------------------------------
    const char* containertag = 0;
    if (attrmap.count("user.tag"))
    {
      containertag = attrmap["user.tag"].c_str();
    }
    retc = quotaspace->FilePlacement(path, vid, containertag, layoutId,
                                     selectedfs, selectedfs,
                                     open_mode & SFS_O_TRUNC,
                                     forcedGroup,
                                     bookingsize);
  }
  else
  {
    // -------------------------------------------------------------------------
    // access existing file
    // -------------------------------------------------------------------------

    // fill the vector with the existing locations
    for (unsigned int i = 0; i < fmd->getNumLocation(); i++)
    {
      int loc = fmd->getLocation(i);
      if (loc)
        selectedfs.push_back(loc);
    }

    if (!selectedfs.size())
    {
      // this file has not a single existing replica
      gOFS->MgmStats.Add("OpenFileOffline", vid.uid, vid.gid, 1);
      return Emsg(epname, error, ENODEV, "open - no replica exists", path);
    }

    // reconstruction opens files in RW mode but we actually need RO mode in this case
    retc = quotaspace->FileAccess(vid, forcedFsId, space.c_str(), layoutId,
                                  selectedfs, fsIndex, isPioReconstruct?false:isRW, fmd->getSize(),
                                  unavailfs);

    if (retc == EXDEV)
    {
      // -----------------------------------------------------------------------
      // indicating that the layout requires the replacement of stripes
      // -----------------------------------------------------------------------
      retc = 0; // TODO: we currently don't support repair on the fly mode
    }
  }

  if (retc)
  {
    // if we don't have quota we don't bounce the client back
    if ((retc != ENOSPC) && (retc != EDQUOT))
    {

      // check if we have a global redirect or stall for offline files
      MAYREDIRECT_ENONET;
      MAYSTALL_ENONET;

      // check if we should try to heal offline replicas (rw mode only)
      if ((!isCreation) && isRW && attrmap.count("sys.heal.unavailable"))
      {
        int nmaxheal = atoi(attrmap["sys.heal.unavailable"].c_str());
        int nheal = 0;
        gOFS->MgmHealMapMutex.Lock();
        if (gOFS->MgmHealMap.count(fileId))
          nheal = gOFS->MgmHealMap[fileId];

        // if there was already a healing
        if (nheal >= nmaxheal)
        {
          // we tried nmaxheal times to heal, so we abort now and 
          // return an error to the client
          gOFS->MgmHealMap.erase(fileId);
          gOFS->MgmHealMap.resize(0);
          gOFS->MgmHealMapMutex.UnLock();
          gOFS->MgmStats.Add("OpenFailedHeal", vid.uid, vid.gid, 1);
          XrdOucString msg = "heal file with inaccesible replica's after ";
          msg += (int) nmaxheal;
          msg += " tries - giving up";
          eos_info("%s", msg.c_str());
          return Emsg(epname, error, ENOSR, msg.c_str(), path);
        }
        else
        {
          // increase the heal counter for that file id
          gOFS->MgmHealMap[fileId] = nheal + 1;
          ProcCommand* procCmd = new ProcCommand();
          if (procCmd)
          {
            // issue the adjustreplica command as root
            eos::common::Mapping::VirtualIdentity vidroot;
            eos::common::Mapping::Copy(vid, vidroot);
            eos::common::Mapping::Root(vidroot);
            XrdOucString cmd = "mgm.cmd=file&mgm.subcmd=adjustreplica&mgm.file.express=1&mgm.path=";
            cmd += path;
            procCmd->open("/proc/user/", cmd.c_str(), vidroot, &error);
            procCmd->close();
            delete procCmd;

            int stalltime = 60; // 1 min by default
            if (attrmap.count("sys.stall.unavailable"))
            {
              stalltime = atoi(attrmap["sys.stall.unavailable"].c_str());
            }
            gOFS->MgmStats.Add("OpenStalledHeal", vid.uid, vid.gid, 1);
            eos_info("attr=sys info=\"stalling file\" path=%s rw=%d stalltime=%d nstall=%d",
                     path, isRW, stalltime, nheal);
            gOFS->MgmHealMapMutex.UnLock();
            return gOFS->Stall(error, stalltime, ""
                               "Required filesystems are currently unavailable!");
          }
          else
          {
            gOFS->MgmHealMapMutex.UnLock();
            return Emsg(epname, error, ENOMEM,
                        "allocate memory for proc command", path);
          }
        }
      }

      // check if the dir attributes tell us to let clients rebounce
      if (attrmap.count("sys.stall.unavailable"))
      {
        int stalltime = atoi(attrmap["sys.stall.unavailable"].c_str());

        if (stalltime)
        {
          // stall the client
          gOFS->MgmStats.Add("OpenStalled", vid.uid, vid.gid, 1);
          eos_info("attr=sys info=\"stalling file since replica's are down\" path=%s rw=%d",
                   path, isRW);
          return gOFS->Stall(error, stalltime,
                             "Required filesystems are currently unavailable!");
        }
      }

      if (attrmap.count("user.stall.unavailable"))
      {
        int stalltime = atoi(attrmap["user.stall.unavailable"].c_str());
        if (stalltime)
        {
          // stall the client
          gOFS->MgmStats.Add("OpenStalled", vid.uid, vid.gid, 1);
          eos_info("attr=user info=\"stalling file since replica's are down\" path=%s rw=%d",
                   path, isRW);
          return gOFS->Stall(error, stalltime,
                             "Required filesystems are currently unavailable!");
        }
      }

      if ((attrmap.count("sys.redirect.enonet")))
      {
        // there is a redirection setting here if files are unaccessible
        redirectionhost = "";
        redirectionhost = attrmap["sys.redirect.enonet"].c_str();
        int portpos = 0;
        if ((portpos = redirectionhost.find(":")) != STR_NPOS)
        {
          XrdOucString port = redirectionhost;
          port.erase(0, portpos + 1);
          ecode = atoi(port.c_str());
          redirectionhost.erase(portpos);
        }
        else
        {
          ecode = 1094;
        }
        rcode = SFS_REDIRECT;
        error.setErrInfo(ecode, redirectionhost.c_str());
        gOFS->MgmStats.Add("RedirectENONET", vid.uid, vid.gid, 1);
        return rcode;
      }

      gOFS->MgmStats.Add("OpenFileOffline", vid.uid, vid.gid, 1);
    }
    else
    {
      if (isCreation)
      {
        // ---------------------------------------------------------------------
        // we will remove the created file in the namespace as root
        // since somebody could have a no-delete ACL
        // ---------------------------------------------------------------------
        eos::common::Mapping::VirtualIdentity vidroot;
        eos::common::Mapping::Root(vidroot);
        gOFS->_rem(cPath.GetPath(), error, vidroot, 0);
      }

      gOFS->MgmStats.Add("OpenFailedQuota", vid.uid, vid.gid, 1);
    }

    if (isRW)
    {
      if (retc == ENOSPC)
      {
        return Emsg(epname, error, retc, "get free physical space", path);
      }
      if (retc == EDQUOT)
      {
        return Emsg(epname, error, retc,
                    "get quota space - quota not defined or exhausted", path);
      }
      return Emsg(epname, error, retc, "access quota space", path);
    }
    else
    {
      return Emsg(epname, error, retc, "open file ", path);
    }
  }
  else
  {
    if (isRW)
    {
      if (isCreation && hasClientBookingSize && (bookingsize == 0))
      {
        // ---------------------------------------------------------------------
        // if this is a creation we commit the scheduled replicas NOW
        // ---------------------------------------------------------------------
        {
          eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
          // -------------------------------------------------------------------

          try
          {
            fmd = gOFS->eosView->getFile(path);

            for (int i = 0; i < (int) selectedfs.size(); i++)
            {
              fmd->addLocation(selectedfs[i]);
            }
            gOFS->eosView->updateFileStore(fmd);
          }
          catch (eos::MDException &e)
          {
            errno = e.getErrno();
            std::string errmsg = e.getMessage().str();
            eos_debug("msg=\"exception\" ec=%d emsg=\"%s\"\n",
                      e.getErrno(), e.getMessage().str().c_str());
            gOFS->MgmStats.Add("OpenFailedQuota", vid.uid, vid.gid, 1);
            return Emsg(epname, error, errno, "open file", errmsg.c_str());
          }
          // -------------------------------------------------------------------
        }
        isZeroSizeFile = true;
      }
    }
    else
    {
      if (!isFuse && !fmd->getSize())
      {
        // 0-size files can be read from the MGM if this is not FUSE access!
        isZeroSizeFile = true;
        return SFS_OK;
      }
    }
  }
  // ---------------------------------------------------------------------------
  // get the redirection host from the selected entry in the vector
  // ---------------------------------------------------------------------------
  if (!selectedfs[fsIndex])
  {
    eos_err("0 filesystem in selection");
    return Emsg(epname, error, ENONET, "received filesystem id 0", path);
  }

  if (FsView::gFsView.mIdView.count(selectedfs[fsIndex]))
    filesystem = FsView::gFsView.mIdView[selectedfs[fsIndex]];
  else
    return Emsg(epname, error, ENONET,
                "received non-existent filesystem", path);

  targethost = filesystem->GetString("host").c_str();
  targetport = atoi(filesystem->GetString("port").c_str());

  redirectionhost = targethost;
  redirectionhost += "?";

  // ---------------------------------------------------------------------------
  // Rebuild the layout ID (for read it should indicate only the number of 
  // available stripes for reading);
  // For 'pio' mode we hand out plain layouts to the client and add the IO 
  // layout as an extra field
  // ---------------------------------------------------------------------------
  newlayoutId =
    eos::common::LayoutId::GetId(
                                 isPio ? eos::common::LayoutId::kPlain :
                                 eos::common::LayoutId::GetLayoutType(layoutId),
                                 isPio ? eos::common::LayoutId::kNone : eos::common::LayoutId::GetChecksum(layoutId),
                                 isPioReconstruct?static_cast<int> (selectedfs.size()) + PioReconstructFsList.size() : static_cast<int> (selectedfs.size()),
                                 eos::common::LayoutId::GetBlocksizeType(layoutId),
                                 eos::common::LayoutId::GetBlockChecksum(layoutId));

  capability += "&mgm.lid=";
  capability += static_cast<int> (newlayoutId);
  // space to be prebooked/allocated
  capability += "&mgm.bookingsize=";
  capability += eos::common::StringConversion::GetSizeString(sizestring,
                                                             bookingsize);

  if (minimumsize)
  {
    capability += "&mgm.minsize=";
    capability += eos::common::StringConversion::GetSizeString(sizestring,
                                                               minimumsize);
  }

  if (maximumsize)
  {
    capability += "&mgm.maxsize=";
    capability += eos::common::StringConversion::GetSizeString(sizestring,
                                                               maximumsize);
  }

  // expected size of the target file on close
  if (targetsize)
  {
    capability += "&mgm.targetsize=";
    capability += eos::common::StringConversion::GetSizeString(sizestring,
                                                               targetsize);
  }

  if (eos::common::LayoutId::GetLayoutType(layoutId) ==
      eos::common::LayoutId::kPlain)
  {
    capability += "&mgm.fsid=";
    capability += (int) filesystem->GetId();
  }

  XrdOucString infolog = "";
  XrdOucString piolist = "";

  if ((eos::common::LayoutId::GetLayoutType(layoutId) == eos::common::LayoutId::kReplica) ||
      (eos::common::LayoutId::GetLayoutType(layoutId) == eos::common::LayoutId::kRaidDP) ||
      (eos::common::LayoutId::GetLayoutType(layoutId) == eos::common::LayoutId::kArchive) ||
      (eos::common::LayoutId::GetLayoutType(layoutId) == eos::common::LayoutId::kRaid6))
  {
    capability += "&mgm.fsid=";
    capability += (int) filesystem->GetId();

    eos::mgm::FileSystem* repfilesystem = 0;
    replacedfs.resize(selectedfs.size());

    // -------------------------------------------------------------------------
    // if replacement has been specified try to get new locations for reco.
    // -------------------------------------------------------------------------

    if (isPioReconstruct && PioReconstructFsList.size())
    {
      const char* containertag = 0;
      if (attrmap.count("user.tag"))
      {
        containertag = attrmap["user.tag"].c_str();
      }

      // -----------------------------------------------------------------------
      // create a plain layout with the number of replacement stripes to be
      // scheduled in the file placement routine
      // -----------------------------------------------------------------------
      unsigned long plainLayoutId = newlayoutId;
      eos::common::LayoutId::SetStripeNumber(
                                             plainLayoutId,
                                             PioReconstructFsList.size() - 1
                                             );

      // -----------------------------------------------------------------------
      // get the original placement group of the first fs to reconstruct
      {
        eos::common::FileSystem::fs_snapshot orig_snapshot;
        // get an original filesystem which is not in the reconstruction list
        unsigned int orig_fs=0;
        for (unsigned int i = 0; i < fmd->getNumLocation(); i++)
        {
          orig_fs = fmd->getLocation(i);
          bool isInReco=false;
          for (unsigned int j = 0; j < PioReconstructFsList.size(); j++)
          {
            if ( orig_fs == PioReconstructFsList[j] )
            {
              isInReco=true;
              break;
            }
          }
          if (!isInReco)
            break;
          orig_fs = 0;
        }

        if (!orig_fs)
        {
          // there is no original filesystem which is not in reconstruction
          return Emsg(epname, error, EINVAL, "get original filesystem for reconstruction", path);
        }

        if (!FsView::gFsView.mIdView.count(orig_fs))
        {
          // not existing original filesystem
          return Emsg(epname, error, EINVAL, "reconstruct filesystem", path);
        }

        // get an original filesystem which is not in the reconstruction list

        eos::mgm::FileSystem* origfs = FsView::gFsView.mIdView[orig_fs];
        origfs->SnapShotFileSystem(orig_snapshot);
        forcedGroup = orig_snapshot.mGroupIndex;
      }

      // -----------------------------------------------------------------------

      eos_info("nstripes=%d => nstripes=%d [ sub-group=%d ]",
               eos::common::LayoutId::GetStripeNumber(newlayoutId),
               eos::common::LayoutId::GetStripeNumber(plainLayoutId),
	       forcedGroup);
      // -----------------------------------------------------------------------
      // compute the size of the stripes to be placed
      // -----------------------------------------------------------------------
      unsigned long long plainBookingSize =
        fmd->getSize() /
        (eos::common::LayoutId::GetStripeNumber(layoutId) + 1);
      plainBookingSize += 4096;
      plainBookingSize *= PioReconstructFsList.size();

      eos::common::Mapping::VirtualIdentity rootvid;
      eos::common::Mapping::Root(rootvid);

      retc = quotaspace->FilePlacement(path, rootvid, containertag, plainLayoutId,
                                       selectedfs, PioReplacementFsList,
                                       false, forcedGroup,
                                       plainBookingSize);

      if (retc)
      {
        // the placement didn't work, we cannot schedule reconstruction
        gOFS->MgmStats.Add("OpenFailedReconstruct", rootvid.uid, rootvid.gid, 1);
        return Emsg(epname, error, retc, "schedule stripes for reconstruction", path);
      }

      for (int i = 0; i < (int) PioReplacementFsList.size(); i++)
      {
        eos_debug("msg=\"scheduled fs for reconstruction\" rec-fsid=%lu nrecofs=%lu", PioReplacementFsList[i], PioReplacementFsList.size());
      }

      // add fsid=0 filesystems to the selection vector if it has less than the nominal replica
      int selection_diff = (eos::common::LayoutId::GetStripeNumber(fmd->getLayoutId())+1) - selectedfs.size();
      eos_info("selection-diff=%d %d/%d", selection_diff, (eos::common::LayoutId::GetStripeNumber(fmd->getLayoutId())+1), selectedfs.size());
      if (selection_diff > 0)
      {
        unavailfs.push_back(0);
        for (int i = 0; i< selection_diff; i++)
        {
         selectedfs.push_back(0);
         eos_info("msg=\"adding fsid=0 as missing filesystem\"");
        }
      }
    }

    // put all the replica urls into the capability
    for (int i = 0; i < (int) selectedfs.size(); i++)
    {
      if (!selectedfs[i])
        eos_err("0 filesystem in replica vector");


      // -----------------------------------------------------------------------
      // Logic to discover filesystems to be reconstructed
      // -----------------------------------------------------------------------
      bool replace = false;
      if (isPioReconstruct)
      {
        for (size_t k = 0; k < PioReconstructFsList.size(); k++)
        {
          if (selectedfs[i] == PioReconstructFsList[k])
          {
            replace = true;
            break;
          }
        }
      }

      if (replace)
      {
        if (!PioReplacementFsList.size())
        {
          // if we don't have found any filesystem to be used as a replacement
          return Emsg(epname,
                      error,
                      EIO,
                      "get replacement file system",
                      path);
        }
        // ---------------------------------------------------------------------
        // take one replacement filesystem from the replacement list
        // ---------------------------------------------------------------------
        replacedfs[i] = selectedfs[i];
        selectedfs[i] = PioReplacementFsList.back();
        eos_info("msg=\"replace fs\" old-fsid=%u new-fsid=%u", replacedfs[i], selectedfs[i]);
        PioReplacementFsList.pop_back();
      }
      else
      {
        // there is no replacement happening
        replacedfs[i] = 0;
      }

      if (FsView::gFsView.mIdView.count(selectedfs[i]))
        repfilesystem = FsView::gFsView.mIdView[selectedfs[i]];
      else
        repfilesystem = 0;

      if (!repfilesystem)
      {
        // don't fail IO on a shadow file system but throw a ciritical error message
        eos_crit("msg=\"Unable to get replica filesystem information\" path=\"%s\" fsid=%d", path, selectedfs[i]);
        continue;
      } 
      else
      {
        if (replace)
        {
        // we have now a new target host which will do the reconstruction
        targethost = repfilesystem->GetString("host").c_str();
        targetport = atoi(repfilesystem->GetString("port").c_str());

        redirectionhost = targethost;
        redirectionhost += "?";
        // point into the right vector entry
        fsIndex=i;
        }
      }

      capability += "&mgm.url";
      capability += i;
      capability += "=root://";
      XrdOucString replicahost = "";
      int replicaport = 0;

      // -----------------------------------------------------------------------
      // Logic to mask 'offline' filesystems
      // -----------------------------------------------------------------------
      bool exclude = false;
      for (size_t k = 0; k < unavailfs.size(); k++)
      {
        if (selectedfs[i] == unavailfs[k])
        {
          exclude = true;
          break;
        }
      }

      if (exclude)
      {
        replicahost = "__offline_";
        replicahost += repfilesystem->GetString("host").c_str();
      }
      else
      {
        replicahost = repfilesystem->GetString("host").c_str();
      }

      replicaport = atoi(repfilesystem->GetString("port").c_str());

      capability += replicahost;
      capability += ":";
      capability += replicaport;
      capability += "//";
      // add replica fsid
      capability += "&mgm.fsid";
      capability += i;
      capability += "=";
      capability += (int) repfilesystem->GetId();
      if (isPio)
      {
        if (replacedfs[i])
        {
          // -------------------------------------------------------------------
          // add the drop message to the replacement capability
          // -------------------------------------------------------------------
          capability += "&mgm.drainfsid";
          capability += i;
          capability += "=";
          capability += (int) replacedfs[i];
        }

        piolist += "pio.";
        piolist += (int) i;
        piolist += "=";
        piolist += replicahost;
        piolist += ":";
        piolist += replicaport;
        piolist += "&";
      }
      eos_debug("Redirection Url %d => %s", i, replicahost.c_str());
      infolog += "target[";
      infolog += (int) i;
      infolog += "]=(";
      infolog += replicahost.c_str();
      infolog += ",";
      infolog += (int) repfilesystem->GetId();
      infolog += ") ";
    }
  }

  // ---------------------------------------------------------------------------
  // Encrypt capability
  // ---------------------------------------------------------------------------
  XrdOucEnv incapability(capability.c_str());
  XrdOucEnv* capabilityenv = 0;
  eos::common::SymKey* symkey = eos::common::gSymKeyStore.GetCurrentKey();

  eos_debug("capability=%s\n", capability.c_str());
  int caprc = 0;
  if ((caprc = gCapabilityEngine.Create(&incapability, capabilityenv, symkey)))
  {
    return Emsg(epname, error, caprc, "sign capability", path);
  }

  int caplen = 0;
  if (isPio)
  {
    redirectionhost = piolist;
    redirectionhost += "mgm.lid=";
    redirectionhost += static_cast<int> (layoutId);
    redirectionhost += "&mgm.logid=";
    redirectionhost += this->logId;
    redirectionhost += capabilityenv->Env(caplen);
  }
  else
  {
    redirectionhost += capabilityenv->Env(caplen);
    redirectionhost += "&mgm.logid=";
    redirectionhost += this->logId;
    if (openOpaque->Get("eos.blockchecksum"))
    {
      redirectionhost += "&mgm.blockchecksum=";
      redirectionhost += openOpaque->Get("eos.blockchecksum");
    }
    else
    {
      if ((!isRW) && (eos::common::LayoutId::GetLayoutType(layoutId) ==
                      eos::common::LayoutId::kReplica))
      {
        redirectionhost += "&mgm.blockchecksum=ignore";
      }
    }

    if (openOpaque->Get("eos.checksum"))
    {
      redirectionhost += "&mgm.checksum=";
      redirectionhost += openOpaque->Get("eos.checksum");
    }

    // For the moment we redirect only on storage nodes
    redirectionhost += "&mgm.replicaindex=";
    redirectionhost += (int) fsIndex;
    redirectionhost += "&mgm.replicahead=";
    redirectionhost += (int) fsIndex;
  }

  if (vid.prot == "https")
  {
    struct stat buf;
    std::string etag;
    eos::common::Mapping::VirtualIdentity rootvid;
    eos::common::Mapping::Root(rootvid);
    
    // get the current ETAG
    gOFS->_stat(path,&buf,error, rootvid, "", &etag);
    redirectionhost += "&mgm.etag=";
    redirectionhost += etag.c_str();
  }

  // add the MGM hex id for this file
  redirectionhost += "&mgm.id=";
  redirectionhost += hexfid;

  // Always redirect
  ecode = targetport;
  rcode = SFS_REDIRECT;
  error.setErrInfo(ecode, redirectionhost.c_str());

  if (redirectionhost.length() > (int) XrdOucEI::Max_Error_Len)
  {
    return Emsg(epname, error, ENOMEM,
                "open file - capability exceeds 2kb limit", path);
  }

  XrdOucString predirectionhost = redirectionhost.c_str();
  eos::common::StringConversion::MaskTag(predirectionhost, "cap.msg");
  eos::common::StringConversion::MaskTag(predirectionhost, "cap.sym");

  if (isRW)
  {
    eos_info("op=write path=%s info=%s %s redirection=%s:%d",
             path, pinfo.c_str(), infolog.c_str(), predirectionhost.c_str(),
             ecode);
  }
  else
  {
    eos_info("op=read  path=%s info=%s %s redirection=%s:%d",
             path, pinfo.c_str(), infolog.c_str(), predirectionhost.c_str(),
             ecode);
  }

  eos_info("info=\"redirection\" hostport=%s:%d", predirectionhost.c_str(),
           ecode);

  if (capabilityenv)
    delete capabilityenv;

  if (attrmap.count("sys.force.atime"))
  {
    // -------------------------------------------------------------------------
    // we are supposed to track the access time of a file. 
    // since we don't have an atime field we use the change time of the file
    // we only update the atime if the current atime is older than the age 
    // value given by the attribute
    // -------------------------------------------------------------------------

    const char* app = 0;
    if (!(app = openOpaque->Get("eos.app")) ||
        (
         (strcmp(app, "balancer")) &&
         (strcmp(app, "drainer")) &&
         (strcmp(app, "converter"))
         )
        )
    {
      // we are supposed to update the change time with the access since this
      // is any kind of external access
      time_t now = time(NULL);
      XrdOucString sage = attrmap["sys.force.atime"].c_str();
      time_t age = eos::common::StringConversion::GetSizeFromString(sage);
      eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
      try
      {
        fmd = gOFS->eosView->getFile(path);
        eos::FileMD::ctime_t ctime;
        fmd->getCTime(ctime);
        if ((ctime.tv_sec + age) < now)
        {
          // only update within the resolution of the access tracking
          fmd->setCTimeNow();
          gOFS->eosView->updateFileStore(fmd);
        }
        errno = 0;
      }
      catch (eos::MDException &e)
      {
        errno = e.getErrno();
        eos_warning("msg=\"failed to update access time\" path=\"%s\" ec=%d emsg=\"%s\"\n",
                    path, e.getErrno(), e.getMessage().str().c_str());
      }
    }
  }
  EXEC_TIMING_END("Open");

  return rcode;
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::close ()
/*----------------------------------------------------------------------------*/
/*
 * @brief close a file object
 * 
 * @return SFS_OK
 * 
 * The close on the MGM is called only for files opened using the 'proc' e.g.
 * EOS shell comamnds. By construction failures can happen only during the open
 * of a 'proc' file e.g. the close always succeeds! 
 */
/*----------------------------------------------------------------------------*/
{
  oh = -1;
  if (fname)
  {
    free(fname);
    fname = 0;
  }

  if (procCmd)
  {

    procCmd->close();
    return SFS_OK;
  }
  return SFS_OK;
}

XrdSfsXferSize
XrdMgmOfsFile::read (XrdSfsFileOffset offset,
                     char *buff,
                     XrdSfsXferSize blen)
/*----------------------------------------------------------------------------*/
/*
 * read a partial result of a 'proc' interface command
 * 
 * @param offset where to read from the result
 * @param buff buffer where to place the result
 * @param blen maximum size to read
 * 
 * @return number of bytes read upon success or SFS_ERROR
 * 
 * This read is only used to stream back 'proc' command results to the EOS 
 * shell since all normal files get a redirection or error during the file open.
 */
/*----------------------------------------------------------------------------*/

{
  static const char *epname = "read";

  if (isZeroSizeFile)
  {
    return 0;
  }
  // Make sure the offset is not too large
  //
#if _FILE_OFFSET_BITS!=64
  if (offset > 0x000000007fffffff)
    return Emsg(epname, error, EFBIG, "read", fname);
#endif

  if (procCmd)
  {

    return procCmd->read(offset, buff, blen);
  }

  return Emsg(epname, error, EOPNOTSUPP, "read", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::read (XrdSfsAio * aiop)
/*----------------------------------------------------------------------------*/
/*
 * aio flavour of a read - not supported
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{
  static const char *epname = "read";
  if (isZeroSizeFile)
    return 0;

  // Execute this request in a synchronous fashion
  //
  return Emsg(epname, error, EOPNOTSUPP, "read", fname);
}

/*----------------------------------------------------------------------------*/
XrdSfsXferSize
XrdMgmOfsFile::write (XrdSfsFileOffset offset,
                      const char *buff,
                      XrdSfsXferSize blen)
/*----------------------------------------------------------------------------*/
/*
 * @brief write a block to an open file - not implemented (no use case)
 * 
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{
  static const char *epname = "write";


  // Make sure the offset is not too large
  //
#if _FILE_OFFSET_BITS!=64

  if (offset > 0x000000007fffffff)
    return Emsg(epname, error, EFBIG, "write", fname);
#endif

  return Emsg(epname, error, EOPNOTSUPP, "write", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::write (XrdSfsAio * aiop)
/*----------------------------------------------------------------------------*/
/*
 * @brief write a block to an open file - not implemented (no use case)
 * 
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{

  static const char *epname = "write";
  // Execute this request in a synchronous fashion
  return Emsg(epname, error, EOPNOTSUPP, "write", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::stat (struct stat * buf)
/*----------------------------------------------------------------------------*/
/*
 * @brief stat the size of an open 'proc' command/file
 * 
 * @param buf stat struct where to store information
 * @return SFS_OK if open proc file otherwise SFS_ERROR
 * 
 * For 'proc' files the result is created during the file open call.
 * The stat function will fill the size of the created result into the stat
 * buffer.
 */
/*----------------------------------------------------------------------------*/

{
  static const char *epname = "stat";

  if (isZeroSizeFile)
  {
    memset(buf, 0, sizeof (struct stat));
    return 0;
  }

  if (procCmd)
    return procCmd->stat(buf);

  return Emsg(epname, error, EOPNOTSUPP, "stat", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::sync ()
/*----------------------------------------------------------------------------*/
/*
 * sync an open file - no implemented (no use case)
 * 
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{

  static const char *epname = "sync";
  return Emsg(epname, error, EOPNOTSUPP, "sync", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::sync (XrdSfsAio * aiop)
/*----------------------------------------------------------------------------*/
/*
 * aio sync an open file - no implemented (no use case)
 * 
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{

  static const char *epname = "sync";
  // Execute this request in a synchronous fashion
  //
  return Emsg(epname, error, EOPNOTSUPP, "sync", fname);
}

/*----------------------------------------------------------------------------*/
int
XrdMgmOfsFile::truncate (XrdSfsFileOffset flen)
/*----------------------------------------------------------------------------*/
/*
 * truncate an open file - no implemented (no use case)
 * 
 * @return SFS_ERROR and EOPNOTSUPP
 */
/*----------------------------------------------------------------------------*/
{
  static const char *epname = "trunc";
  // Make sure the offset is not too larg
#if _FILE_OFFSET_BITS!=64

  if (flen > 0x000000007fffffff)
    return Emsg(epname, error, EFBIG, "truncate", fname);
#endif

  return Emsg(epname, error, EOPNOTSUPP, "truncate", fname);
}

/*----------------------------------------------------------------------------*/
XrdMgmOfsFile::~XrdMgmOfsFile ()
/*----------------------------------------------------------------------------*/
/*
 * @brief destructor
 * 
 * Cleans-up the file object on destruction 
 */
/*----------------------------------------------------------------------------*/
{
  if (oh > 0) close();
  if (openOpaque)
  {
    delete openOpaque;
    openOpaque = 0;
  }
  if (procCmd)
  {

    delete procCmd;
    procCmd = 0;
  }
}
