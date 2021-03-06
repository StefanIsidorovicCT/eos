/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2015 CERN/Switzerland                                  *
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

//------------------------------------------------------------------------------
//! @author Elvin Sindrilaru <esindril@cern.ch>
//! @brief Class representing the file metadata interface
//------------------------------------------------------------------------------

#ifndef EOS_NS_IFILE_MD_HH
#define EOS_NS_IFILE_MD_HH

#include "namespace/Namespace.hh"
#include "namespace/utils/Buffer.hh"
#include "namespace/interface/IContainerMD.hh"
#include <stdint.h>
#include <string>
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

class IFileMDSvc;

//------------------------------------------------------------------------------
//! Interface to file metadata
//------------------------------------------------------------------------------
class IFileMD
{
public:
  //----------------------------------------------------------------------------
  //! Type definitions
  //----------------------------------------------------------------------------
  typedef uint64_t id_t;
  typedef uint32_t location_t;
  typedef uint32_t layoutId_t;
  typedef struct timespec ctime_t;
  typedef std::vector<location_t> LocationVector;
  typedef std::map<std::string, std::string> XAttrMap;

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  IFileMD() {};

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~IFileMD() {};

  //----------------------------------------------------------------------------
  //! Get file id
  //----------------------------------------------------------------------------
  virtual id_t getId() const = 0;

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  virtual void getCTime(ctime_t& ctime) const = 0;

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  virtual void setCTime(ctime_t ctime) = 0;

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  virtual void setCTimeNow() = 0;

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  virtual void getMTime(ctime_t& mtime) const = 0;

  //----------------------------------------------------------------------------
  //! Set modification time
  //----------------------------------------------------------------------------
  virtual void setMTime(ctime_t mtime) = 0;

  //----------------------------------------------------------------------------
  //! Set modification time to now
  //----------------------------------------------------------------------------
  virtual void setMTimeNow() = 0;

  //----------------------------------------------------------------------------
  //! Get size
  //----------------------------------------------------------------------------
  virtual uint64_t getSize() const = 0;

  //----------------------------------------------------------------------------
  //! Set size - 48 bytes will be used
  //----------------------------------------------------------------------------
  virtual void setSize(uint64_t size) = 0;

  //----------------------------------------------------------------------------
  //! Get tag
  //----------------------------------------------------------------------------
  virtual IContainerMD::id_t getContainerId() const = 0;

  //----------------------------------------------------------------------------
  //! Set tag
  //----------------------------------------------------------------------------
  virtual void setContainerId(IContainerMD::id_t containerId) = 0;

  //----------------------------------------------------------------------------
  //! Get checksum
  //----------------------------------------------------------------------------
  virtual const Buffer& getChecksum() const = 0;

  //----------------------------------------------------------------------------
  //! Compare checksums
  //! WARNING: you have to supply enough bytes to compare with the checksum
  //! stored in the object!
  //----------------------------------------------------------------------------
  virtual bool checksumMatch(const void* checksum) const = 0;

  //----------------------------------------------------------------------------
  //! Set checksum
  //----------------------------------------------------------------------------
  virtual void setChecksum(const Buffer& checksum) = 0;

  //----------------------------------------------------------------------------
  //! Clear checksum
  //----------------------------------------------------------------------------
  virtual void clearChecksum(uint8_t size = 20) = 0;

  //----------------------------------------------------------------------------
  //! Set checksum
  //!
  //! @param checksum address of a memory location string the checksum
  //! @param size     size of the checksum in bytes
  //----------------------------------------------------------------------------
  virtual void setChecksum(const void* checksum, uint8_t size) = 0;

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  virtual const std::string getName() const = 0;

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  virtual void setName(const std::string& name) = 0;

  //----------------------------------------------------------------------------
  //! Add location
  //----------------------------------------------------------------------------
  virtual void addLocation(location_t location) = 0;

  //----------------------------------------------------------------------------
  //! Get vector with all the locations
  //----------------------------------------------------------------------------
  virtual LocationVector getLocations() const = 0;

  //----------------------------------------------------------------------------
  //! Get location
  //----------------------------------------------------------------------------
  virtual location_t getLocation(unsigned int index) = 0;

  //----------------------------------------------------------------------------
  //! Replace location by index
  //----------------------------------------------------------------------------
  virtual void replaceLocation(unsigned int index, location_t newlocation) = 0;

  //----------------------------------------------------------------------------
  //! Remove location that was previously unlinked
  //----------------------------------------------------------------------------
  virtual void removeLocation(location_t location) = 0;

  //----------------------------------------------------------------------------
  //! Remove all locations that were previously unlinked
  //----------------------------------------------------------------------------
  virtual void removeAllLocations() = 0;

  //----------------------------------------------------------------------------
  //! Get vector with all unlinked locations
  //----------------------------------------------------------------------------
  virtual LocationVector getUnlinkedLocations() const = 0;

  //----------------------------------------------------------------------------
  //! Unlink location
  //----------------------------------------------------------------------------
  virtual void unlinkLocation(location_t location) = 0;

  //----------------------------------------------------------------------------
  //! Unlink all locations
  //----------------------------------------------------------------------------
  virtual void unlinkAllLocations() = 0;

