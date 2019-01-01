/*
 * VueceShareCommon.h
 *
 *  Created on: Jul 14, 2012
 *      Author: Jingjing Sun
 */


#ifndef VUECE_SHARE_COMMON_
#define VUECE_SHARE_COMMON_

#include <map>
#include <string>
#include "talk/base/stringutils.h"
#include "talk/base/messagequeue.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {
class VueceMediaStreamSession;
}

typedef std::map<std::string, std::string> SessionId2VueceShareIdMap;
typedef std::map<std::string, cricket::VueceMediaStreamSession *>  VueceShareId2SessionMap;


namespace cricket {

///////////////////////////////////////////////////////////////////////////////
// FileShareManifest
///////////////////////////////////////////////////////////////////////////////

class FileShareManifest {
public:

  FileShareManifest(){};

  enum FileType
  {
	  T_FILE = 0,
	  T_IMAGE,
	  T_FOLDER ,
	  T_MUSIC,
	  T_NONE
  };

  enum { SIZE_UNKNOWN = talk_base::SIZE_UNKNOWN };

  struct Item {
	FileType type;
    std::string name;
    size_t size, width, height;
    int bit_rate, sample_rate, nchannels, duration;
  };

  typedef std::vector<Item> ItemList;

  inline bool empty() const { return items_.empty(); }
  inline size_t size() const { return items_.size(); }
  inline const Item& item(size_t index) const { return items_[index]; }

  void AddFile(const std::string& name, size_t size);
  void AddImage(const std::string& name, size_t size,
                size_t width, size_t height);
  void AddFolder(const std::string& name, size_t size);
//  void AddMusic(const std::string& name, size_t size,
//                size_t width, size_t height);

	void AddMusic(
			const std::string& name,
			size_t size,
			size_t width,
			size_t height,
			int bit_rate,
			int sample_rate,
			int nchannels,
			int duration);

  size_t GetItemCount(FileType t) const;
  inline size_t GetFileCount() const { return GetItemCount(T_FILE); }
  inline size_t GetImageCount() const { return GetItemCount(T_IMAGE); }
  inline size_t GetFolderCount() const { return GetItemCount(T_FOLDER); }
  inline size_t GetMusicCount() const { return GetItemCount(T_MUSIC); }

private:
  ItemList items_;
};

class FileContentDescription : public ContentDescription
{
public:
	FileContentDescription() : supports_http(false) { }
	~FileContentDescription(){}
    FileShareManifest manifest;
    bool supports_http;
    std::string source_path;
    std::string preview_path;
};

enum FileShareState {
  FS_NONE,          // Initialization
  FS_OFFER,         // Offer extended
  FS_TRANSFER,      // In progress
  FS_COMPLETE,      // Completed successfully
  FS_LOCAL_CANCEL,  // Local side cancelled
  FS_REMOTE_CANCEL, // Remote side cancelled
  FS_FAILURE,        // An error occurred during transfer
  FS_TERMINATED,
  FS_RESOURCE_RELEASED
};

class VueceStreamUtil
{
public:
	static bool GetStreamDurationInMilliSecs(const char* filePath, size_t* duration);

};


}  // namespace cricket

#endif  // VUECE_SHARE_COMMON_
