//------------------------------------------------------------------------------
//! @file PlainLayout.hh
//! @author Elvin-Alin Sindrilaru / Andreas-Joachim Peters - CERN
//! @brief Physical layout of a plain file without any replication or striping
//------------------------------------------------------------------------------

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

#ifndef __EOSFST_PLAINLAYOUT_HH__
#define __EOSFST_PLAINLAYOUT_HH__

/*----------------------------------------------------------------------------*/
#include "common/LayoutId.hh"
#include "fst/Namespace.hh"
#include "fst/XrdFstOfsFile.hh"
#include "fst/layout/Layout.hh"
/*----------------------------------------------------------------------------*/
#include "XrdOuc/XrdOucString.hh"
#include "XrdOfs/XrdOfs.hh"
/*----------------------------------------------------------------------------*/

EOSFSTNAMESPACE_BEGIN


//------------------------------------------------------------------------------
//! Class abstracting the phsysical layout of a plain file
//------------------------------------------------------------------------------
class PlainLayout : public Layout
{
  public:

    //--------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param file file handler
    //! @param lid layout id
    //! @param client security information
    //! @param error error information
    //!
    //--------------------------------------------------------------------------
    PlainLayout( XrdFstOfsFile*      file,
                 int                 lid,
                 const XrdSecEntity* client,
                 XrdOucErrInfo*      error );

  
    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~PlainLayout();

  
    //--------------------------------------------------------------------------
    //! Open file
    //!
    //! @param path file path
    //! @param flags open flags
    //! @param mode open mode
    //! @param opaque opaque information
    //!
    //--------------------------------------------------------------------------
    virtual int Open( const std::string&  path,
                      uint16_t            flags,
                      uint16_t            mode,
                      const char*         opaque );

  
    //--------------------------------------------------------------------------
    //! Read from file
    //!
    //! @param offset offset
    //! @param buffer place to hold the read data
    //! @param length length
    //!
    //! @return number of bytes read
    //!
    //--------------------------------------------------------------------------
    virtual int Read( uint64_t offset,
                      char*    buffer,
                      uint32_t length );


    //--------------------------------------------------------------------------
    //! Write to file
    //!
    //! @param offset offset
    //! @paramm buffer data to be written
    //! @param length length
    //!
    //! @return number of bytes written
    //!
    //--------------------------------------------------------------------------
    virtual int Write( uint64_t offset,
                       char*    buffer,
                       uint32_t length );

  
    //--------------------------------------------------------------------------
    //! Truncate
    //!
    //! @param offset truncate file to this value
    //!
    //! @return 0 if successful, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Truncate( uint64_t offset );

  
    //--------------------------------------------------------------------------
    //! Allocate file space
    //!
    //! @param length space to be allocated
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Fallocate( uint64_t length );

  
    //--------------------------------------------------------------------------
    //! Deallocate file space
    //!
    //! @param fromOffset offset start
    //! @param toOffset offset end
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Fdeallocate( uint64_t fromOffset,
                             uint64_t toOffset );

  
    //--------------------------------------------------------------------------
    //! Remove file
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Remove();

  
    //--------------------------------------------------------------------------
    //! Sync file to disk
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Sync();

  
    //--------------------------------------------------------------------------
    //! Close file
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Close();

  
    //--------------------------------------------------------------------------
    //! Get stats about the file
    //!
    //! @param buf stat buffer
    //!
    //! @return 0 on success, error code otherwise
    //!
    //--------------------------------------------------------------------------
    virtual int Stat( struct stat* buf );

};

EOSFSTNAMESPACE_END
#endif