  //----------------------------------------------------------------------------
  //! Clear unlinked locations without notifying the listeners
  //----------------------------------------------------------------------------
  virtual void clearUnlinkedLocations() = 0;

  //----------------------------------------------------------------------------
  //! Test the unlinkedlocation
  //----------------------------------------------------------------------------
  virtual bool hasUnlinkedLocation(location_t location) = 0;

  //----------------------------------------------------------------------------
  //! Get number of unlinked locations
  //----------------------------------------------------------------------------
  virtual size_t getNumUnlinkedLocation() const = 0;

  //----------------------------------------------------------------------------
  //! Clear locations without notifying the listeners
  //----------------------------------------------------------------------------
  virtual void clearLocations() = 0;

  //----------------------------------------------------------------------------
  //! Test the location
  //----------------------------------------------------------------------------
  virtual bool hasLocation(location_t location) = 0;

  //----------------------------------------------------------------------------
  //! Get number of location
  //----------------------------------------------------------------------------
  virtual size_t getNumLocation() const = 0;

  //----------------------------------------------------------------------------
  //! Get uid
  //----------------------------------------------------------------------------
  virtual uid_t getCUid() const = 0;

  //----------------------------------------------------------------------------
  //! Set uid
  //----------------------------------------------------------------------------
  virtual void setCUid(uid_t uid) = 0;

  //----------------------------------------------------------------------------
  //! Get gid
  //----------------------------------------------------------------------------
  virtual gid_t getCGid() const = 0;

  //----------------------------------------------------------------------------
  //! Set gid
  //----------------------------------------------------------------------------
  virtual void setCGid(gid_t gid) = 0;

  //----------------------------------------------------------------------------
  //! Get layout
  //----------------------------------------------------------------------------
  virtual layoutId_t getLayoutId() const = 0;

  //----------------------------------------------------------------------------
  //! Set layout
  //----------------------------------------------------------------------------
  virtual void setLayoutId(layoutId_t layoutId) = 0;

  //----------------------------------------------------------------------------
  //! Get flags
  //----------------------------------------------------------------------------
  virtual uint16_t getFlags() const = 0;

  //----------------------------------------------------------------------------
  //! Get the n-th flag
  //----------------------------------------------------------------------------
  virtual bool getFlag(uint8_t n) = 0;

  //----------------------------------------------------------------------------
  //! Set flags
  //----------------------------------------------------------------------------
  virtual void setFlags(uint16_t flags) = 0;

  //----------------------------------------------------------------------------
  //! Set the n-th flag
  //----------------------------------------------------------------------------
  virtual void setFlag(uint8_t n, bool flag) = 0;

  //----------------------------------------------------------------------------
  //! Env Representation
  //----------------------------------------------------------------------------
  virtual void getEnv(std::string& env, bool escapeAnd = false) = 0;

  //----------------------------------------------------------------------------
  //! Set the FileMDSvc object
  //----------------------------------------------------------------------------
  virtual void setFileMDSvc(IFileMDSvc* fileMDSvc) = 0;

  //----------------------------------------------------------------------------
  //! Get the FileMDSvc object
  //----------------------------------------------------------------------------
  virtual IFileMDSvc* getFileMDSvc() = 0;

  //----------------------------------------------------------------------------
  //! Get symbolic link
  //----------------------------------------------------------------------------
  virtual std::string getLink() const = 0;

  //----------------------------------------------------------------------------
  //! Set symbolic link
  //----------------------------------------------------------------------------
  virtual void setLink(std::string link) = 0;

  //----------------------------------------------------------------------------
  //! Check if symbolic link
  //----------------------------------------------------------------------------
  virtual bool isLink() const = 0;

  //----------------------------------------------------------------------------
  //! Add extended attribute
  //----------------------------------------------------------------------------
  virtual void setAttribute(const std::string& name,
                            const std::string& value) = 0;

  //----------------------------------------------------------------------------
  //! Remove attribute
  //----------------------------------------------------------------------------
  virtual void removeAttribute(const std::string& name) = 0;

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  virtual bool hasAttribute(const std::string& name) const = 0;

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  virtual size_t numAttributes() const = 0;

  //----------------------------------------------------------------------------
  //! Get the attribute
  //----------------------------------------------------------------------------
  virtual std::string getAttribute(const std::string& name) const = 0;

  //----------------------------------------------------------------------------
  //! Get attribute begin iterator
  //----------------------------------------------------------------------------
  virtual XAttrMap::iterator attributesBegin() = 0;

  //----------------------------------------------------------------------------
  //! Get the attribute end iterator
  //----------------------------------------------------------------------------
  virtual XAttrMap::iterator attributesEnd() = 0;

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  virtual void serialize(Buffer& buffer) = 0;

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer
  //----------------------------------------------------------------------------
  virtual void deserialize(const Buffer& buffer) = 0;

private:
  //----------------------------------------------------------------------------
  //! Make copy constructor and assignment operator private to avoid "slicing"
  //! when dealing with derived classes.
  //----------------------------------------------------------------------------
  IFileMD(const IFileMD& other);

  IFileMD& operator=(const IFileMD& other);
};

EOSNSNAMESPACE_END

#endif // EOS_NS_IFILE_MD_HH
