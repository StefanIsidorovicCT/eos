//------------------------------------------------------------------------------
// File: PlainLayout.cc
// Author: Andreas-Joachim Peters - CERN
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

/*----------------------------------------------------------------------------*/
#include "fst/layout/PlainLayout.hh"
#include "fst/layout/FileIoPlugin.hh"
/*----------------------------------------------------------------------------*/

EOSFSTNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
PlainLayout::PlainLayout( XrdFstOfsFile*      file,
                          int                 lid,
                          const XrdSecEntity* client,
                          XrdOucErrInfo*      error ) :
  Layout( file, lid, client, error )
{
  //............................................................................
  // For the plain layout we use only LocalFileIo type
  //............................................................................
  FileIo* fileIo = FileIoPlugin::GetIoObject( mOfsFile,
                                              eos::common::LayoutId::kLocal,
                                              mSecEntity,
                                              mError );
  mPhysicalFile.push_back( fileIo );
}


//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------

PlainLayout::~PlainLayout()
{
  //empty
}


//------------------------------------------------------------------------------
// Open File
//------------------------------------------------------------------------------
int
PlainLayout::Open( const std::string&  path,
                   uint16_t            flags,
                   uint16_t            mode,
                   const char*         opaque )
{
  eos_debug( "path = %s", path.c_str() );
  mLocalPath = path;
  return mPhysicalFile[0]->Open( path, flags, mode, opaque );
}


//------------------------------------------------------------------------------
// Read from file
//------------------------------------------------------------------------------
int
PlainLayout::Read( uint64_t offset, char* buffer, uint32_t length )
{
  eos_debug( "offset = %llu, length = %lu", offset, length );
  return mPhysicalFile[0]->Read( offset, buffer, length );
}


//------------------------------------------------------------------------------
// Write to file
//------------------------------------------------------------------------------
int
PlainLayout::Write( uint64_t offset, char* buffer, uint32_t length )
{
  eos_debug( "offset = %llu, length = %lu", offset, length );
  return mPhysicalFile[0]->Write( offset, buffer, length );
}


//------------------------------------------------------------------------------
// Truncate file
//------------------------------------------------------------------------------
int
PlainLayout::Truncate( uint64_t offset )
{
  eos_debug( "offset = %llu", offset );
  return mPhysicalFile[0]->Truncate( offset );
}


//------------------------------------------------------------------------------
// Reserve space for file
//------------------------------------------------------------------------------
int
PlainLayout::Fallocate( uint64_t length )
{
  eos_debug( "length = %llu", length );
  return mPhysicalFile[0]->Fallocate( length );
}


//------------------------------------------------------------------------------
// Deallocate reserved space
//------------------------------------------------------------------------------
int
PlainLayout::Fdeallocate( uint64_t fromOffset, uint64_t toOffset )
{
  eos_debug( "from = %llu, to = %llu", fromOffset, toOffset );
  return mPhysicalFile[0]->Fdeallocate( fromOffset, toOffset );
}


//------------------------------------------------------------------------------
// Syn file to disk
//------------------------------------------------------------------------------
int
PlainLayout::Sync()
{
  eos_debug( " " );
  return mPhysicalFile[0]->Sync();
}


//------------------------------------------------------------------------------
// Get stats for file
//------------------------------------------------------------------------------
int
PlainLayout::Stat( struct stat* buf )
{
  eos_debug( " " );
  return mPhysicalFile[0]->Stat( buf );
}


//------------------------------------------------------------------------------
// Close file
//------------------------------------------------------------------------------
int
PlainLayout::Close()
{
  eos_debug( " " );
  return mPhysicalFile[0]->Close();
}


//------------------------------------------------------------------------------
// Remove file
//------------------------------------------------------------------------------
int
PlainLayout::Remove()
{
  eos_debug( " " );
  return mPhysicalFile[0]->Remove();
}

EOSFSTNAMESPACE_END
