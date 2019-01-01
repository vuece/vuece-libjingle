/*
 * VueceWinUtilities.cc
 *
 *  Created on: 2013-7-21
 *      Author: Jingjing Sun
 */

#include "VueceWinUtilities.h"


#define cimg_use_jpeg
#define XMD_H
#define HAVE_BOOLEAN
#define cimg_use_png
#include "CImg.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "talk/base/stringdigest.h"
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"

#include <openssl/md5.h>

//headers from taglib
#include <fileref.h>
#include <tag.h>
#include <id3v2/id3v2tag.h>
#include <mpegfile.h>
#include <id3v2/id3v2frame.h>
#include <id3v2/id3v2header.h>
#include <id3v2/frames/attachedpictureframe.h>

#include "tinyxml2.h"
#include "VueceCrypt.h"
#include "VueceGlobalSetting.h"
#include <Winhttp.h>

#include "libjson.h"
#include "JSONNode.h"

#include "VueceConstants.h"
#include "VueceConfig.h"

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace Microsoft::Win32;

using namespace tinyxml2;

using namespace cimg_library;

static bool ExtracUserInfoFromResp (const char* response, char* user_account, char* display_name, char* img_url);

void VueceWinUtilities::MarshalString ( String ^ s, std::string& os )
{
	using namespace Runtime::InteropServices;
	const char* chars =  (const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
	os = chars;
	Marshal::FreeHGlobal(IntPtr((void*)chars));
}

bool VueceWinUtilities::GenUUID(char* result )
{
		#ifdef WIN32
			GUID guid;
			WCHAR *pszUuid = 0;

			// Create Unique ID
			if( CoCreateGuid( &guid ) != S_OK)
			{
				return false;
			}

			sprintf(result, "%.2X-%.2X-%.2X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X",
					guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
					guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

			LOG(LS_VERBOSE) << "VueceWinUtilities:GenUUID: " << result;

			return true;
		#else
			return false;
		#endif
}

// Convert an UTF8 string to a wide Unicode String
 std::wstring VueceWinUtilities::utf8_decode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string VueceWinUtilities::ws2s(const std::wstring& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
    char* buf = new char[len];
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, buf, len, 0, 0);
    std::string r(buf);
    delete[] buf;
    return r;
}

 std::string VueceWinUtilities::utf8_encode(const std::wstring &wstr)
	{
	    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	    std::string strTo( size_needed, 0 );
	    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	    return strTo;
	}

	 std::wstring VueceWinUtilities::s2ws(const std::string& s)
	{
	    int len;
	    int slength = (int)s.length() + 1;
	    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	    wchar_t* buf = new wchar_t[len];
	    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	    std::wstring r(buf);
	    delete[] buf;
	    return r;
	}

	 void VueceWinUtilities:: ConvertCharArrayToLPCWSTR(const char* charArray, wchar_t* result, int result_size)
	 {
	     MultiByteToWideChar(CP_ACP, 0, charArray, -1, result, result_size);
	 }

	 std::string VueceWinUtilities::wstrtostr(const std::wstring &wstr)
	{
	    // Convert a Unicode string to an ASCII string
	    std::string strTo;
	    char *szTo = new char[wstr.length() + 1];
	    szTo[wstr.size()] = '\0';
	    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, szTo, (int)wstr.length(), NULL, NULL);
	    strTo = szTo;
	    delete[] szTo;
	    return strTo;
	}


	bool VueceWinUtilities::replace(std::string& str, const std::string& from, const std::string& to) {
	    size_t start_pos = str.find(from);
	    if(start_pos == std::string::npos)
	        return false;
	    str.replace(start_pos, from.length(), to);
	    return true;
	}

	std::string VueceWinUtilities::tojsonstr(const TagLib::String s)
	{
		std::string tmp = s.to8Bit(!s.isLatin1()&!s.isAscii());
		//LOG(LS_VERBOSE) << "s:non-unicode:" << s.to8Bit();
		//LOG(LS_VERBOSE) << "s:unicode:" << s.to8Bit(true);
		//LOG(LS_VERBOSE) << "s:latin:" << s.isLatin1(); // false: already utf8 encoded
		//LOG(LS_VERBOSE) << "s:ascii:" << s.isAscii(); // true: english words only
		if(tmp.length() > 0)
		{
			if (!s.isLatin1()&!s.isAscii())
				return talk_base::Base64::Encode(tmp);//VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws(tmp)));
			else
				return talk_base::Base64::Encode(VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws(tmp)));
		}
		else
		{
			return "";
		}

	}


	vuece::VueceMediaItem* VueceWinUtilities::AnalyzeFile(
			const std::string &absFolderPathUtf8,
			const std::string& filename_non_utf8,
			const std::string& filename_utf8,
			struct VueceGlobalSetting *pVueceGlobalSetting)
		{
			size_t size = 0;
			talk_base::Pathname file_path_utf8 ("");
			talk_base::Pathname file_path ("");
			std::ostringstream os;

			ASSERT(absFolderPathUtf8.length() > 0);

			VueceMediaItem* mediaItem = new VueceMediaItem();

			file_path_utf8.AppendFolder(absFolderPathUtf8);
			file_path_utf8.AppendPathname(filename_utf8);

			file_path.AppendFolder(VueceWinUtilities::ws2s(VueceWinUtilities::utf8_decode(absFolderPathUtf8)));
			file_path.AppendPathname(filename_non_utf8);

			LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:File name: " << file_path.filename();
			LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:Absolute file path: " << file_path.pathname();

			LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:File name in utf8: " << file_path_utf8.filename();
			LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:Absolute file path in utf8: " << file_path_utf8.pathname();

			talk_base::Filesystem::GetFileSize(file_path_utf8, &size);
			LOG(LS_VERBOSE) << "File size: " << size;

			if(size > pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes)
			{
				LOG(LS_VERBOSE) << "File size exceeded the limitation: " << pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes;
				return NULL;
			}

			mediaItem->SetSize(size);
			mediaItem->SetName(filename_utf8);//UTF8 encoded for Android to display

			 std::string ext = file_path_utf8.extension();

			 LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:Extension: " << ext;

			 if(ext == ".mp3")
			 {
				 //this(ffmepg analyze) could be slow for a big folder
				if ( !RetrieveCriticalMediaInfo(
						mediaItem,
						file_path.pathname().c_str(),
						pVueceGlobalSetting
						) )
				{
					mediaItem->SetValid(false);
					LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:RetrieveCriticalMediaInfo returned false, this item is invalid ";
					return mediaItem;
				}

				 //process file path
				 os << absFolderPathUtf8 << file_path_utf8.folder_delimiter() << filename_utf8;

				 std::string fpath = os.str();
				 std::string itemID = talk_base::MD5(fpath);

				LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:file path: " << fpath;
				LOG(LS_VERBOSE) << "VueceWinUtilities::AnalyzeFile:md5 key: " << itemID;

				mediaItem->SetUriUtf8(itemID);
				mediaItem->SetPath(talk_base::Base64::Encode(fpath));

				TagLib::FileRef f(file_path.pathname().c_str(), TagLib::AudioProperties::Accurate);

				if(!f.isNull() && f.tag())
				{

					  TagLib::Tag *tag = f.tag();

					  LOG(LS_VERBOSE) << "-- TAG --";
					  LOG(LS_VERBOSE) << "title   - [" << tag->title()   << "]";
					  LOG(LS_VERBOSE) << "artist  - [" << tag->artist()  << "]";
					  LOG(LS_VERBOSE) << "album   - [" << tag->album()   << "]";
					  LOG(LS_VERBOSE) << "year    - [" << tag->year()    << "]";
					  LOG(LS_VERBOSE) << "comment - [" << tag->comment() << "]";
					  LOG(LS_VERBOSE) << "track   - [" << tag->track()   << "]";
					  LOG(LS_VERBOSE) << "genre   - [" << tag->genre()   << "]";

					  LOG(LS_VERBOSE) << "album string len = " <<  tag->album().to8Bit(true).length();
					  LOG(LS_VERBOSE) << "album string len2 = " <<  tag->album().length();

					  if(tag->title()  == TagLib::String::null)
					  {
						  LOG(LS_VERBOSE) << "Title is emtpy";
					  }

					  if(tag->artist()  == TagLib::String::null)
					  {
						  LOG(LS_VERBOSE) << "artist is emtpy";
					  }

					  if(tag->album()  == TagLib::String::null)
					  {
						  LOG(LS_VERBOSE) << "Title is album";
					  }

					  if(tag->title()  == TagLib::String::null)
					  {
						  LOG(LS_VERBOSE) << "Title is emtpy";
					  }

					  mediaItem->SetTitle(tag->title());
					  mediaItem->SetArtist(tag->artist());
					  mediaItem->SetAlbum(tag->album());
				}


				mediaItem->SetValid(true);

				if(!f.isNull() && f.audioProperties())
				{

					TagLib::AudioProperties *properties = f.audioProperties();

					int seconds = properties->length() % 60;
					int minutes = (properties->length() - seconds) / 60;

					LOG(LS_VERBOSE) << "-- AUDIO --";
					LOG(LS_VERBOSE) << "bitrate     - " << properties->bitrate();
					LOG(LS_VERBOSE) << "sample rate - " << properties->sampleRate();
					LOG(LS_VERBOSE) << "channels    - " << properties->channels();
					LOG(LS_VERBOSE) << "length      - " << minutes << ":" << seconds;//formatSeconds(seconds);

					std::string tmp = filename_non_utf8;

					bool b = VueceWinUtilities::replace(tmp, "'", "\\'");

					LOG(LS_VERBOSE) << filename_non_utf8 << " replaced?(" << b << ") to " << tmp;

	//				mediaItem->SetBitRate(properties->bitrate());
	//				mediaItem->SetSampleRate(properties->sampleRate());
	//				mediaItem->SetDuration(properties->length());
				}
			 }

			 return mediaItem;
		}

  void VueceWinUtilities::CountSongsInFolder(const talk_base::Pathname&  p, int *dircount, int *filecount)
		{
				(*dircount) = 0;
				(*filecount) = 0;

				talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator( talk_base::Filesystem::IterateDirectory());

				LOG(LS_VERBOSE) << "VueceWinUtilities::CountSongsInFolder, path = " << p.pathname();

				if (! directoryIterator->Iterate(p))
				{

					LOG(LS_ERROR) << "VueceWinUtilities::CountSongsInFolder - Cannot iterate this folder.";
				}

				do {
							std::string filename = directoryIterator->Name();  //utf8 encoded string
							std::wstring widestr = utf8_decode(filename);  // utf8 decoded wstring
							std::string fn = ws2s(widestr);  // system default windows ansi code page

			//				LOG(LS_VERBOSE) << "Found a file: " << fn;

							if (!directoryIterator->IsDots())
							{
									if(directoryIterator->IsDirectory())
									{
										(*dircount)++;
									}
									else
									{
											talk_base::Pathname file_path(p);
											file_path.AppendPathname(fn);

											std::string ext = file_path.extension();

			//								LOG(LS_VERBOSE) << "File extension: " << ext;

											if(ext == ".mp3")
											{
												(*filecount)++;
											}
											else
											{
												LOG(LS_VERBOSE) << "This is not a valid mp3 file: " << fn;
											}
									}
							}
						}
						while (directoryIterator->Next());

				LOG(LS_VERBOSE) << "VueceWinUtilities::CountSongsInFolder, result = dir " << (*dircount) << " file " << (*filecount);

	}


	void VueceWinUtilities::GenerateJsonMsg(VueceMediaItemList* itemList, std::string targetUri, std::ostringstream& os)
	{
		bool sepFlag = false;
		int counter = 0;

		os << "{'action':'browse','reply':'ok','category':'music', 'uri': '" << targetUri << "', 'list':[";

		LOG(LS_VERBOSE) << "VueceWinUtilities::GenerateJsonMsg - Found " << itemList->size() << " items that can be shared.";

		std::list<vuece::VueceMediaItem*>::iterator iter = itemList->begin();

		while (iter != itemList->end()) {

			if(sepFlag)
			{
				os << ",";
			}

			VueceMediaItem* v = *iter;

			//Note no need to base64 encode the item name here because it's already encoded
			if(!v->IsFolder())
			{
				os << "{'name':'" <<  v->Name()   << "','bitrate':" << v->BitRate()
								<< ",'samplerate':" << v->SampleRate()
								<< ",'artist':'" << v->ArtistB64()
								<< "','album':'" << v->AlbumB64()
								<< "','title':'" << v->TitleB64()
								<< "','length':" << v->Duration()
								<< ",'size':" << v->Size()
								<< ",'uri':'" <<  v->Uri()  << "'}";

			}
			else
			{
				os << "{'name':'" <<  v->Name()  << "','type':'dir','num-dirs':" << v->NumDirs()
						<< ",'num-songs':" << v->NumSongs() << ",'uri':'" <<  v->Uri()  << "'}";
			}

			counter++;

//			LOG(LS_VERBOSE) << "items processed so far: " << counter;

			iter++;

			if(!sepFlag)
			{
				sepFlag = true;
			}
		}

		os << "]}";
	}

	void VueceWinUtilities::FindTheFirstImageFileInFolder(const std::string &absolute_folder_path, talk_base::Pathname* resultPath)
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::FindTheFirstImageFileInFolder - absolute_folder_path:" << absolute_folder_path;

			talk_base::Pathname folder_path (absolute_folder_path);
			talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator( talk_base::Filesystem::IterateDirectory());

			std::string path_utf8=VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws(absolute_folder_path));
			talk_base::Pathname folder_path_utf8 (path_utf8);

			LOG(LS_VERBOSE) << "VueceWinUtilities::FindTheFirstImageFileInFolder:absolute_folder_path = " << absolute_folder_path;

			if (directoryIterator->Iterate(folder_path_utf8))
			{
				do {
					std::string filename = directoryIterator->Name();  //utf8 encoded string

//					LOG(LS_VERBOSE) << "filename:" << filename;

					std::wstring widestr = VueceWinUtilities::utf8_decode(filename);  // utf8 decoded wstring

//					LOG(LS_VERBOSE) << "widestr:" << widestr;

					std::string fn = VueceWinUtilities::ws2s(widestr);  // system default windows ansi code page

//					LOG(LS_VERBOSE) << "Found a file: " << fn;

					if (!directoryIterator->IsDots())
					{
						if(directoryIterator->IsDirectory())
						{
						}
						else
						{
							talk_base::Pathname file_path(absolute_folder_path);
							file_path.AppendPathname(fn);

							//--------------------------------------------------------
							//IMPORTANT NOTE:
							//the following code won't work:
							//const char* ext = file_path.extension().c_str();
							//-------------------------------------------------------

							std::string ext0 = file_path.extension();
							const char* ext = ext0.c_str();

							if(IsFileHidden(file_path))
							{
//								LOG(LS_VERBOSE) << "This is a hidden file, ignore it.";
								continue;
							}

//							LOG(LS_VERBOSE) << "File extension: " << ext;

							if ( _stricmp(ext,".jpg") == 0 ||
									_stricmp(ext,".jpeg") == 0||
									_stricmp(ext,".jpe") == 0||
									_stricmp(ext,".jfif") == 0||
									_stricmp(ext,".jif")   == 0||
									_stricmp(ext,".png") == 0||
									_stricmp(ext,".bmp") == 0)
							{
								resultPath->AppendFolder(absolute_folder_path);
								resultPath->AppendPathname(fn);
								return;
							}
						}
					}
					else
					{
//						LOG(LS_VERBOSE) << "This is not a file or directory, skip.";
					}

				}
				while (directoryIterator->Next());
			}
			else
			{
				LOG(LS_ERROR) << "VueceWinUtilities::FindTheFirstImageFileInFolder:Cannot access this folder!";
			}
		}

	bool VueceWinUtilities::IsFileHidden(const talk_base::Pathname &path)
		{

				WIN32_FILE_ATTRIBUTE_DATA data = {0};

//				LOG(LS_VERBOSE) << "VueceWinUtilities::IsFileHidden - Checking file: " << path.pathname();

				if (0 == ::GetFileAttributesEx(talk_base::ToUtf16(path.pathname()).c_str(),
											 GetFileExInfoStandard, &data))
					return false;

				if( (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN)
				{
					return true;
				}

				return false;
		}

		bool VueceWinUtilities::ExtractAlbumArtFromFile(const std::string& src_path, std::ostringstream& result_preview_path)
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::ExtractAlbumArtFromFile, path = " << src_path;
			LOG(LS_VERBOSE) << "VueceWinUtilities::ExtractAlbumArtFromFile, result_preview_path = " << result_preview_path.str();

			bool res = false;
			static const char *IdPicture = "APIC";

			TagLib::MPEG::File mpegFile(src_path.c_str());

			TagLib::ID3v2::Tag *id3v2tag = mpegFile.ID3v2Tag();
			TagLib::ID3v2::FrameList Frame;
			TagLib::ID3v2::AttachedPictureFrame *PicFrame;
			void *pSrcImage;
			unsigned long imageSize;

			if( id3v2tag )
			{
				// picture frame
				Frame = id3v2tag->frameListMap()[IdPicture];
				if (!Frame.isEmpty() )
				{
					for(TagLib::ID3v2::FrameList::ConstIterator it = Frame.begin(); it != Frame.end(); ++it)
					{
						PicFrame = (TagLib::ID3v2::AttachedPictureFrame *)(*it);
						//  if ( PicFrame->type() ==
						//TagLib::ID3v2::AttachedPictureFrame::FrontCover)
						{
							// extract image (in it’s compressed form)
							//determine image format
							TagLib::String mType = PicFrame->mimeType();
							if(mType == "image/png" || mType == "PNG" )
							{
								result_preview_path << ".png";
							}
							else if(mType == "image/jpeg" || mType == "image/jpg" || mType == "JPG" || mType == "jpg")
							{
								result_preview_path << ".jpg";
							}
							else
							{
								LOG(LS_ERROR) << "VueceWinUtilities::ExtractAlbumArtFromFile - Unknown image format: " << mType;
							}

							LOG(LS_VERBOSE) << "VueceWinUtilities::ExtractAlbumArtFromFile - Final image file path: " << result_preview_path.str();

							imageSize = PicFrame->picture().size();

							LOG(LS_VERBOSE) << "VueceWinUtilities::ExtractAlbumArtFromFile - Image size: : " << imageSize << " bytes";

							if(imageSize <= 0)
							{
								LOG(LS_VERBOSE) << "VueceWinUtilities::ExtractAlbumArtFromFile - Image size is not valid, cannot extrac frame, return false now.";
								res = false;
							}
							else
							{
								pSrcImage = malloc ( imageSize );
								if ( pSrcImage )
								{
									FILE *jpegFile;

									//create and open the tmp file
									jpegFile = fopen(result_preview_path.str().c_str(), "wb");

									memcpy ( pSrcImage, PicFrame->picture().data(), imageSize );
									fwrite(pSrcImage,imageSize,1, jpegFile);
									fclose(jpegFile);
									free( pSrcImage );

									res = true;
								}
							}
						}
					}
				}
				else
				{
					LOG(LS_VERBOSE) << "VueceWinUtilities::id3v2 picture frame is empty." ;
				}
			}
			else
			{
				LOG(LS_VERBOSE) << "VueceWinUtilities::id3v2 not present" ;
			}

			return res;
		}

		bool VueceWinUtilities::ResizeImage(const char* filePath, std::string& destFilePath, int destW, int destH){

				CImg<unsigned char> image(filePath);
				int srcW = image.width();
				int srcH = image.height();
				int finalW;
				int finalH;
				std::ostringstream resizedFullFileName;

				resizedFullFileName << filePath;

				const char *const ext = cimg::split_filename(filePath);

				LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:File path: " << filePath;
				LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:File extension: " << ext;

				LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:destW: " << destW << ", destH: " << destH;

				LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:width: " << srcW;
				LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:height: " << srcH;

				if(srcW > destW && srcH > destH)
				{
					int scaledH = destW * srcH/ srcW;

					LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:: Size needs to be scaled.";

					finalW = destW;
					finalH = scaledH;

					LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage:: Resize image with final size, w = " << finalW
							<< ", h = " << finalH;

					image.resize(finalW, finalH);
				}
				else
				{
					LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage::Use original size.";

					finalW = srcW;
					finalH = srcH;
				}

				if ( _stricmp(ext,"jpg") == 0 ||
						_stricmp(ext,"jpeg") == 0 ||
						_stricmp(ext,"jpe") == 0 ||
						_stricmp(ext,"jfif") == 0 ||
						_stricmp(ext,"jif") == 0
				                 )
				{
					resizedFullFileName << "_resized.jpg";
					image.save_jpeg(resizedFullFileName.str().c_str());
				}
				 else if (_stricmp(ext,"png") == 0)
				 {
					 resizedFullFileName << "_resized.png";
					 image.save_png(resizedFullFileName.str().c_str());
				 }
				 else if (_stricmp(ext,"bmp")== 0)
				 {
					 resizedFullFileName << "_resized.bmp";
					 image.save_bmp(resizedFullFileName.str().c_str());
				 }
				 else
				 {
					 LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage::Unknown image format, image will not be resized";
				 }

				destFilePath = resizedFullFileName.str();

				 LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage::Final resized file path is: " << destFilePath;

				return false;
			}

		bool VueceWinUtilities::RetrieveCriticalMediaInfo(
				vuece::VueceMediaItem* mediaItem,
				const char* filePath,
				struct VueceGlobalSetting *pVueceGlobalSetting
				)
			{
				AVFormatContext *pFormatCtx;
				AVCodecContext  *pCodecCtx;
				size_t i;
				int targetAudioStreamIdx = -1;

				// Register all formats and codecs
				av_register_all();

				LOG(LS_VERBOSE) << "VueceWinUtilities::RetrieveCriticalMediaInfo:File path: " << filePath;

				//av_open_input_file is deprecated
		//		if(avformat_open_input(&pFormatCtx, (const char*)filePath, NULL, NULL)!=0)
				/**
				 * TODO - FIX THIS:
				 * [052:307] [1e48] VueceWinUtilities::AnalyzeFile:Extension: .mp3
					[052:307] [1e48] CallClient::RetrieveCriticalMediaInfo:File path: I:\VDownloader\Converted\VAMPS - MEMORIES (sub espa?ol).mp3
					[052:307] [1e48] Error(VueceWinUtilities.h:711): Cannot open this file.
					[052:307] [1e48] VueceWinUtilities::AnalyzeFile:file path: I:\VDownloader\Converted\VAMPS - MEMORIES (sub espa帽ol).mp3
					[052:307] [1e48] VueceWinUtilities::AnalyzeFile:md5 key: 2c6064e090210a18780e82f4bfcf8a10
				 */
				if(av_open_input_file(&pFormatCtx, (const char*)filePath, NULL, 0, NULL)!=0)
				{
					LOG(LS_ERROR) << "VueceWinUtilities - Cannot open this file.";
					return false;
				}

				// Retrieve stream information
				if(av_find_stream_info(pFormatCtx)<0)
				{
					LOG(LS_ERROR) << "Couldn't find stream information.";
					return false; // Couldn't find stream information
				}

				// Dump information about file
				LOG(LS_VERBOSE) << "===================== Dump Media File LS_VERBOSE Start =============================";
				av_dump_format(pFormatCtx, 0, (const char*)filePath, false);
				LOG(LS_VERBOSE) << "===================== Dump Media File LS_VERBOSE End =============================";

				LOG(LS_VERBOSE) << "Number of streams found in this file: " << pFormatCtx->nb_streams;
				LOG(LS_VERBOSE) << "Audio stream duration: " << pFormatCtx->duration;

				for(i=0; i<pFormatCtx->nb_streams; i++)
				{
					if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
					{
						targetAudioStreamIdx=i;
						break;
					}
				}

				if(targetAudioStreamIdx==-1)
				{
					LOG(LS_VERBOSE) << "Didn't find a audio stream.";
					return false; // Didn't find a video stream
				}

				pCodecCtx = pFormatCtx->streams[targetAudioStreamIdx]->codec;

				int dur = (int)(pFormatCtx->duration / (1000 * 1000));

				mediaItem->SetSampleRate(pCodecCtx->sample_rate);
				mediaItem->SetBitRate(pCodecCtx->bit_rate);
				mediaItem->SetNChannels(pCodecCtx->channels);

				mediaItem->SetDuration(dur);

				LOG(LS_VERBOSE) << "Sample rate = " << pCodecCtx->sample_rate
						<< ", bitrate = " << pCodecCtx->bit_rate
						<< ", channels = " << pCodecCtx->channels
						<< ", duration = " << dur << " seconds";

				av_close_input_file(pFormatCtx);

				if(dur < pVueceGlobalSetting->iMediaStreamFileMinDuration)
				{
					LOG(LS_VERBOSE) << "Duration is too short, will be filtered away, min duration: "
							<< pVueceGlobalSetting->iMediaStreamFileMinDuration;
					return false;
				}

				return true;

			}


		//recursively iterate all shared folders
 void VueceWinUtilities::ScanAndBuildMediaItemList(
				 const std::string &absRootFolderPathUtf8,
				 VueceMediaItemList* iMediaItemList,
				 gcroot<VueceProgressUI^> ui,
				 struct VueceGlobalSetting *pVueceGlobalSetting,
				int* pNumSongs,
				int* pNumDirs)
			{
				talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator( talk_base::Filesystem::IterateDirectory());
				std::string parent_uri = talk_base::MD5(absRootFolderPathUtf8);
				bool bBrowseRoot = false;

				if(absRootFolderPathUtf8 == "")
				{
					bBrowseRoot = true;
				}

				LOG(LS_VERBOSE) << "MEDIA SCAN - Target folder is: " << absRootFolderPathUtf8;

				if(bBrowseRoot)
				{
					std::list<std::string>::iterator it = pVueceGlobalSetting->iPublicFolderList->begin();

					LOG(LS_VERBOSE) << "MEDIA SCAN - Browse root started.";

					while (it != pVueceGlobalSetting->iPublicFolderList->end())
					{
						VueceMediaItem* mediaItem = new VueceMediaItem();
						int dir_count=0;
						int file_count=0;

						std::string folderPath = *it;
						std::string fp_utf8 = VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws(folderPath));

						std::string uri = talk_base::MD5(fp_utf8);

						LOG(LS_VERBOSE) << "MEDIA SCAN - Browsing top level folder: " << fp_utf8;
						LOG(LS_VERBOSE) << "MEDIA SCAN - md5 key: " << uri;

						if(ui)
						{
							std::string displayMsg("Scanning ");
							displayMsg.append(fp_utf8);
							ui->OnMessage(VueceProgressUI::MSG_HINT_SCANNING, fp_utf8);
						}

						talk_base::Pathname folder_path(fp_utf8);

						LOG(LS_VERBOSE) << "MEDIA SCAN - Folder name is: "
								<< folder_path.folder_name() << ", file name: " << folder_path.filename();

						mediaItem->SetValid(true);
						mediaItem->SetFolder(true);
						mediaItem->SetName(folder_path.filename()); //UTF8 encoded for Android to display
						mediaItem->SetUriUtf8(uri);
						mediaItem->SetParentUriUtf8(parent_uri);

						mediaItem->SetPath( talk_base::Base64::Encode(fp_utf8) );

						talk_base::Pathname f;
						f.AppendFolder(fp_utf8);

						VueceWinUtilities::CountSongsInFolder(f,&dir_count,&file_count);

						mediaItem->SetNumDirs(dir_count);
						mediaItem->SetNumSongs(file_count);

						iMediaItemList->push_back(mediaItem);

						*pNumDirs++;

						LOG(LS_VERBOSE) << "MEDIA SCAN - Start scanning folder: " << folderPath;
						ScanAndBuildMediaItemList(fp_utf8, iMediaItemList, ui, pVueceGlobalSetting, pNumSongs, pNumDirs);

						++it;
					}

					LOG(LS_VERBOSE) << "MEDIA SCAN -  Browse root done";

					return;
				}

				if(ui)
					ui->OnMessage(VueceProgressUI::MSG_HINT_SCANNING, absRootFolderPathUtf8);

				LOG(LS_VERBOSE) << "MEDIA SCAN - Start browsing sub folder:  " << absRootFolderPathUtf8;

				talk_base::Pathname folder_path;
				folder_path.AppendFolder( absRootFolderPathUtf8 );

				if (!directoryIterator->Iterate(folder_path))
				{
					LOG(LS_ERROR) << "MEDIA SCAN - :Cannot access this folder! It does not exit or not valid";
					return;
				}

				do {
						std::string fname_utf8 = directoryIterator->Name();  //utf8 encoded string
						LOG(LS_VERBOSE) << "filename:" << fname_utf8;

						std::wstring widestr = VueceWinUtilities::utf8_decode(fname_utf8);  // utf8 decoded wstring
						LOG(LS_VERBOSE) << "widestr:" << widestr;

						std::string fname_non_utf8 = VueceWinUtilities::ws2s(widestr);  // system default windows ansi code page

						LOG(LS_VERBOSE) << "Found a file: " << fname_utf8;

						if (!directoryIterator->IsDots())
						{
							if(directoryIterator->IsDirectory())
							{
								LOG(LS_VERBOSE) << "This is a folder: " << fname_utf8 << ", counting songs.";
								int numSongs = 0;
								int numDirs = 0;

								talk_base::Pathname sub_folder_path(folder_path);
								sub_folder_path.AppendFolder(fname_utf8);

								VueceWinUtilities::CountSongsInFolder(sub_folder_path,&numDirs,&numSongs);

								if(numSongs > 0||numDirs > 0)
								{
										std::ostringstream os;
										VueceMediaItem* mediaItem = new VueceMediaItem();
										mediaItem->SetValid(true);
										mediaItem->SetFolder(true);
										mediaItem->SetName(fname_utf8); //UTF8 encoded for Android to display

										LOG(LS_VERBOSE) << "MEDIA SCAN - This sub folder has " << numSongs << " songs.";

										mediaItem->SetNumSongs( numSongs );
										mediaItem->SetNumDirs( numDirs );

										//we are not browsing root folder here, so the folder path should not be empty
										ASSERT(absRootFolderPathUtf8.length() > 0);

										//process file path
										os << absRootFolderPathUtf8 << folder_path.folder_delimiter() << fname_utf8;

										std::string fpath = os.str();
										std::string itemID = talk_base::MD5(fpath);

										LOG(LS_VERBOSE) << "MEDIA SCAN - Adding sub folder to list: " << fpath;
										LOG(LS_VERBOSE) << "MEDIA SCAN - md5 key: " << itemID;

										mediaItem->SetUriUtf8( itemID );
										mediaItem->SetParentUriUtf8(parent_uri);

										mediaItem->SetPath( talk_base::Base64::Encode(fpath) );

										iMediaItemList->push_back(mediaItem);

										*pNumDirs++;

										ScanAndBuildMediaItemList(fpath, iMediaItemList, ui, pVueceGlobalSetting, pNumSongs, pNumDirs);
								}
								else
								{
									LOG(LS_VERBOSE) << "MEDIA SCAN - This sub folder has no songs.";
								}

							}
							else
							{

								VueceMediaItem* mediaItem = VueceWinUtilities::AnalyzeFile(
										absRootFolderPathUtf8, fname_non_utf8, fname_utf8, pVueceGlobalSetting);
								if(mediaItem!=NULL && mediaItem->IsValid())
								{
									LOG(LS_VERBOSE) << "Got a valid item";

									if(ui)
											ui->OnMessage(VueceProgressUI::MSG_HINT_SCANNING, fname_non_utf8);

									mediaItem->SetParentUriUtf8(parent_uri) ;

									iMediaItemList->push_back(mediaItem);

									*pNumSongs++;
								}
							}

						}
						else
						{
							LOG(LS_VERBOSE) << "MEDIA SCAN - This is not a file or directory, skip.";
						}

					}
					while (directoryIterator->Next());
			}

	 void VueceWinUtilities::EnableAutoStart()
	 {
			std::string appPath("");

			LOG(LS_VERBOSE) << ("VueceWinUtilities - EnableAutoStart");

			String^ path;

			path = System::Windows::Forms::Application::ExecutablePath;

			path += " auto";

			VueceWinUtilities::MarshalString(path,  appPath);

			LOG(LS_VERBOSE) << "VueceWinUtilities - EnableAutoStart, app path is: " << appPath;

			Registry::SetValue("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "Vuece", path);
	 }

	 VueceUserDataLoadResult VueceWinUtilities::LoadUserData2(char** result_buf, int* result_buf_len)
	 	 {
				RegistryKey^ rk;
				VueceCrypt crypt;
				int loaded_usedata_len;
				VueceUserDataLoadResult res = VueceUserDataLoadResult_General_Err;

				LOG_F(INFO) << "Start";

				rk  = Registry::CurrentUser->OpenSubKey("Software", true);
				if (!rk)
				{
					LOG_F(LS_VERBOSE) << ("Failed to open CurrentUser/Software key");
					return VueceUserDataLoadResult_Corrupted_Registry;
				}

				RegistryKey^ nk = rk->OpenSubKey("VueceHub");
				if (!nk)
				{
					LOG_F(LS_VERBOSE) << ("Didn't find sub key 'VueceHub', no data to load");
					return VueceUserDataLoadResult_Corrupted_Registry;
				}

				Object^ v = nk->GetValue("UserData");
				if (!v)
				{
					LOG_F(LS_VERBOSE) << ("'Vuece' doesn't have key 'UserData'");
					return VueceUserDataLoadResult_UserData_Missing;
				}

				array< Byte >^ byteArray = (array< Byte >^)v;

				loaded_usedata_len = byteArray->Length;

				LOG_F(LS_VERBOSE) << "Length of the loaded binary data: " << loaded_usedata_len;

				//allocate memory

				*result_buf = (char*)calloc(loaded_usedata_len+1, sizeof(char));

				if(*result_buf == NULL)
				{
					LOG_F(LS_ERROR) << "Failed to allocate memory to loaded user data, required size: " << loaded_usedata_len;
					return VueceUserDataLoadResult_Mem_Insufficient;
				}

				LOG_F(LS_VERBOSE) << "Mem allocated, size: " << loaded_usedata_len+1;

				*result_buf_len = loaded_usedata_len;

				pin_ptr<System::Byte> p = &byteArray[0];
				unsigned char* pby = p;
				char* pch = reinterpret_cast<char*>(pby);

				memcpy(*result_buf, pch, loaded_usedata_len);

				if( !crypt.DecryptStrFromBinary(*result_buf, loaded_usedata_len))
				{
					LOG_F(LS_ERROR) << "User data decyption failed returned an error";

					if(*result_buf != NULL)
						free(*result_buf);

					res = VueceUserDataLoadResult_Decrypt_Err;
				}
				else
				{
					res = VueceUserDataLoadResult_OK;

					LOG_F(LS_VERBOSE) << "Decrypted data length: " << loaded_usedata_len;;
					LOG_F(LS_VERBOSE) << "Decrypted data: " << *result_buf;
				}

				return res;
	 }

 	bool VueceWinUtilities::ParseUserData(const char* data, struct VueceGlobalSetting *pVueceGlobalSetting)
 	{
 		bool ret = FALSE;

 		tinyxml2::XMLDocument doc;

 		LOG_F(LS_VERBOSE) << "Start";

 		if(data == NULL)
 		{
 	 		LOG_F(LS_ERROR) << "Input is null, do nothing";
 	 		return FALSE;
 		}

 		LOG_F(LS_VERBOSE) << "ParseUserData, len: " << strlen(data);
 		LOG_F(LS_VERBOSE) << "ParseUserData, data: " << strlen(data);

 		tinyxml2::XMLError err = doc.Parse( data );

 		if(err != XML_SUCCESS)
 		{
 			LOG_F(LS_ERROR) << "FATAL ERROR - Cannot parse user data, code: " << err;
 			return FALSE;
 		}

 		//locate root element
 		XMLElement* rootElem = doc.FirstChildElement( "vuecepc" );
 		if(rootElem == 0){
 			LOG_F(LS_ERROR) << "Could not find root config element.";
 			return FALSE;
 		}

 		////////////////////////////////////////////
 		//locate user name/id
 		tinyxml2::XMLElement* userElem = rootElem->FirstChildElement( "username" );
 		if(userElem == 0){
 			LOG_F(LS_ERROR) << "Could not find userElem.";
 			return FALSE;
 		}

 		const char* username = userElem->GetText();
 		if(username != 0)
 		{
 			LOG_F(LS_VERBOSE) << "user name: " << username;
 			strcpy(pVueceGlobalSetting->accountUserName, username);
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "user name: NOT SET";
 		}

 		///////////////////////////////////////////////
 		//locate user refresh token
 		tinyxml2::XMLElement* refreshTokenElem = rootElem->FirstChildElement( "refresh_token" );
 		if(refreshTokenElem == 0)
 		{
 			LOG(WARNING) << "Could not find refresh_token element";
 		}
 		else
 		{
 	 		const char* pwd = refreshTokenElem->GetText();

 	 		if(pwd != 0)
 	 		{
 	 			LOG_F(LS_VERBOSE) << "refresh_token: " << pwd;
 	 			strcpy(pVueceGlobalSetting->accountRefreshToken, pwd);
 	 		}
 	 		else
 	 		{
 	 			LOG_F(LS_VERBOSE) << "refresh_token: NOT SET";
 	 		}
 		}

 		///////////////////////////////////////
 		//display name, optinal
 		tinyxml2::XMLElement* dspNameElem = rootElem->FirstChildElement( "dspname" );
 		if(dspNameElem == 0)
 		{
 			LOG(LS_ERROR) << "Could not find dspNameElem.";
 			return FALSE;
 		}
 		else
 		{
 	 		const char* dspname = dspNameElem->GetText();

 	 		if(dspname != 0)
 	 		{
 	 			LOG_F(LS_VERBOSE) << "hubname: " << dspname;
 	 			strcpy(pVueceGlobalSetting->accountDisplayName, dspname);
 	 		}
 	 		else
 	 		{
 	 			LOG_F(LS_VERBOSE) << "dsplay name: NOT SET";
 	 		}
 		}

 		//////////////////////////////////
 		//image url, optinal
 		tinyxml2::XMLElement* imgUrlElem = rootElem->FirstChildElement( "imgurl" );
 		if(imgUrlElem == 0)
 		{
 			LOG(LS_ERROR) << "Could not find imgUrlElem.";
 			return FALSE;
 		}
 		else
 		{
 	 		const char* imgurl = imgUrlElem->GetText();

 	 		if(imgurl != 0)
 	 		{
 	 			LOG_F(LS_VERBOSE) << "imgurl: " << imgurl;
 	 			strcpy(pVueceGlobalSetting->accountImgUrl, imgurl);
 	 		}
 	 		else
 	 		{
 	 			LOG_F(LS_VERBOSE) << "imgurl: NOT SET";
 	 		}
 		}

 		/////////////////////////////////
 		//parse hub name
 		tinyxml2::XMLElement* hubNameElem = rootElem->FirstChildElement( "hubname" );
 		if(hubNameElem == 0)
 		{
 			LOG(LS_ERROR) << "Could not find hubNameElem.";
 			return FALSE;
 		}

 		const char* hubname = hubNameElem->GetText();
 		if(hubname != 0)
 		{
 			LOG_F(LS_VERBOSE) << "hubname: " << hubname;
 			strcpy(pVueceGlobalSetting->hubName, hubname);
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "hubname: NOT SET";
 		}

 		/////////////////////////////////////
 		//parse hub id
 		tinyxml2::XMLElement* hubIdElem = rootElem->FirstChildElement( "hubid" );
 		if(hubIdElem == 0){
 			LOG(LS_ERROR) << "Could not find hubIdElem.";
 			return FALSE;
 		}

 		const char* hubId = hubIdElem->GetText();

 		if(hubId != 0)
 		{
 			LOG_F(LS_VERBOSE) << "hubId: " << hubId;
 			strcpy(pVueceGlobalSetting->hubID, hubId);
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "hubid: NOT SET";
 		}

 		//////////////////////////////////////////////
 		//parse rescan flag
 		tinyxml2::XMLElement* rescanElem = rootElem->FirstChildElement( "rescanneeded" );
 		if(rescanElem == 0){
 			LOG(LS_ERROR) << "Could not find rescanElem.";
 			return FALSE;
 		}

 		const char* rescan = rescanElem->GetText();

 		if(rescan != 0)
 		{
 			LOG_F(LS_VERBOSE) << "Rescan needed: " << rescan;
 			pVueceGlobalSetting->iRescanNeeded = atoi(rescan);
 			//validate
 			if(pVueceGlobalSetting->iRescanNeeded != 0 && pVueceGlobalSetting->iRescanNeeded != 1){
 				LOG(LS_ERROR) << "Corrupted rescan flag: " << pVueceGlobalSetting->iRescanNeeded;
 				pVueceGlobalSetting->iRescanNeeded = 1;
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "user name: NOT SET";
 			pVueceGlobalSetting->iRescanNeeded = 1;
 		}

 		//////////////////////////////////////////
 		//parse sys auto login flag
 		tinyxml2::XMLElement* autoElem = rootElem->FirstChildElement( "autologin_sys" );
 		if(autoElem == 0){
 			LOG(LS_ERROR) << "Could not find autoElem.";
 			return FALSE;
 		}

 		const char* autologin = autoElem->GetText();

 		if(autologin != 0)
 		{
 			LOG_F(LS_VERBOSE) << "autologin: " << autologin;
 			pVueceGlobalSetting->iAutoLoginAtSysStartup = atoi(autologin);
 			//validate
 			if(pVueceGlobalSetting->iAutoLoginAtSysStartup != 0 && pVueceGlobalSetting->iAutoLoginAtSysStartup != 1){
 				LOG(LS_ERROR) << "Corrupted autologin_sys flag: " << pVueceGlobalSetting->iAutoLoginAtSysStartup;
 				pVueceGlobalSetting->iAutoLoginAtSysStartup = 0;
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "auto login: NOT SET";
 			pVueceGlobalSetting->iAutoLoginAtSysStartup = 0;
 		}

 		//////////////////////////
 		//parse app auto login
 		tinyxml2::XMLElement* autoLogInAppElem = rootElem->FirstChildElement( "autologin_app" );
 		if(autoLogInAppElem == 0){
 			LOG(LS_ERROR) << "Could not find autoLogInAppElem.";
 			return FALSE;
 		}

 		const char* autologinApp = autoLogInAppElem->GetText();

 		if(autologinApp != 0)
 		{
 			LOG_F(LS_VERBOSE) << "autologinApp: " << autologinApp;
 			pVueceGlobalSetting->iAutoLoginAtAppStartup = atoi(autologinApp);
 			//validate
 			if(pVueceGlobalSetting->iAutoLoginAtAppStartup != 0 && pVueceGlobalSetting->iAutoLoginAtAppStartup != 1){
 				LOG(LS_ERROR) << "Corrupted autologin_app flag: " << pVueceGlobalSetting->iAutoLoginAtAppStartup;
 				pVueceGlobalSetting->iAutoLoginAtAppStartup = 0;
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "autologin_app: NOT SET";
 			pVueceGlobalSetting->iAutoLoginAtAppStartup = 0;
 		}

 		///////////////////////////////////////////////////
 		//iAllowFriendAccess
 		tinyxml2::XMLElement* allowFAcElem = rootElem->FirstChildElement( USER_DATA_TAG_ALLOW_FRIEND_ACCESS);
 		if(allowFAcElem == 0)
 		{
 			LOG(LS_ERROR) << "Could not find allowFAcElem.";
 			return FALSE;
 		}

 		const char* allowFAc = allowFAcElem->GetText();

 		if(allowFAc != 0)
 		{
 			LOG_F(LS_VERBOSE) << "allowFAc: " << allowFAc;
 			pVueceGlobalSetting->iAllowFriendAccess = atoi(allowFAc);
 			//validate
 			if(pVueceGlobalSetting->iAllowFriendAccess != 0 && pVueceGlobalSetting->iAllowFriendAccess != 1){
 				LOG(LS_ERROR) << "Corrupted allow_friend_ac flag: " << pVueceGlobalSetting->iAllowFriendAccess;
 				pVueceGlobalSetting->iAllowFriendAccess = 0;
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "allow_friend_ac: NOT SET";
 			pVueceGlobalSetting->iAllowFriendAccess = 0;
 		}

 		///////////////////////////////////////////////////
 		//iMaxConcurrentStreaming
 		tinyxml2::XMLElement* maxConStreamElem = rootElem->FirstChildElement( USER_DATA_TAG_MAX_CONCURRENT_STREAMING );
 		if(maxConStreamElem == 0)
 		{
 			LOG_F(LS_ERROR) << "Could not find maxConStreamElem.";
 			return FALSE;
 		}

 		const char* maxCs = maxConStreamElem->GetText();

 		if(maxCs != 0)
 		{
 			LOG_F(LS_VERBOSE) << "maxCs: " << maxCs;
 			pVueceGlobalSetting->iMaxConcurrentStreaming = atoi(maxCs);

 			//validate
 			if(pVueceGlobalSetting->iMaxConcurrentStreaming <= 0
 					|| pVueceGlobalSetting->iAllowFriendAccess > VUECE_MAX_CONCURRENT_STREAMING){
 				LOG_F(LS_ERROR) << "Corrupted allow_friend_ac flag: " << pVueceGlobalSetting->iMaxConcurrentStreaming;
 				pVueceGlobalSetting->iMaxConcurrentStreaming = 1;
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "max_concurrent_str: NOT SET";
 			pVueceGlobalSetting->iMaxConcurrentStreaming = 1;
 		}

 		pVueceGlobalSetting->iPublicFolderList->clear();

 		///////////////////////////////////
 		//public folder
 		tinyxml2::XMLElement* pfElem = rootElem->FirstChildElement( USER_DATA_TAG_PUBLIC_FOLDERS );
 		if(pfElem != 0)
 		{
 			const char* publicfolders = pfElem->GetText();

 			if(publicfolders != 0)
 			{
 				char* tmpbuf_ptr;
 				char tmpBuf2[VUECE_MAX_FILE_PATH+1];
 				char * pch;

 				memset(tmpBuf2, '\0', sizeof(tmpBuf2));

 				tmpbuf_ptr = (char* )calloc(strlen(publicfolders) + 1, sizeof(char));

 				if(tmpbuf_ptr == NULL)
 				{
 					LOG_F(LS_ERROR) << "Failed to allocate memory, required size: " << strlen(publicfolders);
 					return FALSE;
 				}

 				strcpy(tmpbuf_ptr, publicfolders);

 				//parse public folder list string and add folders to pVueceGlobalSetting
 				LOG_F(LS_VERBOSE) << "publicfolders: " << tmpbuf_ptr;

 				pch = strtok (tmpbuf_ptr, VUECE_FOLDER_SEPARATOR_STRING);
 				while (pch != NULL)
 				{
 					strcpy(tmpBuf2, pch);

 					LOG_F(LS_VERBOSE) << "folder extracted: " << tmpBuf2;

 					if(strlen(tmpBuf2) > 0)
 					{
 						std::string f (tmpBuf2);
 						pVueceGlobalSetting->iPublicFolderList->push_back(f);
 					}

 					pch = strtok (NULL, VUECE_FOLDER_SEPARATOR_STRING);
 				}

 				LOG_F(LS_VERBOSE) << "parsing public folder finished, list size: " << pVueceGlobalSetting->iPublicFolderList->size();

 				if(tmpbuf_ptr != NULL)
 				{
 					free(tmpbuf_ptr);
 				}
 			}
 			else
 			{
 				LOG_F(LS_VERBOSE) << "publicfolder: NOT SET";
 			}
 		}
 		else
 		{
 			LOG_F(LS_VERBOSE) << "publicfolder element not found";
 			return FALSE;
 		}

 		return TRUE;
 	}

	bool VueceWinUtilities::CompareVersion(const char* currentVersion, const char* latestVersion)
	{
		int current_major;
		int current_minor;
		int current_build;
		int current_revision;
		int latest_major;
		int latest_minor;
		int latest_build;
		int latest_revision;
		char * pch;
		int counter;
		bool hasError = false;
		//use this flag to disable/enable revision number compare
		bool disable_revision_compare = true;

		char tmpBuf[64];

		LOG_F(LS_VERBOSE) << ":CompareVersion, parsing current version: " << currentVersion;

		strcpy(tmpBuf, currentVersion);

		pch = strtok (tmpBuf, ".");

		counter = 0;

		while (pch != NULL)
		{
			switch (counter)
			{
			case 0:
				current_major = atoi(pch);
				break;
			case 1:
				current_minor = atoi(pch);
				break;
			case 2:
				current_build = atoi(pch);
				break;
			case 3:
				current_revision = atoi(pch);
				break;
			default:
				hasError = true;
				break;
			}

			counter++;

			pch = strtok (NULL, ".");
		}

		counter--;

		if(counter == 3 && !hasError)
		{
			LOG_F(LS_VERBOSE) << "Current version successfully parsed - major: " << current_major << ", minor: " << current_minor << ", build: " << current_build << ", revision: " << current_revision;
		}
		else
		{
			LOG_F(LS_ERROR) << "Parsing current version failed.";
			return false;
		}


		LOG_F(LS_VERBOSE) << "CompareVersion, parsing latest version: " << latestVersion;

		strcpy(tmpBuf, latestVersion);

		pch = strtok (tmpBuf, ".");

		counter = 0;

		while (pch != NULL)
		{
			switch (counter)
			{
			case 0:
				latest_major = atoi(pch);
				break;
			case 1:
				latest_minor = atoi(pch);
				break;
			case 2:
				latest_build = atoi(pch);
				break;
			case 3:
				latest_revision = atoi(pch);
				break;
			default:
				hasError = true;
				break;
			}

			counter++;

			pch = strtok (NULL, ".");
		}

		counter--;

		if(counter == 3 && !hasError)
		{
			LOG_F(LS_VERBOSE) << "Latest version successfully parsed - major: " << latest_major << ", minor: " << latest_minor << ", build: " << latest_build << ", revision: " << latest_revision;
		}
		else
		{
			LOG_F(LS_ERROR) << "Parsing latest version failed.";
			return false;
		}

		if(latest_major > current_major)
		{
			LOG_F(LS_VERBOSE) << "latest_major > current_major";
			return true;
		}
		else if(latest_major == current_major)
		{
			if(latest_minor > current_minor)
			{
				LOG_F(LS_VERBOSE) << "latest_minor > current_minor";
				return true;
			}
			else if(latest_minor == current_minor)
			{
				if(latest_build > current_build)
				{
					LOG_F(LS_VERBOSE) << "latest_build > current_build";
					return true;
				}
				else if(latest_build == current_build)
				{
						if(disable_revision_compare)
						{
								if(latest_revision > current_revision)
								{
									LOG_F(LS_VERBOSE) << "latest_revision > current_revision";
									return true;
								}
								else
								{
									return false;
								}
						}
						else
						{
							LOG_F(LS_VERBOSE) << "revision compare is disabled";
							return false;
						}

				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		return false;
	}

	void VueceWinUtilities::SaveUserDataToRegistry(char* data, int data_len){
		RegistryKey^ rk;
		array< Byte >^ byteArray = gcnew array< Byte >(data_len);

		rk  = Registry::CurrentUser->OpenSubKey("Software", true);
		if (!rk)
		{
			LOG(LS_ERROR) << ("SaveUserDataToRegistry - Failed to open CurrentUser/Software key");
			return;
		}

		//Creates a new subkey or opens an existing subkey for write access.
		RegistryKey^ nk = rk->CreateSubKey("VueceHub");
		if (!nk)
		{
			LOG(LS_ERROR) << ("Failed to create 'VueceHub'");
			return ;
		}

		// convert native pointer to System::IntPtr with C-Style cast
		Marshal::Copy((IntPtr)data, byteArray, 0, data_len);

		//Sets the value of a name/value pair in the registry key, using the specified registry data type.
		nk->SetValue("UserData", byteArray, RegistryValueKind::Binary );

		return;
	}


	int VueceWinUtilities::GenerateUserSettingString2(char** result_buf_ptr, int* result_buf_size, struct VueceGlobalSetting *pVueceGlobalSetting)
		{
				int result_len = -1;
				std::ostringstream os;
				tinyxml2::XMLDocument doc;
				tinyxml2::XMLPrinter printer;
				std::ostringstream os_folder_list;

				LOG_F(LS_VERBOSE) << "- Start";

				//construct folder list string
				std::list<std::string>::iterator it = pVueceGlobalSetting->iPublicFolderList->begin();
				while (it != pVueceGlobalSetting->iPublicFolderList->end())
				{
					std::string fp = *it;
					os_folder_list << fp << VUECE_FOLDER_SEPARATOR_STRING;
					++it;
				}

				os << "<vuecepc><username>" << pVueceGlobalSetting->accountUserName << "</username>"
						<< "<refresh_token>" << pVueceGlobalSetting->accountRefreshToken << "</refresh_token>"
					<< "<hubname>" << pVueceGlobalSetting->hubName<< "</hubname>"
					<< "<dspname>" << pVueceGlobalSetting->accountDisplayName<< "</dspname>"
					<< "<imgurl>" << pVueceGlobalSetting->accountImgUrl<< "</imgurl>"
					<< "<hubid>" << pVueceGlobalSetting->hubID<< "</hubid>"
					<< "<rescanneeded>" << pVueceGlobalSetting->iRescanNeeded << "</rescanneeded>"
					<< "<autologin_sys>" << pVueceGlobalSetting->iAutoLoginAtSysStartup << "</autologin_sys>"
					<< "<autologin_app>" << pVueceGlobalSetting->iAutoLoginAtAppStartup << "</autologin_app>"
					<< "<" << USER_DATA_TAG_ALLOW_FRIEND_ACCESS << ">" << pVueceGlobalSetting->iAllowFriendAccess << "</" << USER_DATA_TAG_ALLOW_FRIEND_ACCESS <<">"
					<< "<max_concurrent_str>" << pVueceGlobalSetting->iMaxConcurrentStreaming << "</max_concurrent_str>"
					<< "<publicfolders>" << os_folder_list.str() << "</publicfolders></vuecepc>";

				LOG_F(LS_VERBOSE) << "Final string: " << os.str();

				doc.Parse( os.str().c_str() );
				doc.Print( &printer );

				result_len = strlen(printer.CStr()) ;

				*result_buf_size = result_len + 1;

				*result_buf_ptr =  (char*)calloc(result_len + 1, sizeof(char));

				LOG_F(LS_VERBOSE) << "final user data length:" << result_len;

				if(*result_buf_ptr == NULL)
				{
					LOG_F(LS_VERBOSE) << "Failed to allocate memory, required size: " << *result_buf_size;
					return result_len;
				}

				strcpy(*result_buf_ptr, printer.CStr());

				return result_len;
	}

    bool VueceWinUtilities::QueryUpdate(char* latestVersion, char* updateUrl)
	{
    	LOG_F(LS_VERBOSE) << "QueryUpdate";

		BOOL bResults = FALSE;
		BOOL bHasErr = FALSE;
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		HINTERNET hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
		char finalRespBuf[1024];
		char tmpBuf[512];

		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUpdate";

		memset(finalRespBuf, 0, sizeof(finalRespBuf));
		memset(tmpBuf, 0, sizeof(tmpBuf));

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen( L"WinHTTP Example/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0 );

		// Specify an HTTP server.
		if( hSession )
		{
//			hConnect = WinHttpConnect( hSession, L"update.vuece.com",
//					INTERNET_DEFAULT_HTTP_PORT, 0 );

			wchar_t w_requrl[sizeof(tmpBuf)*2];

			strcpy(tmpBuf, VUECE_UPDATE_SERVER_URL);

			ConvertCharArrayToLPCWSTR(tmpBuf, w_requrl, sizeof(w_requrl));

			hConnect = WinHttpConnect( hSession, w_requrl,
					VUECE_UPDATE_SERVER_PORT, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - session cannot be created.";
			bHasErr = TRUE;
		}

		// Create an HTTP request handle.
		if( hConnect )
		{
			wchar_t w_requrl[sizeof(tmpBuf)*2];

			memset(tmpBuf, 0, sizeof(tmpBuf));

			strcpy(tmpBuf, VUECE_UPDATE_VERSION_INFO_LOCATION);

			ConvertCharArrayToLPCWSTR(tmpBuf, w_requrl, sizeof(w_requrl));

//			hRequest = WinHttpOpenRequest( hConnect, L"GET", L"/vuece/hub/webupdate.txt",
//					NULL, WINHTTP_NO_REFERER,
//					WINHTTP_DEFAULT_ACCEPT_TYPES,
//					0 );

			hRequest = WinHttpOpenRequest( hConnect, L"GET", w_requrl,
					NULL, WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Connection to host cannot be opened.";
			bHasErr = TRUE;
		}

		// Send a request.
		if( hRequest )
		{
			bResults = WinHttpSendRequest( hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0,
					0, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Request cannot be opened";
			bHasErr = TRUE;
		}

		if (bResults)
		{
			bResults = WinHttpReceiveResponse( hRequest, NULL);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Error when receiving response.";
			bHasErr = TRUE;
		}

		// Keep checking for data until there is nothing left.
		if( bResults )
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if( !WinHttpQueryDataAvailable( hRequest, &dwSize ) )
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - An error occurred when waiting for response data, code = " << GetLastError();
					bHasErr = TRUE;
				}

				// Allocate space for the buffer.
				if(dwSize > sizeof(tmpBuf))
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - response is too long - " << dwSize;
					dwSize=0;
					bHasErr = TRUE;
				}
				else
				{
					// Read the data.
					ZeroMemory( tmpBuf, dwSize+1 );

					if( !WinHttpReadData( hRequest, (LPVOID)tmpBuf,
									dwSize, &dwDownloaded ) )
					{
						LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - An error occurred when reading response data, code = " << GetLastError( );
						bHasErr = TRUE;
					}
					else
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUpdate - Response is:\n " << tmpBuf;

						if(strlen(tmpBuf) > 0)
						{
							strcat(finalRespBuf, tmpBuf);
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUpdate - Current final response is: \n" << finalRespBuf;
						}
					}

					// Free the memory allocated to the buffer.
					//										delete [] finalRespBuf;
				}

			}while( dwSize > 0 );

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate  - Request cannot be sent.";
		}

		// Report any errors.
		if( !bResults )
		{
//		    	  printf( "Error %d has occurred.\n", GetLastError( ) );
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Request failed, code =  " << GetLastError( );
		}
		else
		{

			if ( !bHasErr )
			{
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUpdate - A complete response has been successfully received: " << finalRespBuf;

							//expected format:
							//{'version': '0.0.0.1', 'url':'http://update.vuece.com/v0.0.0.1'}

							JSONNODE *node = json_parse(finalRespBuf);

							if(node == NULL)
							{
								LOG(LS_ERROR) << ("VueceWinUtilities::QueryUpdate - Invalid JSON Node");
							}
							else
							{
								JSONNODE_ITERATOR i = json_begin(node);

								//We don't accept array
								if(i == json_end(node)) {
									LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Msg end reached, not expected!";
								}

								if (*i == NULL) {
									LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Msg iterator is NULL, not expected!";
								}

								json_char *node_version= json_name(*i);

								if (strcmp(node_version, "version") != 0) {
									LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - The first node is not version, not expected!";
								}

								json_free(node_version);

								json_char *node_value_version = json_as_string(*i);
								LOG(LS_VERBOSE) << "version: " << node_value_version;

								strcpy(latestVersion, node_value_version);

								json_free(node_value_version);

								i++;
								json_char* node_url = json_name(*i);
								if (strcmp(node_url, "url") != 0)
								{
									LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - The second node is not url, not expected!";
								}

								json_free(node_url);

								json_char *node_value_url = json_as_string(*i);

								LOG(LS_VERBOSE) << "url: " << node_value_url;

								strcpy(updateUrl, node_value_url);

								json_free(node_value_url);
							}

							json_delete(node);
						}
			}

		// Close any open handles.
		if( hRequest ) WinHttpCloseHandle( hRequest );
		if( hConnect ) WinHttpCloseHandle( hConnect );
		if( hSession ) WinHttpCloseHandle( hSession );

		return bResults;
	}

	bool VueceWinUtilities::QueryAccessToken (bool refresh, const char* code, char* result_access_tok, char* result_refresh_tok)
	{
		BOOL bResults = FALSE;
		BOOL bHasErr = FALSE;
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		HINTERNET hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
		char finalRespBuf[1024 * 10];
		char tmpBuf[1024];
		int data_len = 0;

		char access_tok[512+1];
		char tok_type[32+1];
		char exp[32+1];
		char refresh_tok[512+1];
		char err_msg[512+1];

		//example link: https://developers.google.com/accounts/docs/OAuth2InstalledApp
		/*
		 * POST  /oauth2/v3/token HTTP/1.1
			Host: www.googleapis.com
			Content-Type: application/x-www-form-urlencoded

			code=4/v6xr77ewYqhvHSyW6UJ1w7jKwAzu&
			client_id=8819981768.apps.googleusercontent.com&
			client_secret=your_client_secret&
			redirect_uri=https://oauth2-login-demo.appspot.com/code&
			grant_type=authorization_code
		 */
		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Start ";

		memset(finalRespBuf, 0, sizeof(finalRespBuf));
		memset(access_tok, 0, sizeof(access_tok));
		memset(tok_type, 0, sizeof(tok_type));
		memset(exp, 0, sizeof(exp));
		memset(refresh_tok, 0, sizeof(refresh_tok));
		memset(err_msg, 0, sizeof(err_msg));

		//construct the data to post

		if(!refresh)
		{
			sprintf(tmpBuf, "code=%s&", code);
			strcat(tmpBuf, "grant_type=authorization_code&");
			strcat(tmpBuf, "redirect_uri=urn:ietf:wg:oauth:2.0:oob&");
		}
		else
		{
			sprintf(tmpBuf, "refresh_token=%s&", code);
			strcat(tmpBuf, "grant_type=refresh_token&");
		}

		strcat(tmpBuf, "client_id=489257891728-rftvnu18uv50q61qjrq5quu3b1p9s54s.apps.googleusercontent.com&");
		strcat(tmpBuf, "client_secret=qlR-6dG52vev818d6AuHH98v");

		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUpdate - Data to post: " << tmpBuf;

		data_len = strlen(tmpBuf);

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen( L"VueceHub Win/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0 );

		// Specify an HTTP server.
		if( hSession )
		{
			hConnect = WinHttpConnect( hSession, L"www.googleapis.com",
					INTERNET_DEFAULT_HTTPS_PORT, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - session cannot be created.";
			bHasErr = TRUE;
		}

		// Create an HTTP request handle.
		if( hConnect )
		{
			hRequest = WinHttpOpenRequest( hConnect, L"POST", L"/oauth2/v3/token",
					NULL, WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - Connection to host cannot be opened.";
			bHasErr = TRUE;
		}

		// Send a request.
		if( hRequest )
		{

			WinHttpAddRequestHeaders(
			    hRequest,
			    L"Content-Type: application/x-www-form-urlencoded",
			    -1L,
			    WINHTTP_ADDREQ_FLAG_ADD
			);

			bResults = WinHttpSendRequest( hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)tmpBuf, data_len, data_len, 0);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - Request cannot be opened";
			bHasErr = TRUE;
		}

		if (bResults)
		{
			bResults = WinHttpReceiveResponse( hRequest, NULL);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - Error when receiving response.";
			bHasErr = TRUE;
		}

		// Keep checking for data until there is nothing left.
		if( bResults )
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if( !WinHttpQueryDataAvailable( hRequest, &dwSize ) )
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - An error occurred when waiting for response data, code = " << GetLastError();
					bHasErr = TRUE;
				}

				// Allocate space for the buffer.
				if(dwSize > sizeof(tmpBuf))
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - response is too long - " << dwSize;
					dwSize=0;
					bHasErr = TRUE;
				}
				else
				{
					// Read the data.
					ZeroMemory( tmpBuf, dwSize+1 );

					if( !WinHttpReadData( hRequest, (LPVOID)tmpBuf,
									dwSize, &dwDownloaded ) )
					{
						LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - An error occurred when reading response data, code = " << GetLastError( );
						bHasErr = TRUE;
					}
					else
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Response is:\n " << tmpBuf;

						if(strlen(tmpBuf) > 0)
						{
							strcat(finalRespBuf, tmpBuf);
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Current final response is: \n" << finalRespBuf;
						}
					}

					// Free the memory allocated to the buffer.
					//										delete [] finalRespBuf;
				}

			}while( dwSize > 0 );

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken  - Request cannot be sent.";
		}

		// Report any errors.
		if( !bResults )
		{
//		    	  printf( "Error %d has occurred.\n", GetLastError( ) );
			LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - Request failed, code =  " << GetLastError( );
		}
		else
		{

			if ( !bHasErr )
			{
					LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - A complete response has been successfully received: " << finalRespBuf;

					/**
					 * Example response - Ok
					 *  {
							"access_token": "ya29.QAFDHGz23X9OOgjur-XwUWFk46_WGLMZ_D4SQBvAxs_NDh1IDpG_Wg_jfjI81RlFwb0f-459U0Q6sw",
							"token_type": "Bearer",
							"expires_in": 3600,
							"refresh_token": "1/dOuI_0ernmUJDpCR0uUgJhNcldQkcyAIKYb26r18wnI"
							}

						Example response - Token revoked
						{
						 	 "error": "invalid_grant",
						 	 "error_description": "Token has been revoked."
						}
					 */

					ExtracInfoFromOauth2PostResp (finalRespBuf,
								 access_tok,  tok_type,  exp,  refresh_tok, err_msg);

					LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - err_msg = " << err_msg;

					if(strlen(access_tok) > 0)
					{
							strcpy(result_access_tok, access_tok);

							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - access_tok = " << access_tok;
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - tok_type = " << tok_type;
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - exp = " << exp;
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - refresh_tok = " << refresh_tok;
					}
					else
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Token request has failed. ";
					}


					if(strlen(refresh_tok) > 0)
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - refresh token is not empty, will be updated.";
						if(result_refresh_tok)
						{
							strcpy(result_refresh_tok, refresh_tok);
						}
						else
						{
							LOG(LS_ERROR) << "VueceWinUtilities::QueryAccessToken - input result_refresh_tok is null, sth is wrong";
						}
					}

			}
		}

		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Closing connection handles ";

		// Close any open handles.
		if( hRequest ) WinHttpCloseHandle( hRequest );
		if( hConnect ) WinHttpCloseHandle( hConnect );
		if( hSession ) WinHttpCloseHandle( hSession );

		if(bResults)
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - OK";
		}
		else
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::QueryAccessToken - Failed";
		}

		return bResults;
	}

	bool VueceWinUtilities::ExtracInfoFromOauth2PostResp (const char* response,
			char* access_tok, char* tok_type, char* exp, char* refresh_tok, char* err_msg)
	{
		bool result = true;
		bool grant_failed = false;

		JSONNODE *node = json_parse(response);

		LOG(LS_VERBOSE) << "VueceWinUtilities::ExtracInfoFromOauth2PostResp - Start";

		if(node == NULL)
		{
			LOG(LS_ERROR) << ("VueceWinUtilities::ExtracInfoFromOauth2PostResp - Invalid JSON Node");
			return false;
		}

		JSONNODE_ITERATOR i = json_begin(node);
		//We don't accept array
		if(i == json_end(node)) {
			LOG(LS_ERROR) << "VueceWinUtilities::ExtracInfoFromOauth2PostResp - Msg end reached, not expected!";
			json_delete(node);
			return false;
		}

		/**
		 * Example response
		 *  {
				"access_token": "ya29.QAFDHGz23X9OOgjur-XwUWFk46_WGLMZ_D4SQBvAxs_NDh1IDpG_Wg_jfjI81RlFwb0f-459U0Q6sw",
				"token_type": "Bearer",
				"expires_in": 3600,
				"refresh_token": "1/dOuI_0ernmUJDpCR0uUgJhNcldQkcyAIKYb26r18wnI"
				}
		 */

		while (i != json_end(node))
		{

				if (*i == NULL || *i == JSON_NULL){
						LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Invalid JSON Node";
						return false;
				}

				char node_type = json_type(*i) ;

//				if (node_type != JSON_NODE){
//					LOG(LS_ERROR) << "VueceWinUtilities::QueryUpdate - Unexpected node type";
//					return false;
//				}

				json_char *node_name = json_name(*i);
//grant_failed

				if (strcmp(node_name, "access_token") == 0){
						json_char *node_value = json_as_string(*i);
						strcpy(access_tok, node_value);
						json_free(node_value);
				}
				else if (strcmp(node_name, "token_type") == 0){
					json_char *node_value = json_as_string(*i);
					strcpy(tok_type, node_value);
					json_free(node_value);
				}
				else if (strcmp(node_name, "expires_in") == 0){
						json_char *node_value = json_as_string(*i);
						strcpy(exp, node_value);
						json_free(node_value);
					}
				else if (strcmp(node_name, "refresh_token") == 0){
						json_char *node_value = json_as_string(*i);
						strcpy(refresh_tok, node_value);
						json_free(node_value);
					}
				else if (strcmp(node_name, "error") == 0){
						grant_failed = true;
						json_char *node_value = json_as_string(*i);
						strcpy(err_msg, node_value);
						json_free(node_value);
				}

				// cleanup and increment the iterator
				json_free(node_name);
				++i;
		}

		json_delete(node);

		LOG(LS_VERBOSE) << "VueceWinUtilities::ExtracInfoFromOauth2PostResp - Done";

		return result;
	}


	bool  VueceWinUtilities::QueryUserAccountInfo( const char* access_token, char* user_id, char* display_name, char* img_url)
	{
		BOOL bResults = FALSE;
		BOOL bHasErr = FALSE;
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		HINTERNET hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
		char finalRespBuf[1024+1];
		char tmpBuf[1024+1];
		int data_len = 0;

		char err_msg[512+1];

		/*
			GET https://www.googleapis.com/plus/v1/people/userId
		 */
		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - Start ";

		memset(finalRespBuf, 0, sizeof(finalRespBuf));
		memset(err_msg, 0, sizeof(err_msg));

		//check string lenght
		memset(tmpBuf, 0, sizeof(tmpBuf));

		if(strlen(access_token) > sizeof(tmpBuf))
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - access_token is too long, try to make tmp buffer bigger.";
			return false;
		}

//		sprintf(tmpBuf, "/plus/v1/people/me");
		sprintf(tmpBuf, "/plus/v1/people/me?fields=emails,displayName,image");

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen( L"VueceHub Win/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0 );

		// Specify an HTTP server.
		if( hSession )
		{
			hConnect = WinHttpConnect( hSession, L"www.googleapis.com",
					INTERNET_DEFAULT_HTTPS_PORT, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - session cannot be created.";
			bHasErr = TRUE;
		}

		// Create an HTTP request handle.
		if( hConnect )
		{
			wchar_t w_requrl[sizeof(tmpBuf)*2];

			ConvertCharArrayToLPCWSTR(tmpBuf, w_requrl, sizeof(w_requrl));

			hRequest = WinHttpOpenRequest( hConnect, L"GET", w_requrl,
					NULL, WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - Connection to host cannot be opened.";
			bHasErr = TRUE;
		}

		// Send a request.
		if( hRequest )
		{
			wchar_t w_head_access_tok[sizeof(tmpBuf)*2];

			memset(tmpBuf, 0, sizeof(tmpBuf));
			sprintf(tmpBuf, "Authorization: Bearer %s", access_token);

			ConvertCharArrayToLPCWSTR(tmpBuf, w_head_access_tok, sizeof(w_head_access_tok));

			WinHttpAddRequestHeaders(
			    hRequest,
			    w_head_access_tok,
			    -1L,
			    WINHTTP_ADDREQ_FLAG_ADD
			);

			WinHttpAddRequestHeaders(
			    hRequest,
			    L"Content-Type: application/x-www-form-urlencoded",
			    -1L,
			    WINHTTP_ADDREQ_FLAG_ADD
			);

			bResults = WinHttpSendRequest( hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0,
					0, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - Request cannot be opened";
			bHasErr = TRUE;
		}

		if (bResults)
		{
			bResults = WinHttpReceiveResponse( hRequest, NULL);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - Error when receiving response.";
			bHasErr = TRUE;
		}

		// Keep checking for data until there is nothing left.
		if( bResults )
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if( !WinHttpQueryDataAvailable( hRequest, &dwSize ) )
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - An error occurred when waiting for response data, code = " << GetLastError();
					bHasErr = TRUE;
				}

				// Allocate space for the buffer.
				if(dwSize > sizeof(tmpBuf))
				{
					LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - response is too long - " << dwSize;
					dwSize=0;
					bHasErr = TRUE;
				}
				else
				{
					// Read the data.
					ZeroMemory( tmpBuf, dwSize+1 );

					if( !WinHttpReadData( hRequest, (LPVOID)tmpBuf,
									dwSize, &dwDownloaded ) )
					{
						LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - An error occurred when reading response data, code = " << GetLastError( );
						bHasErr = TRUE;
					}
					else
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - Response is:\n " << tmpBuf;

						if(strlen(tmpBuf) > 0)
						{
							strcat(finalRespBuf, tmpBuf);
							LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - Current final response is:\n " << finalRespBuf;
						}
					}
				}

			}while( dwSize > 0 );

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo  - Request cannot be sent.";
		}


		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - Response returned, closing connection handles ";

		// Close any open handles.
		if( hRequest ) WinHttpCloseHandle( hRequest );
		if( hConnect ) WinHttpCloseHandle( hConnect );
		if( hSession ) WinHttpCloseHandle( hSession );

		if(bHasErr)
		{
			bResults = false;
		}


		// Report any errors.
		if( !bResults )
		{
//		    	  printf( "Error %d has occurred.\n", GetLastError( ) );
			LOG(LS_ERROR) << "VueceWinUtilities::QueryUserAccountInfo - Request failed, code =  " << GetLastError( );
		}
		else
		{

			if ( !bHasErr )
			{

				ExtracUserInfoFromResp(finalRespBuf, user_id, display_name, img_url);

				if(strlen(user_id) > 0)
				{
					bResults = true;
				}
			}
		}

		LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - Done";

		if(bResults)
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - user_id = " << user_id;
			LOG(LS_VERBOSE) << "VueceWinUtilities::QueryUserAccountInfo - display_name = " << display_name;
		}

		return bResults;
	}


	bool  VueceWinUtilities::RevokeAccountAccess( const char* refresh_token)
	{
		BOOL bResults = FALSE;
		BOOL bHasErr = FALSE;
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		HINTERNET hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;
		char finalRespBuf[1024+1];
		char tmpBuf[1024+1];
		int data_len = 0;

		char err_msg[512+1];

		/*
			GET https://www.googleapis.com/plus/v1/people/userId
		 */
		LOG(INFO) << "VueceWinUtilities::RevokeAccountAccess - Start, refresh_token: " << refresh_token;

		memset(finalRespBuf, 0, sizeof(finalRespBuf));
		memset(err_msg, 0, sizeof(err_msg));

		//check string lenght
		memset(tmpBuf, 0, sizeof(tmpBuf));

		if(strlen(refresh_token) > sizeof(tmpBuf))
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - access_token is too long, try to make tmp buffer bigger.";
			return false;
		}

		sprintf(tmpBuf, "/o/oauth2/revoke?token=%s", refresh_token);

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen( L"VueceHub Win/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0 );

		// Specify an HTTP server.
		if( hSession )
		{
			hConnect = WinHttpConnect( hSession, L"accounts.google.com",
					INTERNET_DEFAULT_HTTPS_PORT, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - session cannot be created.";
			bHasErr = TRUE;
		}

		// Create an HTTP request handle.
		if( hConnect )
		{
			wchar_t w_requrl[sizeof(tmpBuf)*2];

			ConvertCharArrayToLPCWSTR(tmpBuf, w_requrl, sizeof(w_requrl));

			hRequest = WinHttpOpenRequest( hConnect, L"GET", w_requrl,
					NULL, WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - Connection to host cannot be opened.";
			bHasErr = TRUE;
		}

		// Send a request.
		if( hRequest )
		{

			WinHttpAddRequestHeaders(
			    hRequest,
			    L"Content-Type: application/x-www-form-urlencoded",
			    -1L,
			    WINHTTP_ADDREQ_FLAG_ADD
			);

			bResults = WinHttpSendRequest( hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0,
					0, 0 );
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - Request cannot be opened";
			bHasErr = TRUE;
		}

		if (bResults)
		{
			bResults = WinHttpReceiveResponse( hRequest, NULL);
		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - Error when receiving response.";
			bHasErr = TRUE;
		}

		// Keep checking for data until there is nothing left.
		if( bResults )
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if( !WinHttpQueryDataAvailable( hRequest, &dwSize ) )
				{
					LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - An error occurred when waiting for response data, code = " << GetLastError();
					bHasErr = TRUE;
				}

				// Allocate space for the buffer.
				if(dwSize > sizeof(tmpBuf))
				{
					LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - response is too long - " << dwSize;
					dwSize=0;
					bHasErr = TRUE;
				}
				else
				{
					// Read the data.
					ZeroMemory( tmpBuf, dwSize+1 );

					if( !WinHttpReadData( hRequest, (LPVOID)tmpBuf,
									dwSize, &dwDownloaded ) )
					{
						LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - An error occurred when reading response data, code = " << GetLastError( );
						bHasErr = TRUE;
					}
					else
					{
						LOG(LS_VERBOSE) << "VueceWinUtilities::RevokeAccountAccess - Response is:\n " << tmpBuf;

						if(strlen(tmpBuf) > 0)
						{
							strcat(finalRespBuf, tmpBuf);
							LOG(LS_VERBOSE) << "VueceWinUtilities::RevokeAccountAccess - Current final response is:\n " << finalRespBuf;
						}
					}
				}

			}while( dwSize > 0 );

		}
		else
		{
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess  - Request cannot be sent.";
		}

		LOG(INFO) << "VueceWinUtilities::RevokeAccountAccess - Response returned, closing connection handles ";

		// Close any open handles.
		if( hRequest ) WinHttpCloseHandle( hRequest );
		if( hConnect ) WinHttpCloseHandle( hConnect );
		if( hSession ) WinHttpCloseHandle( hSession );

		if(bHasErr)
		{
			bResults = false;
		}


		// Report any errors.
		if( !bResults )
		{
//		    	  printf( "Error %d has occurred.\n", GetLastError( ) );
			LOG(LS_ERROR) << "VueceWinUtilities::RevokeAccountAccess - Request failed, code =  " << GetLastError( );
		}
		else
		{

			if ( !bHasErr )
			{
				LOG(INFO) << "VueceWinUtilities::RevokeAccountAccess - Ok";

				bResults = true;
			}
		}

		LOG(INFO) << "VueceWinUtilities::RevokeAccountAccess - Done";

		return bResults;

	}



	static void LogJsonNodeTypeById(int id)
	{
		switch(id)
		{
		case JSON_NULL:
			{
				LOG(LS_VERBOSE) << "json node type: NULL";
				break;
			}
		case JSON_STRING:
			{
				LOG(LS_VERBOSE) << "json node type: STRING";
				break;
			}
		case JSON_NUMBER:
			{
				LOG(LS_VERBOSE) << "json node type: NUMBER";
				break;
			}
		case JSON_BOOL:
			{
				LOG(LS_VERBOSE) << "json node type: BOOL";
				break;
			}
		case JSON_ARRAY:
			{
				LOG(LS_VERBOSE) << "json node type: ARRAY";
				break;
			}
		case JSON_NODE:
		{
			LOG(LS_VERBOSE) << "json node type: NODE";
			break;
		}
		}
	}

	/**
	 * skip_check flag is used to enable check for node name 'emails' during the first recursive call
	 * the node name must have 'emails' otherwise there is no point to continue parsing
	 */
	 static void ParseJsonNode_Email(JSONNODE *n, char* user_account);

	 static bool JsonGetValueByName(JSONNODE *n,  const char* name, char* value)
	 {
			 bool found = false;

			JSONNODE_ITERATOR i = json_find(n, name);
			if (i != json_end(n))
			{
				LOG(LS_VERBOSE) << ("JsonGetValueByName -  node found: ") << name;
				json_char *node_name = json_name(*i);
				LOG(LS_VERBOSE) << ("JsonGetValueByName - Found a node: ") << (const char*)node_name;

				LogJsonNodeTypeById(json_type(*i));

				json_char *node_value = json_as_string(*i);

				LOG(LS_VERBOSE) << ("JsonGetValueByName - Node value: ") << (const char*)node_value;

				strcpy(value, node_value);

				json_free(node_value);
				json_free(node_name);

			}

			return found;
	 }

     static void ParseJsonNode_Email(JSONNODE *n, char* user_account){

    	 	bool found_type = false;
    	 	bool found_email = false;

			LOG(LS_VERBOSE) << ("ParseJsonNode_Email -  Start");

			if (n == NULL || n == JSON_NULL){
				LOG(LS_VERBOSE) << ("ParseJsonNode_Email - Invalid JSON Node");
				return;
			}

			JSONNODE_ITERATOR i = json_begin(n);
			while (i != json_end(n))
			{
				if (*i == NULL || *i == JSON_NULL){
					LOG(LS_VERBOSE) << ("ParseJsonNode_Email -  Invalid JSON Node");
					return;
				}

				json_char *node_name = json_name(*i);
				LOG(LS_VERBOSE) << ("ParseJsonNode_Email - Found a node: ") << (const char*)node_name;

					if( json_type(*i) == JSON_ARRAY)
					{
						LOG(LS_WARNING) << ("ParseJsonNode_Email - Not expecting another array insided emails array.") ;
					}
					else if( json_type(*i) == JSON_NODE)
					{
						LOG(LS_VERBOSE) << ("ParseJsonNode_Email - This is a node, parse again") ;

						ParseJsonNode_Email(*i, user_account);
					}
					else if( json_type(*i) == JSON_STRING)
					{
						LOG(LS_VERBOSE) << ("ParseJsonNode_Email - This is a string") ;
					}

				json_char *node_value = json_as_string(*i);

				LOG(LS_VERBOSE) << ("ParseJsonNode_Email - value is: ") << (const char*)node_value;

				if(strcmp(node_name, "type") == 0)
				{
					if(strcmp(node_value, "account") == 0)
					{
						LOG(LS_VERBOSE) << ("ParseJsonNode_Email - located email with 'account' type");
						found_type = true;
					}
				}

				if(strcmp(node_name, "value") == 0)
				{
					LOG(LS_VERBOSE) << ("ParseJsonNode_Email - located value string");

					found_email = true;

					strcpy(user_account, node_value);
				}


				json_free(node_value);
				json_free(node_name);
				++i;

				
				if(found_type  && found_email)
				{
					LOG(LS_VERBOSE) << ("ParseJsonNode_Email - Account email successfully located.");			
					break;
				}
			}

			LOG(LS_VERBOSE) << ("ParseJsonNode_Email -  End");
		}

