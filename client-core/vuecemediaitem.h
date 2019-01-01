/*
 * vuecemediaitem.h
 *
 *  Created on: Dec 14, 2012
 *      Author: Jingjing Sun
 */

#ifndef VUECEMEDIAITEM_H_
#define VUECEMEDIAITEM_H_

#include <tag.h>
#include <list>

namespace vuece
{

class VueceMediaItem {
public:
	VueceMediaItem();

	int SampleRate() const;
	int BitRate() const;
	int NChannels() const;

	int Duration() const;
	int Size() const;
	std::string Name() const;
	std::string Uri() const;
	std::string ParentUri() const;
	std::string Type() const;
	std::string Path() const;
	bool IsValid() const;
	TagLib::String Artist() const;
	TagLib::String Album() const;
	TagLib::String Title() const;
	std::string ArtistB64() const;
	std::string AlbumB64() const;
	std::string TitleB64() const;
	bool IsFolder() const;
	int NumSongs();
	int NumDirs();

	void SetName(const std::string& n);
	void SetUriUtf8(const std::string& n);
	void SetParentUriUtf8(const std::string& n);
	void SetBitRate(int br);
	void SetDuration(int dur);
	void SetSize(int s);
	void SetValid(bool b);
	void SetFolder(bool b);
	void SetArtist(const TagLib::String& n);
	void SetAlbum(const TagLib::String& n);
	void SetTitle(const TagLib::String& n);
	void SetArtistB64(const std::string& n);
	void SetAlbumB64(const std::string& n);
	void SetTitleB64(const std::string& n);
	void SetSampleRate(int s);
	void SetNumSongs(int i);
	void SetNumDirs(int i);
//	void SetType(const std::string& n);
	void SetPath(const std::string& n);
	void SetNChannels(int i);

private:
	int iBitRate;
	int iSampleRate;
	int iDuration;
	int iSize;
	int iNChannels;
	int iNumSongs;
	int iNumDirs;
	bool bIsValid;
	bool bIsFolder;
	std::string iName;
	std::string iUriUtf8;
	std::string iParentUriUtf8;
	std::string iPath;
	TagLib::String iArtist;
	TagLib::String iAlbum;
	TagLib::String iTitle;
	std::string iArtist_b64;
	std::string iAlbum_b64;
	std::string iTitle_b64;
//	std::string iType;
};

}

typedef std::list<vuece::VueceMediaItem*> VueceMediaItemList;

#endif /* VUECEMEDIAITEM_H_ */
