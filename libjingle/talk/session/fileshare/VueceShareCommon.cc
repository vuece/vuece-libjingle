/*
 * VueceShareCommon.cc
 *
 *  Created on: Jul 14, 2012
 *      Author: Jingjing Sun
 */

#include "talk/session/fileshare/VueceShareCommon.h"

#include "VueceLogger.h"

extern "C" {
#include "libavformat/avformat.h"
}

namespace cricket {

///////////////////////////////////////////////////////////////////////////////
// FileShareManifest
///////////////////////////////////////////////////////////////////////////////

void FileShareManifest::AddFile(const std::string& name, size_t size) {
	Item i = { T_FILE, name, size };
	items_.push_back(i);
}

void FileShareManifest::AddImage(const std::string& name, size_t size, size_t width, size_t height) {
	Item i = { T_IMAGE, name, size, width, height };
	items_.push_back(i);
}

void FileShareManifest::AddMusic(const std::string& name, size_t size, size_t width, size_t height,
		 int bit_rate,  int sample_rate,int nchannels, int duration) {
	Item i = { T_MUSIC, name, size, width, height,
			bit_rate, sample_rate, nchannels, duration };
	items_.push_back(i);
}

void FileShareManifest::AddFolder(const std::string& name, size_t size) {
	Item i = { T_FOLDER, name, size };
	items_.push_back(i);
}

size_t FileShareManifest::GetItemCount(FileType t) const {
	size_t count = 0;
	for (size_t i = 0; i < items_.size(); ++i) {
		if (items_[i].type == t)
			++count;
	}
	return count;
}

bool VueceStreamUtil::GetStreamDurationInMilliSecs(const char* filePath, size_t* duration)
	{
		AVFormatContext *pFormatCtx = NULL;
		int ret = 0;

		VueceLogger::Debug("VueceStreamUtil - GetStreamDurationInMilliSecs");
		VueceLogger::Debug("Opening file: %s", filePath);

		//ret = avformat_open_input(&pFormatCtx, (const char*)filePath, NULL, NULL);


	   av_register_all();


		//ret = av_open_input_file(&pFormatCtx, (const char*)filePath, NULL, 0, NULL);

	   	ret = avformat_open_input(&pFormatCtx, (const char*)filePath, NULL, NULL);

		VueceLogger::Debug("avformat_open_input returned: %d", ret);

		if(ret!=0)
		{
			VueceLogger::Fatal("Cannot open this file.");
			return false; // Couldn't open file
		}

		VueceLogger::Debug("File successfully opened.");

		// Retrieve stream information
		if(av_find_stream_info(pFormatCtx)<0)
		{
			VueceLogger::Error("Couldn't find stream information.");
			return false; // Couldn't find stream information
		}

		VueceLogger::Debug("VUECE STREAM UTIL - ========== Dump Media File Info Start =========================");
		av_dump_format(pFormatCtx, 0, (const char*)filePath, false);
		VueceLogger::Debug("VUECE STREAM UTI - ========== Dump Media File Info End =========================");

		*duration = pFormatCtx->duration / 1000;

		VueceLogger::Debug("Stream duration = %u milliseconds", *duration);

		av_close_input_file(pFormatCtx);

		return true;
	}


}