bool ExtracUserInfoFromResp (const char* response, char* user_account, char* display_name, char* img_url)
{
		BOOL bResults = FALSE;
		BOOL bHasErr = FALSE;
		JSONNODE *root = NULL;

		/**
		Full response example:

		{
			 "kind": "plus#person",
			 "etag": "\"RqKWnRU4WW46-6W3rWhLR9iFZQM/tkcy2tbegBhrmiUPourVTyzVhCI\"",
			 "gender": "male",
			 "emails": [
			  {
			   "value": "alice@gmail.com",
			   "type": "account"
			  }
			 ],
			 "objectType": "person",
			 "id": "116031504013957806541",
			 "displayName": "Jingjing Sun",
			 "name": {
			  "familyName": "Sun",
			  "givenName": "Jingjing"
			 },
			 "url": "https://plus.google.com/116031504013957806541",
			 "image": {
			  "url": "https://lh3.googleusercontent.com/-bZAvUNwdxfo/AAAAAAAAAAI/AAAAAAAAACU/BcNDhFqXPjU/photo.jpg?sz=50",
			  "isDefault": false
			 },
			 "isPlusUser": true,
			 "circledByCount": 0,
			 "verified": false
		}

		*/
		/**
		 * Example example response:
		 * {
				"emails":
				[
						{
						"value": "alice@gmail.com",
						"type": "account"
						}
				]
		 }
		 *
		 */
		LOG(LS_VERBOSE) << "VueceWinUtilities::ExtracUserInfoFromResp - A complete response has been successfully received: " << response;

		root = json_parse(response);

		int type = json_type(root);

		LogJsonNodeTypeById(type);

		JSONNODE_ITERATOR ie = json_find(root, "emails");
		if (ie != json_end(root)){
			ParseJsonNode_Email(*ie,  user_account);
		}

		JsonGetValueByName(root, "displayName", display_name);

		 ie = json_find(root, "image");
		if (ie != json_end(root)){
			JsonGetValueByName(*ie, "url", img_url);
		}

		json_delete(root);

		if(strlen(user_account) > 0)
		{
			LOG(LS_VERBOSE) << "VueceWinUtilities::ExtracUserInfoFromResp - User account email is: " << user_account;
		}

		LOG(LS_VERBOSE) << "VueceWinUtilities::ExtracUserInfoFromResp - Done";

		return bResults;

}
