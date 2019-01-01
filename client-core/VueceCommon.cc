/*
 * VueceCommon.cc
 *
 *  Created on: 2014-9-10
 *      Author: Jingjing Sun
 */

#include "VueceCommon.h"
#include "VueceConstants.h"
#include "VueceLogger.h"
#include "VueceGlobalSetting.h"

#include "talk/base/stringdigest.h"
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"

#include <openssl/md5.h>

#define MD5_BUFSIZE	1024*4

#ifdef ANDROID
#define VUECE_LOG_FILE_LOCATION "/sdcard/vuece/tmp"
#endif

void VueceCommon::ConfigureLogging(int logging_level)
{
	char buf[512];
	char buf2[512];

	struct tm tstruct;

	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));

	time_t now = time(0);   // get time now
	tstruct = *localtime(&now);

	//log file format: YEAR-MONTH-DAY_HOURMINSEC.log
	strftime(buf2, sizeof(buf2), "vuece_hub_trace_%Y-%m-%d_%H%M%S.log", &tstruct);

#ifndef ANDROID
	//example: PC_LOG_FILE_LOC="C:\\vuece-pc-logs\\"
	strcpy(buf, VueceGlobalContext::GetAppUserDataDir());
#else
	strcpy(buf, VUECE_LOG_FILE_LOCATION);
#endif

	strcat(buf, buf2);

	switch (logging_level)
	{
	case VUECE_LOG_LEVEL_DEBUG:
		talk_base::LogMessage::ConfigureLogging(
				"tstamp thread verbose file", buf);
		strcpy(buf2, "DEBUG");
		break;
	case VUECE_LOG_LEVEL_INFO:
		talk_base::LogMessage::ConfigureLogging("tstamp thread info file", buf);
		strcpy(buf2, "INFO");
		break;
	case VUECE_LOG_LEVEL_WARN:
		talk_base::LogMessage::ConfigureLogging("tstamp thread warning file",
				buf);
		strcpy(buf2, "WARN");
		break;
	case VUECE_LOG_LEVEL_ERROR:
		talk_base::LogMessage::ConfigureLogging("tstamp thread error file",
				buf);
		strcpy(buf2, "ERROR");
		break;
	case VUECE_LOG_LEVEL_NONE:
		talk_base::LogMessage::ConfigureLogging("none", buf);
		strcpy(buf2, "NONE");
		break;
	default:
		break;
	}

	talk_base::LogMessage::LogTimestamps(true);

	LOG(LS_VERBOSE) << "VueceCommon::ConfigureLogging - Done, level is: " << buf2 << ", log file path: " << buf;
	LOG(LS_INFO) << "VueceCommon::ConfigureLogging - Done, level is: " << buf2 << ", log file path: " << buf;
	LOG(LS_WARNING) << "VueceCommon::ConfigureLogging - Done, level is: " << buf2 << ", log file path: " << buf;
	LOG(LS_ERROR) << "VueceCommon::ConfigureLogging - Done, level is: " << buf2 << ", log file path: " << buf;
}

std::string VueceCommon::CalculateMD5FromFile(const std::string& path)
{
	MD5_CTX ctx;
	size_t result = 0;
	size_t f_size = 0;
	size_t total_read = 0;
	unsigned char buf[MD5_BUFSIZE];
	unsigned char digest[16];
	int error;
	talk_base::StreamResult sr;
	std::string hex_digest = "";

	talk_base::FileStream::GetSize(path, &f_size);

	LOG(LS_VERBOSE) << "CalculateMD5FromFile - File size = " << f_size
			<< " bytes";

	LOG(LS_VERBOSE) << "CalculateMD5FromFile - Opening file: " << path;

	talk_base::FileStream* file = new talk_base::FileStream;

	if (file->Open(path, "rb", NULL))
	{
		LOG(LS_INFO) << "CalculateMD5FromFile:File opened";
	}
	else
	{
		LOG(LS_INFO) << "CalculateMD5FromFile - Cannot open file";
		delete file;
	}

	MD5_Init(&ctx);

	while (true)
	{

		sr = file->Read(buf, MD5_BUFSIZE, &result, &error);

//		LOG(LS_VERBOSE) << "CalculateMD5FromFile - Bytes read: " << result;

		total_read += result;

//		LOG(LS_VERBOSE) << "CalculateMD5FromFile - total_read: " << total_read;

		if (sr == talk_base::SR_ERROR)
		{
			LOG(LS_VERBOSE)
					<< "CalculateMD5FromFile - Error occurred during file read, stop reading now";
			break;
		}
		else if (sr == talk_base::SR_EOS)
		{
			LOG(LS_VERBOSE)
					<< "CalculateMD5FromFile - Hit EOF, stop reading";
			break;
		}
		else
		{
			//continue reading
		}

		MD5_Update(&ctx, buf, (unsigned long) result);
	}

	file->Close();

	delete file;

	LOG(LS_VERBOSE)
			<< "CalculateMD5FromFile - Reading finished, bytes total read: "
			<< total_read;

	if (sr == talk_base::SR_ERROR)
	{
		LOG(LS_VERBOSE)
				<< "CalculateMD5FromFile - Error occurred during file reading, empty string will be returned";
		return "";
	}

	if (total_read != f_size)
	{
		LOG(LS_VERBOSE)
				<< "CalculateMD5FromFile - Error occurred, total bytes read is not equal to file size: "
				<< f_size;
		;
	}

	MD5_Final(digest, &ctx);

	for (int i = 0; i < 16; ++i)
	{
		hex_digest += talk_base::hex_encode(digest[i] >> 4);
		hex_digest += talk_base::hex_encode(digest[i] & 0xf);
	}

	return hex_digest;
}

void VueceCommon::LogVueceEvent(VueceEvent code)
{
	switch(code)
	{
	case VueceEvent_Client_SignedIn:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_SignedIn";
		break;
	case VueceEvent_Client_SignedOut:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_SignedOut";
		break;
	case VueceEvent_Client_AuthFailed:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_AuthFailed";
		break;

	case VueceEvent_Client_BackOnLine:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_Client_BackOnLine";
		break;
	case VueceEvent_Client_Destroyed:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_Client_Destroyed";
		break;
	case VueceEvent_FileAccess_Denied:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_FileAccess_Denied";
		break;
	case VueceEvent_Connection_Started:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_Connection_Started";
		break;
	case VueceEvent_Connection_Failed:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_Connection_Failed";
		break;
	case VueceEvent_Connection_FailedWithAutoReconnect:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_Connection_FailedWithAutoReconnect";
		break;
	case VueceEvent_None:
		LOG(LS_VERBOSE) << "VueceCommon::LogVueceEvent - VueceEvent_None";
		break;
	default:
		VueceLogger::Fatal("VueceCommon::LogVueceEvent - Unknown code: %d, abort now.", code);
		break;
	}
}

