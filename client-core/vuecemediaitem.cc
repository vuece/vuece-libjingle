#ifdef WIN32
#include "talk/base/win32.h"
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>
#endif  // WIN32

#include "vuecemediaitem.h"

namespace vuece
{


VueceMediaItem::VueceMediaItem()
{
	iNChannels = 0;
	iBitRate = 0;
	iDuration = 0;
	iSize = 0;
	bIsValid = false;
	iNumSongs = 0;
	iNumDirs = 0;
	iSampleRate = 0;
	iName = "";
	iUriUtf8 = "";
	iParentUriUtf8 = "";
	iPath = "";
	bIsFolder = false;

	iArtist = TagLib::String::null;
	iAlbum = TagLib::String::null;
	iTitle = TagLib::String::null;
}

std::string VueceMediaItem::Path() const
{
	return iPath;
}

int VueceMediaItem::NumSongs()
{
	return iNumSongs;
}

void VueceMediaItem::SetNumSongs(int i)
{
	iNumSongs = i;
}

int VueceMediaItem::NumDirs()
{
	return iNumDirs;
}

void VueceMediaItem::SetNumDirs(int i)
{
	iNumDirs = i;
}

bool VueceMediaItem::IsValid() const {
  return bIsValid;
}

bool VueceMediaItem::IsFolder() const {
	return bIsFolder;
}

std::string VueceMediaItem::Type() const{
	if(bIsFolder){
		return "dir";
	}else{
		return "file";
	}
}

void VueceMediaItem::SetPath(const std::string& n)
{
	iPath = n;
}

void VueceMediaItem::SetValid(bool b)
{
	bIsValid = b;
}

void VueceMediaItem::SetFolder(bool b)
{
	bIsFolder = b;
}

std::string VueceMediaItem::Name() const {
  return iName;
}

std::string VueceMediaItem::Uri() const {
  return iUriUtf8;
}

TagLib::String VueceMediaItem::Artist() const {
  return iArtist;
}

TagLib::String VueceMediaItem::Album() const {
	return iAlbum;
}

TagLib::String VueceMediaItem::Title() const {
	return iTitle;
}

int VueceMediaItem::Size() const {
  return iSize;
}

int VueceMediaItem::BitRate() const {
  return iBitRate;
}

int VueceMediaItem::Duration() const {
  return iDuration;
}

void VueceMediaItem::SetName(const std::string& n)
{
	iName = n;
}

void VueceMediaItem::SetUriUtf8(const std::string& n)
{
	iUriUtf8 = n;
}

std::string VueceMediaItem::ParentUri() const
{
	return iParentUriUtf8;
}

void VueceMediaItem::SetParentUriUtf8(const std::string& n)
{
	iParentUriUtf8 = n;
}

void VueceMediaItem::SetBitRate(int br)
{
	iBitRate = br;
}

void VueceMediaItem::SetDuration(int dur)
{
	iDuration = dur;
}
void VueceMediaItem::SetSize(int s)
{
	iSize = s;
}

void VueceMediaItem::SetArtist(const TagLib::String& n)
{
	iArtist = n;
}

void VueceMediaItem::SetAlbum(const TagLib::String& n)
{
	iAlbum = n;
}

void VueceMediaItem::SetTitle(const TagLib::String& n)
{
	iTitle = n;
}

void VueceMediaItem::SetSampleRate(int s)
{
	iSampleRate = s;
}

int VueceMediaItem::SampleRate() const
{
	return iSampleRate;
}

void VueceMediaItem::SetNChannels(int i)
{
	iNChannels = i;
}

int VueceMediaItem::NChannels() const
{
	return iNChannels;
}

std::string VueceMediaItem::ArtistB64() const
{
	return iArtist_b64;
}
std::string VueceMediaItem::AlbumB64() const
{
	return iAlbum_b64;
}
std::string VueceMediaItem::TitleB64() const
{
	return iTitle_b64;
}

void VueceMediaItem::SetArtistB64(const std::string& n)
{
	iArtist_b64 = n;
}
void VueceMediaItem::SetAlbumB64(const std::string& n)
{
	iAlbum_b64 = n;
}
void VueceMediaItem::SetTitleB64(const std::string& n)
{
	iTitle_b64 = n;
}



}

