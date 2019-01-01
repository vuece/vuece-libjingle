/*
 * VueceMediaStream.cc
 *
 *  Created on: Jul 14, 2012
 *      Author: Jingjing Sun
 */

#if defined(POSIX)
#include <sys/file.h>
#endif  // POSIX

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string>
#include <limits>

#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/messagequeue.h"
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"
#include "talk/base/thread.h"
#include "talk/base/pathutils.h"
#include "talk/base/fileutils.h"
#include "talk/session/fileshare/VueceMediaStream.h"

#ifndef VUECE_APP_ROLE_HUB
#include "talk/session/fileshare/VueceStreamEngine.h"
#include "talk/session/fileshare/VueceMediaDataBumper.h"
#include "talk/session/fileshare/VueceStreamPlayer.h"
#endif

#include "VueceGlobalSetting.h"
#include "VueceConstants.h"
#include "VueceLogger.h"

static char cVueceFFmpegLogBuf[1024];

//#define LOCAL_DECODE_TEST 1

#ifdef LOCAL_DECODE_TEST
AVCodec * pTestDecodeCodec;
AVCodecContext  *pTestCodecCtx;
int16_t 	*test_outbuf;
#endif

#define BUFFER_WINDOW_ENABLED 1

namespace talk_base {

static void VueceFFmpgeLogCallBack(void* avcl, int level, const char*fmt, va_list vl) {
	//vl doesn't compile with Android
	//	if(vl != 0)
	//	{
	vsprintf(cVueceFFmpegLogBuf, fmt, vl);

	switch (level) {
	case AV_LOG_VERBOSE:
	case AV_LOG_DEBUG:
	case AV_LOG_INFO:
		LOG(LS_VERBOSE)<< (cVueceFFmpegLogBuf);
		break;

		case AV_LOG_WARNING:
		{
			LOG(LS_WARNING) << (cVueceFFmpegLogBuf);
			break;
		}
		case AV_LOG_ERROR:
		{
			LOG(LS_ERROR) << (cVueceFFmpegLogBuf);
			break;
		}
		case AV_LOG_FATAL:
		case AV_LOG_PANIC:
		{
			LOG(LS_ERROR) << (cVueceFFmpegLogBuf);
			break;
		}
	}
	//	}
}

static void ffmpeg_init() {
	static bool done=FALSE;
	VueceLogger::Debug("VueceMediaStream - ffmpeg_init");
	if (!done) {
		VueceLogger::Debug("VueceMediaStream - ffmpeg_init - 1");
		av_log_set_callback(VueceFFmpgeLogCallBack);
		// Register all formats and codecs
		VueceLogger::Debug("VueceMediaStream - ffmpeg_init : av_register_all");
		av_register_all();
		VueceLogger::Debug("VueceMediaStream - ffmpeg_init - 2");

		done=true;
	}
	else
	{
		VueceLogger::Debug("VueceMediaStream - ffmpeg_init, already initialized");
	}
}



#ifndef VUECE_APP_ROLE_HUB

static void generate_chunk_file_name_from_number(int num, char* fname, bool isVideo)
{
	char tmp[16];

	VueceLogger::Debug("VueceMediaStream - generate_chunk_file_name_from_number: %d", num);

	memset(tmp, 0, sizeof(tmp));

	if(!isVideo)
	{
		VueceLogger::Debug("VueceMediaStream - generate audio chunk file name");
		strcpy(fname, VUECE_MEDIA_AUDIO_BUFFER_LOCATION);
	}
	else
	{
		VueceLogger::Debug("VueceMediaStream - generate video chunk file name");
		strcpy(fname, VUECE_MEDIA_VIDEO_BUFFER_LOCATION);
	}


	sprintf(tmp, "%d", num);

	strcat(fname, tmp);
}

static void activate_audio_chunk_file(VueceStreamData* d)
{
	char cfilename[128];
	char mode[8];
	FILE* file_ = NULL;

	VueceLogger::Debug("VueceMediaStream - activate_audio_chunk_file");

	if(d->fActiveAudioChunkFile != NULL)
	{
//		VueceLogger::Debug("VueceMediaStream - activate_audio_chunk_file: Activate file is not null, close it at first.");
//		fclose(d->fActiveAudioChunkFile);
//        d->fActiveAudioChunkFile = NULL;

		VueceLogger::Fatal("VueceMediaStream - activate_audio_chunk_file: Fatal error - d->fActiveAudioChunkFile should be NULL!");
		return;
	}

	memset(cfilename, 0, sizeof(cfilename));
	memset(mode, 0, sizeof(mode));

	strcpy(mode, "w");

	generate_chunk_file_name_from_number(d->iLastAvailableAudioChunkFileIdx, cfilename, false);

	VueceLogger::Debug("VueceMediaStream - activate_audio_chunk_file: file name created: %s", cfilename);

	file_ = fopen(cfilename, mode);

	if(file_ == NULL)
	{
	  VueceLogger::Fatal("VueceMediaStream - bumper_activate_buffer_file: file open failed!");
	}

	d->fActiveAudioChunkFile = file_;

}

static void activate_video_chunk_file(VueceStreamData* d)
{
	char cfilename[128];
	char mode[8];
	FILE* file_ = NULL;

	VueceLogger::Debug("VueceMediaStream - activate_video_chunk_file");

	if(d->fActiveVideoChunkFile != NULL)
	{
		VueceLogger::Fatal("VueceMediaStream - activate_video_chunk_file: Fatal error - d->fActiveVideoChunkFile should be NULL!");
		return;
	}

	memset(cfilename, 0, sizeof(cfilename));
	memset(mode, 0, sizeof(mode));

	strcpy(mode, "w");

	generate_chunk_file_name_from_number(d->iCurrentVideoChunkFileIdx, cfilename, true);

	VueceLogger::Debug("VueceMediaStream - activate_video_chunk_file: file name created: %s", cfilename);

	file_ = fopen(cfilename, mode);

	if(file_ == NULL)
	{
	  VueceLogger::Fatal("VueceMediaStream - activate_video_chunk_file: file open failed!");
	}

	d->fActiveVideoChunkFile = file_;

}

/**
 * Write one frame into current chunk file and check the number of frames in current chunk file
 * if frame number reaches the threshold value defined by VUECE_AUDIO_FRAMES_PER_CHUNK, close
 * current chunk file, increase the index of last available chunk file and activate a new chunk file
 * for next write operation.
 */
void VueceMediaStream::WriteAudioFrameToChunkFile(uint8_t* frame, size_t len, VueceStreamData* d)
{
	size_t result;

	if(d->fActiveAudioChunkFile == NULL)
	{
		VueceLogger::Fatal("VueceMediaStream::WriteAudioFrameToChunkFile - active chunk file is NULL!");
		return;
	}

	result = fwrite(frame, 1, len, d->fActiveAudioChunkFile);

	if(result != len)
	{
		VueceLogger::Fatal("VueceMediaStream::WriteAudioFrameToChunkFile - Write failed!");
		return;
	}

	d->iTotalAudioFrameCounter++;
	d->iAudioFrameCounterInCurrentChunk++;

	if(d->iAudioFrameCounterInCurrentChunk == VUECE_AUDIO_FRAMES_PER_CHUNK)
	{
		fclose(d->fActiveAudioChunkFile);
		d->fActiveAudioChunkFile = NULL;

		VueceLogger::Debug("VueceMediaStream::WriteAudioFrameToChunkFile - chunk file completed with index: %d", d->iLastAvailableAudioChunkFileIdx);

		VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(d->iLastAvailableAudioChunkFileIdx);

		//notify vuece bumper that the last index of the available chunk file
		if(VueceStreamPlayer::HasStreamEngine())
		{
			VueceStreamPlayer::InjectLastAvailChunkIdIntoBumper(d->iLastAvailableAudioChunkFileIdx);
		}

		size_t mediaDur = -1;

		iStreamData->iCurrentFramePositionSec = iStreamData->iTotalAudioFrameCounter *
				iStreamData->iAACRawFrameBytes / iStreamData->lTotalAudioBytesConsumedPerSecond;

		mediaDur = iStreamData->iCurrentFramePositionSec + iStreamData->iFirstFramePosSec;

		LOG(LS_VERBOSE) << "VueceMediaStream::WriteAudioFrameToChunkFile - current chunk is full, inject stream termination position: " << mediaDur;

		//update terminate position in audio writer
		VueceStreamPlayer::InjectStreamTerminationPosition(mediaDur, false);

		//TODO - Check buffer download threshold here, if reached, terminate download
		//Note file id starts with zero
//BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
		if(BUFFER_WINDOW_ENABLED)
		{
			int window_width = d->iLastAvailableAudioChunkFileIdx - d->iFirstAudioChunkFileIdxInBufWindow + 1;

			VueceLogger::Debug("VueceMediaStream::WriteAudioFrameToChunkFile - Calculate current buffer window width, last chunk id = %d, first chunk id = %d, width = %d",
					d->iLastAvailableAudioChunkFileIdx, d->iFirstAudioChunkFileIdxInBufWindow, window_width);

			if(window_width >= VUECE_BUFFER_WINDOW)
			{
				LOG(LS_VERBOSE) << "VueceMediaStream::WriteAudioFrameToChunkFile - Current buffer window is full, stream will be terminated.";

				//we need to remember the last downloaded chunk file ID as a global variable because VueceMediaStream will be destroyed once we
				//return EOS, it triggers session termination
				d->bBufWindowFull = true;

				VueceLogger::Debug("TROUBLESHOOTING 4 - iStreamData V = %d, Global V = %d",
						d->iLastAvailableAudioChunkFileIdx, VueceGlobalContext::GetLastAvailableAudioChunkFileIdx());

				VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(d->iLastAvailableAudioChunkFileIdx);
				VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(d->iAudioFrameCounterInCurrentChunk);
				VueceGlobalContext::SetTotalAudioFrameCounter(d->iTotalAudioFrameCounter);
				VueceLogger::Debug("TROUBLESHOOTING 1 - V = %d", d->iLastAvailableAudioChunkFileIdx);

				VueceStreamPlayer::LogCurrentStreamingParams();

				LOG(LS_VERBOSE) << "VueceMediaStream::WriteAudioFrameToChunkFile - Activate audio writer buffer window check.";
				VueceStreamPlayer::EnableBufWinCheckInAudioWriter();

				VueceLogger::Debug("VueceMediaStream::WriteAudioFrameToChunkFile - ----------------------------------------------");

				return;
			}
		}

		//If buffer window is not full or disabled, then continue downloading and writing data to disk
		//increase chunk file idx and create a new chunk file
		d->iLastAvailableAudioChunkFileIdx++;

		//reset frame counter
		d->iAudioFrameCounterInCurrentChunk = 0;

		activate_audio_chunk_file(d);

		VueceLogger::Debug("VueceMediaStream::WriteAudioFrameToChunkFile - next chunk file activated with index: %d", d->iLastAvailableAudioChunkFileIdx);
		VueceLogger::Debug("TROUBLESHOOTING 5 - iStreamData V = %d, Global V = %d", d->iLastAvailableAudioChunkFileIdx, VueceGlobalContext::GetLastAvailableAudioChunkFileIdx());

	}

}

static void write_video_frame_to_chunk_file(uint8_t* frame, size_t len, VueceStreamData* d)
{
	size_t result;

	if(d->fActiveVideoChunkFile == NULL)
	{
		VueceLogger::Fatal("write_video_frame_to_chunk_file - active chunk file is NULL!");
		return;
	}

	result = fwrite(frame, 1, len, d->fActiveVideoChunkFile);

	if(result != len)
	{
		VueceLogger::Fatal("write_video_frame_to_chunk_file - Write failed!");
		return;
	}

	d->lTotoalVideoFrameCounter++;
	d->iReceivedVideoFrameCounter++;

	if(d->iReceivedVideoFrameCounter == VUECE_VIDEO_FRAMES_PER_CHUNK)
	{
		fclose(d->fActiveVideoChunkFile);
		d->fActiveVideoChunkFile = NULL;

		VueceLogger::Debug("write_video_frame_to_chunk_file - chunk file completed with index: %d", d->iCurrentVideoChunkFileIdx);

		//increase chunk file idx and create a new chunk file
		d->iCurrentVideoChunkFileIdx++;
		d->iReceivedVideoFrameCounter = 0;

		activate_video_chunk_file(d);

		VueceLogger::Debug("write_video_frame_to_chunk_file - next chunk file activated with index: %d", d->iCurrentVideoChunkFileIdx);
	}
}
#endif

bool VueceMediaStream::InternalInit(bool isServer)
{

	bool ret = true;

	VueceLogger::Debug("VueceMediaStream::InternalInit");

	bIsServer = isServer;

	iStreamState = SS_CLOSED;

	LOG(LS_VERBOSE) << "VueceMediaStream::InternalInit - Init VueceStreamData";

	iStreamData =(VueceStreamData*)malloc(sizeof(VueceStreamData));

	memset(iStreamData, 0, sizeof(VueceStreamData));

	iStreamData->bBufWindowFull = false;

	iStreamData->bReceivingFirstFrame = true;
	iStreamData->iFirstFramePosSec = 0;
	iStreamData->pFormatCtx = NULL;
	iStreamData->pAudioTranscodeEncCtx = NULL;
	iStreamData->pAudioTranscodeEnc = NULL;
	iStreamData->pAudioTranscodeDec = NULL;

	iStreamData->iAACRawFrameBytes = 4096;
	iStreamData->iMP3RawFrameBytes = -1;//4608;

	iStreamData->iBufferLen = 0;
	iStreamData->iFileSize = 0;
	iStreamData->iAudioBytesRead = 0;
	iStreamData->iFrameHeaderStartPos = 0;
	iStreamData->iCurrentFramePositionSec = 0;

	iStreamData->iAudioFrameCounterInCurrentChunk = 0;
	iStreamData->iLastAvailableAudioChunkFileIdx = 0;

	VueceLogger::Debug("TROUBLESHOOTING 0 - V = %d", iStreamData->iLastAvailableAudioChunkFileIdx);

	iStreamData->iFirstAudioChunkFileIdxInBufWindow = 0;

	iStreamData->iTotalAudioFrameCounter = 0;
	iStreamData->lTotoalVideoFrameCounter = 0;

	iStreamData->iReceivedVideoFrameCounter = 0;
	iStreamData->iCurrentVideoChunkFileIdx = 0;
	iStreamData->iFrameDurationInMs = 0;

	iStreamData->fActiveAudioChunkFile = NULL;
	iStreamData->fActiveVideoChunkFile = NULL;

	iStreamData->bIsDownloadCompleted = false;
	iStreamData->bIsReceivingAudioPacket = true;


	iStreamData->bIsReadingBigAudioFrame = false;
	iStreamData->bIsReadingBigVideoFrame = false;
	iStreamData->lCurrentBigFrameReadPos = 0;

	//NOTE - Following fields are hard-coded in order to give the some default values
	//actual values will be populated when the codec is open for the target audio file
	//see the Open() method for details

	LOG(LS_VERBOSE) << "Checking music attributes - sample_rate = " << sample_rate
						<< ", bit_rate = " << bit_rate << ", nchannels = " << nchannels
						<< ", duration = " << duration;

	ASSERT(sample_rate != 0);
	ASSERT(bit_rate != 0);
	ASSERT(nchannels != 0);
	ASSERT(duration != 0);

	iStreamData->iAudioSampleRate = sample_rate;
	iStreamData->iBitRate = bit_rate;
	iStreamData->iNChannels = nchannels;
	iStreamData->iDuration = duration;

	iStreamData->iBitDepth = 16; //hard coded for now

	if(nchannels == 1)
	{
		iStreamData->iAACRawFrameBytes = 2048;
	}
	else
	{
		iStreamData->iAACRawFrameBytes = 4096;
	}

	iStreamData->lTotalAudioBytesConsumedPerSecond =
			iStreamData->iAudioSampleRate *
			iStreamData->iBitDepth *
			iStreamData->iNChannels / 8;

	iStreamData->iCurrentTimeStamp = 0;

	if(iStartPosSec > 0)
	{
		iStreamData->iCurrentTimeStamp = iStartPosSec*1000;

		VueceLogger::Debug("VueceMediaStream::InternalInit: Start position is non-zero, base time stamp is set to %d ms",
				iStreamData->iCurrentTimeStamp);
	}

	iStreamData->iFrameHeaderLen = VUECE_STREAM_FRAME_HEADER_LENGTH;

	//as mentioned above, this value will be updated in Open() method
	VueceLogger::Debug("VueceMediaStream::InternalInit - lTotalAudioBytesConsumedPerSecond = %ld B by default",
			iStreamData->lTotalAudioBytesConsumedPerSecond);

	VueceLogger::Debug("VueceMediaStream::InternalInit - 1");

	ffmpeg_init();

	VueceLogger::Debug("VueceMediaStream::InternalInit - 2");

	//allocate memory for buffers
	//TODO Note this can be avoided if we don't need transcode
	iStreamData->pAudioDecOutBuf = (int16_t*)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	VueceLogger::Debug("VueceMediaStream::InternalInit - 3");

	iStreamData->pAudioOutBufTranscoded = (uint8_t*)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	VueceLogger::Debug("VueceMediaStream::InternalInit - 4");

	iStreamData->pAudioEncodeFifo = av_fifo_alloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	VueceLogger::Debug("VueceMediaStream::InternalInit - 5");

	iStreamData->iBigFrameBuf 	= (uint8_t*)malloc(VUECE_MAX_PACKET_SIZE);

	iStreamData->pTmpBuf = (uint8_t*)av_malloc(VUECE_ENCODE_OUTPUT_BUFFER_SIZE);

	VueceLogger::Debug("VueceMediaStream::InternalInit - 6");

#ifndef VUECE_APP_ROLE_HUB
	if(!bIsServer)
	{

		LOG(INFO) << "VueceMediaStream::InternalInit - In hub client mode,  creating audio stream as a media hub client.";

		iStreamData->iBuffer 		= (uint8_t*)malloc(VUECE_MAX_BUFFER_SIZE);

		memset(iStreamData->iBuffer, 0, VUECE_MAX_BUFFER_SIZE);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Determine frame duration - Start
		// This is very important because SEEK operation won't succeed if the frame duration is not accurate

		LOG(INFO) << "determine frame duration";

		if(iStreamData->iAudioSampleRate == 22050 && iStreamData->iNChannels == 2)
		{
			iStreamData->iAACRawFrameBytes = 4096;

			LOG(LS_VERBOSE) << "VueceMediaStream::Open - Sample rate is 22050 and nchannel is 2, iAACRawFrameBytes is set to 4096";
		}

		if(iStreamData->iAudioSampleRate == 22050 && iStreamData->iNChannels == 1)
		{
			iStreamData->iAACRawFrameBytes = 2048;

			LOG(LS_VERBOSE) << "VueceMediaStream::Open - Sample rate is 22050 and nchannel is 1, iAACRawFrameBytes is set to 2048";
		}

		LOG(LS_VERBOSE) << "VueceMediaStream::Open(Hub client mode) - Final iAACRawFrameBytes is set to "<< iStreamData->iAACRawFrameBytes;

		//now calculate frame duration
		VueceLogger::Debug("VueceMediaStream::InternalInit - lTotalAudioBytesConsumedPerSecond = %ld B by default",
				iStreamData->lTotalAudioBytesConsumedPerSecond);

		double tmp2 = (double)iStreamData->iAACRawFrameBytes / iStreamData->lTotalAudioBytesConsumedPerSecond;
		iStreamData->iFrameDurationInMs = tmp2 * 1000;

		VueceLogger::Debug("VueceMediaStream::Open(Hub client mode) - iFrameDurationInMs = %d ms",
				iStreamData->iFrameDurationInMs);
		// Determine frame duration - End
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		//TODO - Check player state here, if it's waiting for next buffer window, then there is no
		//need to create a new player.

		if(VueceStreamPlayer::StillInTheSamePlaySession())
		{
			int idx = VueceGlobalContext::GetLastAvailableAudioChunkFileIdx();

			VueceLogger::Debug("VueceMediaStream::Open(Hub client mode) - Player is currently waiting for next buffer window, last available chunk file id = %d",
					idx);

			VueceStreamPlayer::LogCurrentStreamingParams();

			//recover some previous streaming values otherwise play progress value will not be correct
			iStreamData->iTotalAudioFrameCounter = 0;
			iStreamData->iFirstFramePosSec = VueceGlobalContext::GetFirstFramePositionSec();

			VueceLogger::Debug("VueceMediaStream::Open(Hub client mode) - Recovered previous streaming position info, iTotalAudioFrameCounter = %d, iFirstFramePosSec = %d",
					iStreamData->iTotalAudioFrameCounter, iStreamData->iFirstFramePosSec);

			idx++;

			//activate a new chunk file which is next to the last available chunk
			VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(idx);

			VueceLogger::Debug("TROUBLESHOOTING 2 - iStreamData V = %d, Global V = %d", iStreamData->iLastAvailableAudioChunkFileIdx, idx);

			iStreamData->iLastAvailableAudioChunkFileIdx = idx;

			iStreamData->iFirstAudioChunkFileIdxInBufWindow = iStreamData->iLastAvailableAudioChunkFileIdx;

			VueceLogger::Debug("VueceMediaStream::Open(Hub client mode) - First chunk file id in buffer window is updated to %d",
					iStreamData->iFirstAudioChunkFileIdxInBufWindow);

			activate_audio_chunk_file(iStreamData);
		}
		else
		{
			//create and open the first audio chunk file
			activate_audio_chunk_file(iStreamData);

			activate_video_chunk_file(iStreamData);

			ASSERT(NULL == VueceStreamEngine::Instance());

			//reset other global properties
			VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(VUECE_VALUE_NOT_SET);
			VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(VUECE_VALUE_NOT_SET);
			VueceGlobalContext::SetDownloadCompleted(false);
			VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(VUECE_VALUE_NOT_SET);
			VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(VUECE_VALUE_NOT_SET);

			VueceLogger::Debug("VueceMediaStream::InternalInit - TROUBLESHOOTING: Global settings are reset");

			VueceLogger::Debug("VueceMediaStream::InternalInit - Start pos is: %lu second",
					VueceGlobalContext::GetNewResumePos());

			ret = VueceStreamPlayer::CreateStreamEngine(
				iStreamData->iAudioSampleRate,
				iStreamData->iBitRate,
				iStreamData->iNChannels,
				iStreamData->iDuration,
				iStreamData->iFrameDurationInMs,
				false,
				false,
				-1,
				-1,
				VueceGlobalContext::GetNewResumePos()
			   );

			//once used, reset to 0
			VueceGlobalContext::SetNewResumePos(0);
			VueceLogger::Debug("VueceMediaStream::InternalInit - new_resume_pos is reset to 0");

			if(ret)
			{
				VueceStreamPlayer::StartStreamEngine();
			}
			else
			{
				VueceLogger::Error("VueceMediaStream::InternalInit - Failed to create stream engine.");
			}

		}

		VueceThreadUtil::MutexLock(&mutex_wait_session_release);
		bAllowWrite = true;

		LOG(LS_VERBOSE) << "VueceMediaStream::InternalInit - Write is allowed now.";

		VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);
	}

	VueceLogger::Debug("VueceMediaStream::InternalInit(Hub client mode) - Done");
#endif

	return ret;
}

void VueceMediaStream::InternalRelease()
{

	//NOTE - This is called from session management thread (same as VueceMediaStreamSessionClient and VueceMediaStreamSession thread)
	//, but the audio frame data is read from another thread - the network streaming thread, so before we release all
	// Relevant resources, we need a flag to tell the streaming thread that all resource should not be accessed.
	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 1";

	if(iStreamData == NULL)
	{
		LOG(LS_VERBOSE) << "VueceMediaStream - iStreamData is NULL, return now.";
		return;
	}

	av_free(iStreamData->pAudioDecOutBuf);

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 1a";

	av_free(iStreamData->pAudioOutBufTranscoded);

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 1b";

	av_fifo_free(iStreamData->pAudioEncodeFifo);

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 1c";

	av_free(iStreamData->pTmpBuf);

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 1d";

	free(iStreamData->iBigFrameBuf);

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 2";

	if(iStreamData->pAudioTranscodeEncCtx)
	{
		LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 3";
		avcodec_close(iStreamData->pAudioTranscodeEncCtx);
	}

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 4";

	if(bIsServer)
	{
		LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 5";
		av_close_input_file(iStreamData->pFormatCtx);
	}
	else
	{
#ifndef VUECE_APP_ROLE_HUB
		LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 6";

		free(iStreamData->iBuffer);

		if(iStreamData->fActiveAudioChunkFile != NULL)
		{
			LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 7";
			fclose(iStreamData->fActiveAudioChunkFile);
			iStreamData->fActiveAudioChunkFile = NULL;

		}
		else
		{
			LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease, active chunk file already closed.";
		}
#endif
	}

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease 8";

	LOG(LS_VERBOSE) << "VueceMediaStream - InternalRelease done! Total audio frames = "
			<< iStreamData->iTotalAudioFrameCounter << ", total video frames = " << iStreamData->lTotoalVideoFrameCounter;

	free(iStreamData);

	iStreamData = NULL;
}

VueceMediaStream::VueceMediaStream(const std::string& session_id_, int sample_rate_, int bit_rate_, int nchannels_, int duration_)
{

	LOG(LS_VERBOSE) << "VueceMediaStream - Constructor called with session id: " << session_id_ << ", sample_rate: " << sample_rate_
	<< ", bit_rate: " << bit_rate_ << ", nchannels_: " << nchannels_ << ", duration_: " << duration_;

//	iStreamData = (struct VueceStreamData *) malloc( sizeof( struct VueceStreamData ));

	session_id = session_id_;
	sample_rate = sample_rate_;
	bit_rate = bit_rate_;
	nchannels = nchannels_;
	duration = duration_;

	bIsServer = false;
	bIsAllDataConsumed = false;
	iStartPosSec = 0;
	bAllowWrite = false;

#ifdef ANDROID
	VueceThreadUtil::InitMutex(&mutex_wait_session_release);
#endif

}

VueceMediaStream::~VueceMediaStream() {
	LOG(LS_VERBOSE) << "--------- VueceMediaStream - Destructor called ------------";

#ifdef ANDROID
	mutex_wait_session_release.Unlock();
#endif

	VueceMediaStream::Close();

	LOG(LS_VERBOSE) << "--------- VueceMediaStream - Destructor called OK ------------";

}

bool VueceMediaStream::Open(const std::string& filename, const char* mode, int start_pos_)
{

	LOG(LS_VERBOSE) << "VueceMediaStream::Open - file name: " << filename << ", mode = " << mode
			<< ", iStartPosSec = " << start_pos_;

	iStartPosSec = start_pos_;

	return Open(filename, mode);
}

bool VueceMediaStream::Open(const std::string& filename, const char* mode) {

	size_t i;
	int ret = 0;
	bool bret = true;
	struct stat file_stats;
	const char *filePath=(const char*)filename.c_str();

	talk_base::Pathname path(filename);

	LOG(LS_VERBOSE) << "VueceMediaStream::Open - file name: " << filename << ", mode = " << mode;

	if(strcmp(mode, (const char*)"hubclient") == 0)
	{
		LOG(LS_VERBOSE) << "VueceMediaStream::Open - In hub client mode, only init receive session.";
		bret = InternalInit(false);
		return bret;
	}

	LOG(LS_VERBOSE) << "VueceMediaStream::Open - In hub server mode.";

	bret = InternalInit(true);

	if (stat(filename.c_str(), &file_stats) != 0)
	{
		LOG(LS_ERROR) << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
		LOG(LS_ERROR) << "VueceMediaStream::Open - Cannot retrieve file size! Returning false";
		return false;
	}

	iStreamData->iFileSize = (std::numeric_limits<size_t>::max)();

	VueceLogger::Debug("VueceMediaStream - Open: opening file: %s, size = %d bytes", filePath, iStreamData->iFileSize);

	//int avformat_open_input(AVFormatContext **ps, const char *filename, AVInputFormat *fmt, AVDictionary **options);
	ret = avformat_open_input(&iStreamData->pFormatCtx, (const char*)filePath, NULL, NULL);

	VueceLogger::Debug("VueceMediaStream::Open - avformat_open_input returned: %d", ret);

	if(ret!=0)
	{
		VueceLogger::Fatal("VueceMediaStream::Open - Cannot open this file.");
		return false; // Couldn't open file
	}

	VueceLogger::Debug("VueceMediaStream::Open - File successfully opened.");

	// Retrieve stream information
	if(av_find_stream_info(iStreamData->pFormatCtx)<0)
	{
		VueceLogger::Error("VueceMediaStream::Open - Couldn't find stream information.");
		return false; // Couldn't find stream information
	}

	VueceLogger::Debug("VueceMediaStream - ========== Dump Media File Info Start =========================");
	av_dump_format(iStreamData->pFormatCtx, 0, (const char*)filePath, FALSE);
	VueceLogger::Debug("VueceMediaStream - ========== Dump Media File Info End =========================");

	// Find the first audio stream
	iStreamData->targetAudioStreamIdx=-1;
	iStreamData->targetVideoStreamIdx=-1;

	VueceLogger::Debug("VueceMediaStream::Open - Number of streams found in this file: %d", iStreamData->pFormatCtx->nb_streams);
	VueceLogger::Debug("VueceMediaStream::Open - Locating audio stream");

	for(i=0; i<iStreamData->pFormatCtx->nb_streams; i++)
	{
		if(iStreamData->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			iStreamData->targetAudioStreamIdx=i;
			break;
		}
	}

	if(iStreamData->targetAudioStreamIdx==-1)
	{
		VueceLogger::Fatal("VueceMediaStream::Open - Didn't find a audio stream.");
		return false; // Didn't find a audio stream
	}

	VueceLogger::Debug("VueceMediaStream::Open - Audio stream located, now locating video stream");

	for(i=0; i<iStreamData->pFormatCtx->nb_streams; i++)
	{
		if(iStreamData->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			iStreamData->targetVideoStreamIdx=i;
			break;
		}
	}

	if(iStreamData->targetVideoStreamIdx==-1)
	{
		VueceLogger::Debug("VueceMediaStream::Open - Didn't find a video stream.");
	}

	// Get a pointer to the codec context for the audio stream
	iStreamData->pTargetAudioStream = iStreamData->pFormatCtx->streams[iStreamData->targetAudioStreamIdx];
	iStreamData->pAudioCodecCtx = iStreamData->pFormatCtx->streams[iStreamData->targetAudioStreamIdx]->codec;

	VueceLogger::Debug("VueceMediaStream::Open - Audio stream located at idx: %d, \
codec name = %s, sample rate = %d, \
frame size = %d,  frame bits = %d,  frame number  = %d, \
bit rate = %d, bits_per_raw_sample = %d, bits_per_coded_sample = %d, \
channels = %d, time base(num) = %d, time base(den) = %d",
			iStreamData->targetAudioStreamIdx,
			iStreamData->pAudioCodecCtx->codec_name,
			iStreamData->pAudioCodecCtx->sample_rate,
			iStreamData->pAudioCodecCtx->frame_size,
			iStreamData->pAudioCodecCtx->frame_bits,
			iStreamData->pAudioCodecCtx->frame_number,
			iStreamData->pAudioCodecCtx->bit_rate,
			iStreamData->pAudioCodecCtx->bits_per_raw_sample,
			iStreamData->pAudioCodecCtx->bits_per_coded_sample,
			iStreamData->pAudioCodecCtx->channels,
			iStreamData->pFormatCtx->streams[iStreamData->targetAudioStreamIdx]->time_base.num,
			iStreamData->pFormatCtx->streams[iStreamData->targetAudioStreamIdx]->time_base.den);

	//Now we can initialize some global parameters
	//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	iStreamData->iAudioSampleRate = iStreamData->pAudioCodecCtx->sample_rate;
	iStreamData->iNChannels = iStreamData->pAudioCodecCtx->channels;
	iStreamData->iBitDepth = 16; //hard-coded for now

	VueceLogger::Debug("VueceMediaStream::Open - Codec name is: %s", avcodec_get_name(iStreamData->pAudioCodecCtx->codec_id));
	VueceLogger::Debug("VueceMediaStream::Open - Sample format value is: %d", (int) iStreamData->pAudioCodecCtx->sample_fmt);
	VueceLogger::Debug("VueceMediaStream::Open - Sample format is: %s", av_get_sample_fmt_name(iStreamData->pAudioCodecCtx->sample_fmt));

	VueceLogger::Debug("VueceMediaStream::Open - Bit number per sample: %d", av_get_bits_per_sample(iStreamData->pAudioCodecCtx->codec_id));


#ifdef LOCAL_DECODE_TEST
	pTestDecodeCodec = avcodec_find_decoder(CODEC_ID_AAC);
	pTestCodecCtx = avcodec_alloc_context3(pTestDecodeCodec);

	pTestCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
//	pTestCodecCtx->sample_fmt = iStreamData->pAudioCodecCtx->sample_fmt;

	//set default values, they will be updated in later SET methods
	pTestCodecCtx->sample_rate = iStreamData->pAudioCodecCtx->sample_rate;
	pTestCodecCtx->channels = iStreamData->pAudioCodecCtx->channels;

	//this is hard-coded for now
//	pTestCodecCtx->bit_rate = iStreamData->pAudioCodecCtx->bit_rate;

	test_outbuf =(int16_t*)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	if(avcodec_open(pTestCodecCtx, pTestDecodeCodec)<0)
	{
		VueceLogger::Fatal("LOCAL_DECODE_TEST - open_codec:Cannot open AAC codec!");
	}


#endif


	/**
	 * For stereo and 16 bits audio, the audio bytes per seconds is: 44100 * 16 * 2 / 8 = 176400
	 * so for mono audio with the same setting the value will be 176400 / 2 = 88200
	 */
	iStreamData->lTotalAudioBytesConsumedPerSecond =
			iStreamData->iAudioSampleRate *
			iStreamData->iBitDepth *
			iStreamData->iNChannels / 8;

	VueceLogger::Debug("VueceMediaStream - player_init: lTotalAudioBytesConsumedPerSecond is updated to  %ld bytes",
			iStreamData->lTotalAudioBytesConsumedPerSecond);


	if(iStreamData->pAudioCodecCtx->codec_id == CODEC_ID_AAC)
	{
		VueceLogger::Debug("VueceMediaStream::Open - Stream is in AAC format.");
	}
	else if(iStreamData->pAudioCodecCtx->codec_id == CODEC_ID_MP3)
	{
		VueceLogger::Debug("VueceMediaStream::Open - Stream is in MP3 format.");
	}
	else if(iStreamData->pAudioCodecCtx->codec_id == CODEC_ID_MP2)
	{
		VueceLogger::Debug("VueceMediaStream::Open - Stream is in MP2 format.");
	}

	if(iStreamData->pAudioCodecCtx->codec_id == CODEC_ID_MP3 || iStreamData->pAudioCodecCtx->codec_id == CODEC_ID_MP2)
	{
		int tmp;

		VueceLogger::Debug("VueceMediaStream::Open - Stream is MP3/MP2 format, we need transcoding.");

		//why???
//		iStreamData->iMP3RawFrameBytes = iStreamData->pAudioCodecCtx->frame_size * 2 * 2;
		iStreamData->iMP3RawFrameBytes = iStreamData->pAudioCodecCtx->frame_size * 2 * iStreamData->pAudioCodecCtx->channels;

		VueceLogger::Debug("VueceMediaStream::Open - Stream is MP3 format, raw frame length is: %d", iStreamData->iMP3RawFrameBytes);

		if(iStreamData->iMP3RawFrameBytes == 2304)
		{
			iStreamData->iAACRawFrameBytes = 2048;//2048;
		}
		else if(iStreamData->iMP3RawFrameBytes == 4608)
		{
			iStreamData->iAACRawFrameBytes = 4096;
		}
		else
		{
			VueceLogger::Fatal("VueceMediaStream::Open - This raw frame length is not supported!Abort now.");
//			return false;
		}

		//handle special cases
		if(iStreamData->pAudioCodecCtx->sample_rate == 22050 && iStreamData->pAudioCodecCtx->channels == 2)
		{
			iStreamData->iAACRawFrameBytes = 4096;

			LOG(LS_VERBOSE) << "VueceMediaStream::Open - Sample rate is 22050 and nchannel is 2, iAACRawFrameBytes is set to 4096";
		}

		if(iStreamData->pAudioCodecCtx->sample_rate == 22050 && iStreamData->pAudioCodecCtx->channels == 1)
		{
			iStreamData->iAACRawFrameBytes = 2048;

			LOG(LS_VERBOSE) << "VueceMediaStream::Open - Sample rate is 22050 and nchannel is 1, iAACRawFrameBytes is set to 2048";
		}

		VueceLogger::Debug("VueceMediaStream::Open - Stream is MP3 format, AAC raw frame length is updated to: %d", iStreamData->iAACRawFrameBytes);

		double tmp2 = (double)iStreamData->iAACRawFrameBytes / iStreamData->lTotalAudioBytesConsumedPerSecond;
		iStreamData->iFrameDurationInMs = tmp2 * 1000;

		VueceLogger::Debug("VueceMediaStream::Open - iFrameDurationInMs = %d ms",
				iStreamData->iFrameDurationInMs);


		// Find the decoder for the audio stream
		iStreamData->pAudioTranscodeDec = avcodec_find_decoder(iStreamData->pAudioCodecCtx->codec_id);
		if(iStreamData->pAudioTranscodeDec == NULL)
		{
			VueceLogger::Fatal("VueceMediaStream::Open - MP3 decoder not found");
			return false; // Codec not found
		}

		VueceLogger::Debug("VueceMediaStream::Open - MP3 decoder located.");

		if( avcodec_open(iStreamData->pAudioCodecCtx, iStreamData->pAudioTranscodeDec ) < 0 )
		{
			VueceLogger::Fatal("VueceMediaStream::Open - Cannot open MP3 encoder!");
			return false;
		}

		VueceLogger::Debug("VueceMediaStream::Open - MP3 decoder successfully opened.");

		// Prepare encoder
		VueceLogger::Debug("VueceMediaStream::Open - Preparing AAC encoder.");

		iStreamData->pAudioTranscodeEnc = avcodec_find_encoder(CODEC_ID_AAC);
		if(iStreamData->pAudioTranscodeEnc == NULL)
		{
			VueceLogger::Fatal("VueceMediaStream::Open - AAC encoder not found");
			return false; // Codec not found
		}

		iStreamData->pAudioTranscodeEncCtx = avcodec_alloc_context();
		if(iStreamData->pAudioTranscodeEncCtx == NULL)
		{
			VueceLogger::Fatal("VueceMediaStream::Open - Cannot allocate encoder context!");
			return false;
		}

		//assign encoder parameters based on the original codec context
		iStreamData->pAudioTranscodeEncCtx->sample_rate = iStreamData->pAudioCodecCtx->sample_rate;
		iStreamData->pAudioTranscodeEncCtx->channels = iStreamData->pAudioCodecCtx->channels;
		iStreamData->pAudioTranscodeEncCtx->bit_rate = iStreamData->pAudioCodecCtx->bit_rate;
		iStreamData->pAudioTranscodeEncCtx->sample_fmt = iStreamData->pAudioCodecCtx->sample_fmt;
		iStreamData->pAudioTranscodeEncCtx->profile = FF_PROFILE_AAC_MAIN;//iStreamData->pAudioCodecCtx->profile;
		iStreamData->pAudioTranscodeEncCtx->codec_id = CODEC_ID_AAC;
		iStreamData->pAudioTranscodeEncCtx->codec_type = AVMEDIA_TYPE_AUDIO;

		tmp = (iStreamData->pAudioTranscodeEncCtx->channels * av_get_bits_per_sample(iStreamData->pAudioTranscodeEncCtx->codec_id));

		LOG(LS_VERBOSE) << "VueceMediaStream::Open - Encoder ctx allocated, frame size = " << iStreamData->pAudioTranscodeEncCtx->frame_size;

//		iStreamData->pAudioTranscodeEncCtx->frame_size = 1024;


		tmp = av_samples_get_buffer_size( NULL,
				iStreamData->pAudioTranscodeEncCtx->channels,
				iStreamData->pAudioCodecCtx->frame_size,
				iStreamData->pAudioTranscodeEncCtx->sample_fmt,
				1);

		LOG(LS_VERBOSE) << "VueceMediaStream::Open - av_samples_get_buffer_size returnedd frame size = " << tmp;

		//TODO - We need to make it adaptable in the future, currently simply
		//return false then terminate the session if buffer size is not 4096.
		//When a song has mono configuration, the buffer size is 2048, 64kb/s
		//we need to let both size know the audio config before streaming is started
		if(tmp != 4096)
		{
			VueceLogger::Error("VueceMediaStream::Open - AAC sample size is not 4096 actual sample size: %d", tmp);

//			av_free(iStreamData->pAudioTranscodeEncCtx);
//
//			iStreamData->pAudioTranscodeEncCtx = NULL;
////			talk_base::Break();
//			return false;
		}

		if( avcodec_open(iStreamData->pAudioTranscodeEncCtx, iStreamData->pAudioTranscodeEnc) < 0 )
		{
			VueceLogger::Fatal("VueceMediaStream::Open - Cannot open AAC encoder!");
			return false;
		}

		VueceLogger::Debug("VueceMediaStream::Open - AAC Encoder successfully opened.");

	}

	LOG(INFO) << "VueceMediaStream::Open operation was successful.";

	//NOTE - We only seek audio frame for now
	if(iStartPosSec > 0)
	{
		LOG(INFO) << "VueceMediaStream::Open - Start position is > 0, seek target frame at first, call av_rescale now.";

		// Convert time into frame number

		int64_t	desiredFrameNumber = av_rescale(
				iStartPosSec*1000,
				iStreamData->pTargetAudioStream->time_base.den,
				iStreamData->pTargetAudioStream->time_base.num);

		LOG(INFO) << "VueceMediaStream::Open - av_rescale returned with frame number: " << desiredFrameNumber;

		desiredFrameNumber/=1000;

		if(avformat_seek_file(
				iStreamData->pFormatCtx,
				iStreamData->targetAudioStreamIdx,
				0,
				desiredFrameNumber,
				desiredFrameNumber,
				AVSEEK_FLAG_ANY)<0
				)
		{
			VueceLogger::Fatal("FATAL ERROR!!! VueceMediaStream::Open - avformat_seek_file failed, sth is wrong!");
			return false;
		}
		else
		{
			LOG(INFO) << "VueceMediaStream::Open - avformat_seek_file returned OK.";
		}

	}

	iStreamState = SS_OPEN;

	return true;
}

bool VueceMediaStream::OpenShare(const std::string& filename, const char* mode,
		int shflag) {

	VueceLogger::Fatal( "VueceMediaStream::OpenShare - Not supported!");

	return false;
}

bool VueceMediaStream::DisableBuffering() {

	LOG(LS_ERROR) << "VueceMediaStream::DisableBuffering - Not supported";

	return false;
}

StreamState VueceMediaStream::GetState() const {

	VueceLogger::Fatal( "VueceMediaStream::GetState - Note supported.");

	return iStreamState;
}



StreamResult VueceMediaStream::Read(
		void* buffer,
		size_t buffer_len,
		size_t* read,
		int* error
		)
{

	AVPacket packet;
	int encodedAACFrameLen = 0;
	int pos = 0;
	int threshold = 1024;
	int decLen, resultSizeBytes, i;

	char* p = (char*)buffer;

//	VueceLogger::Debug("VueceMediaStream::Read[SID: %s] - Target length: %d", session_id.c_str(), buffer_len);

	//NOTE - the buffer length MUST be bigger than header length, this is
	//the minimum requirement, we should not split header itself into chunks
	if(buffer_len < iStreamData->iFrameHeaderLen)
	{
//		LOG(LS_VERBOSE) << "VueceMediaStream::Read - Available buffer is smaller than header length, return 0 bytes read.";
		*read = 0;
		return SR_SUCCESS;
	}

	if(bIsAllDataConsumed)
	{
		*read = 0;
		VueceLogger::Debug("********** VueceMediaStream::Read - All data consumed!");
		return SR_EOS;
	}

	if(iStreamData->bIsReadingBigVideoFrame && iStreamData->bIsReadingBigAudioFrame)
	{
		VueceLogger::Fatal("VueceMediaStream - FATAL ERROR!!! Read - Impossible case!");
		return SR_ERROR;
	}

	//we don't read next frame from file unless we finish writing current big frame
	if(iStreamData->bIsReadingBigVideoFrame || iStreamData->bIsReadingBigAudioFrame)
	{
		size_t restDataLen = iStreamData->lBigFrameLen - iStreamData->lCurrentBigFrameReadPos;

//		LOG(LS_VERBOSE)
//		<< "VueceMediaStream::Read - Copying big frame, big frame len = " << iStreamData->lBigFrameLen
//		<< ", lCurrentBigFrameReadPos = " << iStreamData->lCurrentBigFrameReadPos
//		<< ", data remaining = " << restDataLen;

		if(restDataLen <= buffer_len)
		{
//			LOG(LS_VERBOSE) << "VueceMediaStream::Read - Copy of this frame can be finished, buffer is big enough";

			memcpy(p, iStreamData->iBigFrameBuf + iStreamData->lCurrentBigFrameReadPos, restDataLen);
			*read = restDataLen;

			iStreamData->lCurrentBigFrameReadPos = 0;
			iStreamData->lBigFrameLen = 0;

			if(iStreamData->bIsReadingBigVideoFrame)
			{
				iStreamData->bIsReadingBigVideoFrame = false;
				iStreamData->lTotoalVideoFrameCounter++;
			}

			if(iStreamData->bIsReadingBigAudioFrame)
			{
				iStreamData->bIsReadingBigAudioFrame = false;
				iStreamData->iTotalAudioFrameCounter++;
			}

			return SR_SUCCESS;
		}
		else
		{
//			LOG(LS_VERBOSE) << "VueceMediaStream::Read - Buffer is not big enough, send next chunk.";

			//send next chunk
			memcpy(p, iStreamData->iBigFrameBuf + iStreamData->lCurrentBigFrameReadPos, buffer_len);
			*read = buffer_len;

			iStreamData->lCurrentBigFrameReadPos += buffer_len;

//			LOG(LS_VERBOSE) << "VueceMediaStream::Read - One chunk copied, lCurrentBigFrameReadPos = " << iStreamData->lCurrentBigFrameReadPos;

			return SR_SUCCESS;
		}
	}

	//start reading frame
	while(true)
	{
//		LOG(LS_VERBOSE) << "VueceMediaStream::Reading frame loop 1.";

		//Note the video frame/packet could be very big!
		if(av_read_frame(iStreamData->pFormatCtx, &packet)>=0)
		{

//			LOG(LS_VERBOSE) << "VueceMediaStream::Reading frame loop 2";

//			VueceLogger::Debug("One packet has been read, size = %d, idx = %d, dts = %lld, duration = %d, pts = %lld",
//					packet.size, packet.stream_index ,
//					packet.dts, packet.duration ,
//					packet.pts);


			if(packet.stream_index == iStreamData->targetAudioStreamIdx)
			{

				//this cannot happen because (we suppose) audio frame is always smaller then 1024 bytes
				if(packet.size > buffer_len)
				{
					//This is warning, not an error, because at this point we don't know how big the transacoded frame is
//					VueceLogger::Warn("VueceMediaStream - Warning!!! Audio frame size(%d) is bigger than target read length(%d)",
//							packet.size, buffer_len);

					//NOTE(JJ) - set *read -> 0 and return SR_SUCCESS will not work here, in this case we only need to
					//decode and this packet and send part of it.
					//DO NOT ENABLE FOLLOWING CODE!
//					*read = 0;
//					return SR_SUCCESS;
				}

				//double check this flag, we assume audio packet will only be written once, we NEVER write audio packet
				//by multiple chunks
				if(iStreamData->bIsReadingBigVideoFrame)
				{
					VueceLogger::Fatal("VueceMediaStream - FATAL ERROR!!! - Still reading big frame but we got a audio packet, abort now.");
					return SR_ERROR;
				}

//				LOG(LS_VERBOSE) << "VueceMediaStream::Reading frame loop 3";

				//If codec context for transcode is not empty, then we need to do transcode
				if(iStreamData->pAudioTranscodeEncCtx != NULL )
				{
					resultSizeBytes = AVCODEC_MAX_AUDIO_FRAME_SIZE;

//					LOG(LS_VERBOSE) << "VueceMediaStream::Calling avcodec_decode_audio3";

					/**
					 * Expected decoded data size:
					 * Mono 		- 2304 bytes (1 channel)
					 * Stereo 	- 4608 bytes (2 channels)
					 */
					decLen = avcodec_decode_audio3(iStreamData->pAudioCodecCtx,  (int16_t*) (iStreamData->pAudioDecOutBuf), &resultSizeBytes, &packet);

//					LOG(LS_VERBOSE) << "VueceMediaStream::Calling avcodec_decode_audio3 returned with result size: " << resultSizeBytes;

					//resultSize must be 4608/2304 for MP3 stream
					if(resultSizeBytes != iStreamData->iMP3RawFrameBytes)
					{
						VueceLogger::Error("VueceMediaStream - ERROR!!! Decode resultSize is not (%d)! Actual value: %d, continue and read next packet", iStreamData->iMP3RawFrameBytes, resultSizeBytes);
//						return SR_ERROR;
//						continue;
					}

//					LOG(LS_VERBOSE) << "VueceMediaStream::Reading frame loop 4";

					i = av_fifo_generic_write(iStreamData->pAudioEncodeFifo, iStreamData->pAudioDecOutBuf, resultSizeBytes, NULL);
					//comment out to avoid massive trace output - enable for debugging only
//					VueceLogger::Debug("TRANSCODE av_fifo_generic_write returned: %d", i);

//					LOG(LS_VERBOSE) << "VueceMediaStream::Start transcoding to AAC with chunk size: " << iStreamData->iAACRawFrameBytes;

					while(av_fifo_size(iStreamData->pAudioEncodeFifo) >= iStreamData->iAACRawFrameBytes) //2048/4096
					{

						av_fifo_generic_read(   iStreamData->pAudioEncodeFifo,
												iStreamData->pAudioOutBufTranscoded,
												iStreamData->iAACRawFrameBytes,
												NULL);

						resultSizeBytes -= iStreamData->iAACRawFrameBytes;

						encodedAACFrameLen = avcodec_encode_audio(
								iStreamData->pAudioTranscodeEncCtx, //the codec context
								iStreamData->pTmpBuf, // the output buffer
								1024, //the output buffer size
								(short*)iStreamData->pAudioOutBufTranscoded // the input buffer containing the samples
						);

						//comment this out to avoid massive trace output
//						VueceLogger::Debug("TRANSCODE - Encoded AAC frame length: %d", encodedAACFrameLen);

#ifdef LOCAL_DECODE_TEST
						AVPacket pkt;
						av_init_packet(&pkt);
						pkt.data = iStreamData->pTmpBuf;
						pkt.size = encodedAACFrameLen;
						int decLen = -1;
						int resultSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;//iStreamData->iAACRawFrameBytes;

						decLen = avcodec_decode_audio3(pTestCodecCtx, (int16_t *)test_outbuf, &resultSize, &pkt);

						if(decLen <= 0)
						{
//							VueceLogger::Fatal("VUECE AAC DECODER - avcodec_decode_audio3 returned a negative value: %d", decLen);
						}

						VueceLogger::Debug("VUECE AAC DECODER -  Number of bytes decompressed: %d, result data size: %d ", decLen, resultSize);
#endif


						//create frame header (length = 9 bytes) in following format:
						//[SignalByte][FrameLen][FrameTS][DATA]

						if(encodedAACFrameLen <= 0)
						{
							VueceLogger::Fatal("VueceMediaStream - TRANSCODE - FATAL ERROR!!  - No data was encoded.");
						}
						else
						{

							//NOTE - We don't actually know what the maximum encoded frame length is
							//the value of VUECE_ENCODE_OUTPUT_BUFFER_SIZE is determined based on
							//tests

							if(encodedAACFrameLen >= VUECE_ENCODE_OUTPUT_BUFFER_SIZE)
							{
								VueceLogger::Fatal("VueceMediaStream - TRANSCODE - FATAL ERROR!! Encoded AAC frame is too long: %d.", encodedAACFrameLen);
							}

							p[pos++] = VUECE_STREAM_PACKET_TYPE_AUDIO;

							//4 bytes header for frame length
							p[pos++] = (encodedAACFrameLen >> 24) & 0xFF;
							p[pos++] = (encodedAACFrameLen >> 16) & 0xFF;
							p[pos++] = (encodedAACFrameLen >> 8) & 0xFF;
							p[pos++] = encodedAACFrameLen & 0xFF;

							p[pos++] = (iStreamData->iCurrentTimeStamp >> 24) & 0xFF;
							p[pos++] = (iStreamData->iCurrentTimeStamp >> 16) & 0xFF;
							p[pos++] = (iStreamData->iCurrentTimeStamp >> 8) & 0xFF;
							p[pos++] = iStreamData->iCurrentTimeStamp & 0xFF;

							if(encodedAACFrameLen > buffer_len - pos)
							{
//								VueceLogger::Warn("-------------------------------- WARNING ---------------------------------------");
//								VueceLogger::Warn("Reading audio frame(transcode), encodedAACFrameLen = %d, availableBufLen = %d, need to send by chunks", encodedAACFrameLen, (buffer_len - pos));
//								VueceLogger::Warn("-------------------------------- WARNING ---------------------------------------");

								ASSERT(!iStreamData->bIsReadingBigAudioFrame);

								//reset related flags
								iStreamData->bIsReadingBigAudioFrame = true;
								iStreamData->lCurrentBigFrameReadPos = 0;
								iStreamData->lBigFrameLen = encodedAACFrameLen;

								memcpy(iStreamData->iBigFrameBuf, iStreamData->pTmpBuf, encodedAACFrameLen);

								memcpy(p + pos, iStreamData->pTmpBuf, (buffer_len - pos));

								iStreamData->lCurrentBigFrameReadPos += (buffer_len - pos);

								*read = buffer_len;

//								VueceLogger::Debug("Reading audio frame, lCurrentBigFrameReadPos = %ld", iStreamData->lCurrentBigFrameReadPos);

								iStreamData->iCurrentTimeStamp += iStreamData->iFrameDurationInMs;

								//return from here, no further work to do because the buffer is already consumed completely
								return SR_SUCCESS;
							}
							else
							{
								
								if(pos + encodedAACFrameLen > buffer_len)
								{
									LOG(LERROR) << "VueceMediaStream - FATAL ERROR! - Buffer overflow occurred, pos = "
											<< pos << ", encodedAACFrameLen = " << encodedAACFrameLen
											<< ", buffer_len = " << buffer_len;
								}

								memcpy(p + pos, iStreamData->pTmpBuf, encodedAACFrameLen);

								pos += encodedAACFrameLen;

								iStreamData->iAudioBytesRead += encodedAACFrameLen;
								iStreamData->iTotalAudioFrameCounter++;
								iStreamData->iCurrentTimeStamp += iStreamData->iFrameDurationInMs;

								//enable for debugging only
//								VueceLogger::Debug("One transcoded frame has been written into target buffer, current length = %d, buffer size = %d", pos, buffer_len);

								if(pos > buffer_len)
								{
									VueceLogger::Fatal("VueceMediaStream - FATAL ERROR! - Sth is wrong, buffer overflow occurred.");
								}

								if(buffer_len - pos <= 1024)
								{
//									LOG(LS_VERBOSE) << "VueceMediaStream::Audio Read - Threshold reached, stop read frame now.";

									*read = pos;
									return SR_SUCCESS;
								}
							}

						}
					}//end while loop transcoding
				}
				else
				{
					//copy original data if transcode is not needed

					if(pos != 0)
					{
						LOG(LS_WARNING) << "VueceMediaStream::Audio Read - Sth is wrong, pos = " << pos << ", should be zero";
						//abort
						ASSERT(pos == 0);
					}

					p[pos++] = VUECE_STREAM_PACKET_TYPE_AUDIO;

					//4 bytes header for frame length
					p[pos++] = (packet.size >> 24) & 0xFF;
					p[pos++] = (packet.size >> 16) & 0xFF;
					p[pos++] = (packet.size >> 8) & 0xFF;
					p[pos++] = packet.size & 0xFF;

					//timestamp
					p[pos++] = (iStreamData->iCurrentTimeStamp >> 24) & 0xFF;
					p[pos++] = (iStreamData->iCurrentTimeStamp >> 16) & 0xFF;
					p[pos++] = (iStreamData->iCurrentTimeStamp >> 8) & 0xFF;
					p[pos++] = iStreamData->iCurrentTimeStamp & 0xFF;

					if(packet.size > buffer_len)
					{
						VueceLogger::Debug("Reading audio frame, packet size = %d, availableBufLen = %d, need to send by chunks", packet.size, (buffer_len - pos));

						ASSERT(!iStreamData->bIsReadingBigAudioFrame);

						//reset related flags
						iStreamData->bIsReadingBigAudioFrame = true;
						iStreamData->lCurrentBigFrameReadPos = 0;

						if(packet.size > VUECE_MAX_PACKET_SIZE)
						{
							VueceLogger::Fatal("VueceMediaStream - audio packet size is too big, abort now!");
							return SR_EOS;
						}

						iStreamData->lBigFrameLen = packet.size;

						memcpy(iStreamData->iBigFrameBuf, packet.data, packet.size);

						memcpy(p + pos, packet.data, (buffer_len - pos));

						iStreamData->lCurrentBigFrameReadPos += (buffer_len - pos);

						*read = buffer_len;

						VueceLogger::Debug("Reading audio frame, lCurrentBigFrameReadPos = %ld", iStreamData->lCurrentBigFrameReadPos);

						iStreamData->iCurrentTimeStamp += iStreamData->iFrameDurationInMs;

						//return from here, no further work to do because the buffer is already consumed completely
						return SR_SUCCESS;
					}
					else
					{
						//copy the whole frame data because the buffer is sufficient
						memcpy(p + pos, packet.data, packet.size);

						pos += packet.size;

						iStreamData->iAudioBytesRead += packet.size;
						iStreamData->iTotalAudioFrameCounter++;
						iStreamData->iCurrentTimeStamp += iStreamData->iFrameDurationInMs;

	//    				VueceLogger::Debug("VueceMediaStream::Read - One audio frame copied into buffer, pos = %d, ts = %u", pos, iStreamData->iCurrentTimeStamp);
					}

				}

//				LOG(LS_VERBOSE) << "VueceMediaStream::Audio Read - available Buf Len = " << (buffer_len - pos);

				//Note we suppose an audio packet is not bigger than 1024 bytes, we NEVER send chunked audio frame
				if(buffer_len - pos <= 1024)
				{
//					LOG(LS_VERBOSE) << "VueceMediaStream::Audio Read - Threshold reached, stop read frame now.";

					*read = pos;
					return SR_SUCCESS;
				}

			}
			else if(packet.stream_index == iStreamData->targetVideoStreamIdx)
			{

				bool testFlag = true;

				if(testFlag)
				{
					continue;
				}


				if(iStreamData->bIsReadingBigVideoFrame)
				{
					VueceLogger::Fatal("VueceMediaStream - FATAL ERROR!!! - Just read an video packet but bIsReadingBigVideoFrame is still true, abort now!");
					return SR_ERROR;
				}

				//write frame header at first
				p[pos++] = VUECE_STREAM_PACKET_TYPE_VIDEO;

				//two bytes header for frame length
				p[pos++] = (packet.size >> 24) & 0xFF;
				p[pos++] = (packet.size >> 16) & 0xFF;
				p[pos++] = (packet.size >> 8) & 0xFF;
				p[pos++] = packet.size & 0xFF;

				//timestamp
				p[pos++] = (iStreamData->iCurrentTimeStamp >> 24) & 0xFF;
				p[pos++] = (iStreamData->iCurrentTimeStamp >> 16) & 0xFF;
				p[pos++] = (iStreamData->iCurrentTimeStamp >> 8) & 0xFF;
				p[pos++] = iStreamData->iCurrentTimeStamp & 0xFF;

				//if video packet is bigger than available buffer, we need to send it
				//via multiple chunks

				LOG(LS_VERBOSE) << "VueceMediaStream::Read a video frame - available buf len = " << (buffer_len - pos);

				if(packet.size > buffer_len - pos)
				{
					VueceLogger::Debug("Reading video frame, packet size = %d, availableBufLen = %d, need to send by chunks", packet.size, (buffer_len - pos));

					ASSERT(!iStreamData->bIsReadingBigVideoFrame);

					//reset related flags
					iStreamData->bIsReadingBigVideoFrame = true;
					iStreamData->lCurrentBigFrameReadPos = 0;

					if(packet.size > VUECE_MAX_PACKET_SIZE)
					{
						VueceLogger::Fatal("VueceMediaStream - video packet size is too big, abort now!");
						return SR_EOS;
					}

					iStreamData->lBigFrameLen = packet.size;

					memcpy(iStreamData->iBigFrameBuf, packet.data, packet.size);

					memcpy(p + pos, packet.data, (buffer_len - pos));

					iStreamData->lCurrentBigFrameReadPos += (buffer_len - pos);

					*read = buffer_len;

					VueceLogger::Debug("Reading video frame, lCurrentBigFrameReadPos = %ld", iStreamData->lCurrentBigFrameReadPos);

					//return from here, no further work to do because the buffer is already consumed completely
					return SR_SUCCESS;
				}
				else
				{
					VueceLogger::Debug("Available buffer is big engouh, the whole video frame can be copied");

					memcpy(p + pos, packet.data, packet.size);

					pos += packet.size;

					iStreamData->lTotoalVideoFrameCounter++;

					if(buffer_len - pos <= 1024)
					{
						LOG(LS_VERBOSE) << "VueceMediaStream::Read - Threshold reached, stop read frame now, avalaible buf len = " << (buffer_len - pos);

						*read = pos;
						return SR_SUCCESS;
					}
				}

			}
			else
			{
				continue;
			}
		}
		else
		{
			LOG(LS_INFO) << "VueceMediaStream::Read - Stream end reached, total audio frame count = "
					<< iStreamData->iTotalAudioFrameCounter << ", total video frame count = " << iStreamData->lTotoalVideoFrameCounter;

			//send a signal packet with frame len = 1
			int len = 1;

			//send a signal/empty packet
			p[pos++] = VUECE_STREAM_PACKET_TYPE_EOF;

			p[pos++] = (len >> 24) &0xFF;
			p[pos++] = (len >> 16) &0xFF;
			p[pos++] = (len >> 8) &0xFF;
			p[pos++] = len & 0xFF;

			p[pos++] = 0;
			p[pos++] = 0;
			p[pos++] = 0;
			p[pos++] = 0;

			p[pos++] = 0;

			*read = pos;

			bIsAllDataConsumed = true;
			return SR_SUCCESS;
		}

	} //end of while

	VueceLogger::Fatal("VueceMediaStream::Read - Something must be wrong!");

	//Something must be wrong if this line is reached.
	return SR_ERROR;
}

static int byteArrayToInt(uint8_t* b)
{
	int i = 0;
    int value = 0;
    for (i = 0; i < 4; i++) {
        int shift = (4 - 1 - i) * 8;
        value += (b[i] & 0x000000FF) << shift;
    }
    return value;
}

/*
 * Note -
 * 1. This is an interface method, it's called from HttpBase::ProcessData(),
 * which is from another thread, so the resource must be protected by mutex
 * 2. it's possible that when this method is being called, VueceMediaStream is being destroyed
 * by another thread, in this case we must use a flag to avoid being called
 * if the class instance is being finalized.
 */
StreamResult VueceMediaStream::Write(const void* data, size_t data_len,
		size_t* written, int* error) {

#ifdef ANDROID

	int pos = 0;
	int frame_len = 0;
	int sig = 0;
	unsigned long ts = 0;

//	VueceLogger::Debug("VueceMediaStream::Write - START");

//	LOG(LS_VERBOSE) << "VueceMediaStream::Write:Input buffer len = "
//			<< data_len << ", current internal buffer write pos = " << iStreamData->iBufferLen;

	if(iStreamData->bIsDownloadCompleted)
	{
		LOG(LS_VERBOSE) << "VueceMediaStream::Write:Download is completed, no more data is needed, return SR_EOS.";
		return SR_EOS;
	}

	if(data_len + iStreamData->iBufferLen > VUECE_MAX_BUFFER_SIZE)
	{
		VueceLogger::Fatal( "VueceMediaStream::Write:Cannot copy input buffer because internal buffer will overflow!" );
		return SR_ERROR;
	}

	VueceThreadUtil::MutexLock(&mutex_wait_session_release);

	if(!bAllowWrite)
	{
		LOG(LS_WARNING) << "VueceMediaStream::Write - Note allowed now.";

		VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

		return SR_ERROR;
	}

	VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

	//copy data into buffer at first
	memcpy(iStreamData->iBuffer + iStreamData->iBufferLen, data, data_len);
	iStreamData->iBufferLen += data_len;

	//then read data frame by frame, write frames into chunk file
	while(true)
	{

//		LOG(LS_VERBOSE)
//				<< "VueceMediaStream::Write:Current header start position = "
//				<< iStreamData->iFrameHeaderStartPos << ", input buffer len = " << data_len;

		//check length at first, return if buffer is full and frame is not complete
		if(iStreamData->iFrameHeaderStartPos + iStreamData->iFrameHeaderLen >= iStreamData->iBufferLen)
		{
//			LOG(LS_WARNING) << "VueceMediaStream::Write:Frame is not complete (A), wait for next write";
//			LOG(LS_WARNING) << "VueceMediaStream::iFrameHeaderStartPos = " << iStreamData->iFrameHeaderStartPos;
//			LOG(LS_WARNING) << "VueceMediaStream::iBufferLen = " << iStreamData->iBufferLen;

		    memmove(iStreamData->iBuffer,
		    		iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos,
		    		iStreamData->iBufferLen - iStreamData->iFrameHeaderStartPos);

		    iStreamData->iBufferLen -= iStreamData->iFrameHeaderStartPos;

		    //remember the position of the frame header so we can find it at the next write
			iStreamData->iFrameHeaderStartPos = 0;

		    //we need to tell the caller all input buffer has been written
		    *written = data_len;

		    return SR_SUCCESS;
		}

		//otherwise we got at least a complete frame header
		sig = iStreamData->iBuffer[iStreamData->iFrameHeaderStartPos];

		if(sig == VUECE_STREAM_PACKET_TYPE_EOF)
		{
			size_t mediaDur = -1;
			int l = -1;
			GetTimePositionInSecond(&mediaDur);


			LOG(LS_VERBOSE) << "//////////////////////////////////////////////////////////////";
			LOG(LS_VERBOSE) << "//////////////////////////////////////////////////////////////";
			LOG(LS_VERBOSE) << "VueceMediaStream::Write - received termination signal, mediaDur = " << mediaDur << ",  download is finished.";
			LOG(LS_VERBOSE) << "VueceMediaStream::Write - Total audio frames = " <<  iStreamData->iTotalAudioFrameCounter;
			LOG(LS_VERBOSE) << "//////////////////////////////////////////////////////////////";
			LOG(LS_VERBOSE) << "//////////////////////////////////////////////////////////////";

			*written = data_len;
			iStreamData->bIsDownloadCompleted = true;

			//BBB close file handle
			LOG(LS_VERBOSE) << "VueceMediaStream::Write - Close active chunk file handle now.";

			if(iStreamData->fActiveAudioChunkFile != NULL)
			{
				fclose(iStreamData->fActiveAudioChunkFile);
				iStreamData->fActiveAudioChunkFile = NULL;

				LOG(LS_VERBOSE) << "VueceMediaStream::Write - Active chunk file closed";
			}


			//////
#ifndef VUECE_APP_ROLE_HUB
			LOG(LS_VERBOSE) << "VueceMediaStream::Write - Stream is finished,  populate stream data termination info.";

			VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(iStreamData->iLastAvailableAudioChunkFileIdx);

			VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(iStreamData->iAudioFrameCounterInCurrentChunk);

			VueceGlobalContext::SetDownloadCompleted(true);

			VueceLogger::Debug("TROUBLESHOOTING 3 - iStreamData V = %d, Global V = %d", iStreamData->iLastAvailableAudioChunkFileIdx,
					VueceGlobalContext::GetLastAvailableAudioChunkFileIdx());

			//notify vuece bumper that the last index of the available chunk file
			if(VueceStreamPlayer::HasStreamEngine())
			{
				VueceStreamPlayer::InjectBumperTerminationInfo(
						iStreamData->iLastAvailableAudioChunkFileIdx,
						VueceGlobalContext::GetAudioFrameCounterInCurrentChunk()
						);

				//inject the final duration of this meida file
				//Note - Need to update the value of the total duration queried because they might not match
				VueceStreamPlayer::InjectStreamTerminationPosition(mediaDur, true);

				//trigger some post-processing when current streaming is completed
				VueceStreamPlayer::OnStreamingCompleted();
			}

#endif
			return SR_EOS;
		}
		else if(sig == VUECE_STREAM_PACKET_TYPE_AUDIO)
		{
//			VueceLogger::Debug("Receiving an audio packet.");
			iStreamData->bIsReceivingAudioPacket = true;
		}
		else if(sig == VUECE_STREAM_PACKET_TYPE_VIDEO)
		{
			VueceLogger::Debug("Receiving a video packet.");
			iStreamData->bIsReceivingAudioPacket = false;
		}
		else
		{
			VueceLogger::Fatal("VueceMediaStream::Write - unknown vuece stream packet type: %d, abort now.", sig);
		}

		frame_len = byteArrayToInt(iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos+1);

		ts = byteArrayToInt(iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos+5);

		//set the start time
		if(iStreamData->bReceivingFirstFrame)
		{
			iStreamData->bReceivingFirstFrame = false;
			VueceLogger::Debug("VueceMediaStream::Write - we got the first frame header, start position is: %lu ms", ts);

			ASSERT( ts > 0 );

			VueceLogger::Debug("VueceMediaStream::Write - first frame's timestamp is %lu ms > 0", ts);

			iStreamData->iFirstFramePosSec = ts/1000;

			// DO NOT trigger InjectFirstFramePosition() if we are still in the same PLAY SESSION
			if(VueceStreamPlayer::StillInTheSamePlaySession())
			{
				VueceLogger::Debug("VueceMediaStream::Write - We are streaming a buffer window now, and we are still in the same play session, no need to inject first frame position into audio writer in current session.");
			}
			else
			{

				VueceLogger::Debug("VueceMediaStream::Write - Update first frame position because we are in a new play session now.");

				VueceGlobalContext::SetFirstFramePositionSec(iStreamData->iFirstFramePosSec);

				VueceStreamPlayer::LogCurrentStreamingParams();

				VueceStreamPlayer::InjectFirstFramePosition(iStreamData->iFirstFramePosSec);
			}

		}

		//a frame should not be bigger than VUECE_MAX_FRAME_SIZE bytes
		if(iStreamData->bIsReceivingAudioPacket && frame_len > VUECE_MAX_FRAME_SIZE)
		{
			VueceLogger::Fatal("VueceMediaStream::Write:Audio frame is too long(%d), sth must wrong.", frame_len);
			*written = 0;
			return SR_ERROR;
		}

		if(frame_len <= 0)
		{
			VueceLogger::Fatal("VueceMediaStream::Write:Wrong frame length!");
			*written = 0;
			return SR_ERROR;
		}


//		LOG(LS_VERBOSE) << "VueceMediaStream::Write:Current header position = " << iStreamData->iFrameHeaderStartPos << ", Next frame len = " << frame_len;

		//forward write position by iStreamData->iFrameHeaderLen bytes
		iStreamData->iFrameHeaderStartPos += iStreamData->iFrameHeaderLen;

		//check if we can get a complete frame
		if(iStreamData->iFrameHeaderStartPos + frame_len > iStreamData->iBufferLen)
		{
//			LOG(LS_VERBOSE) << "VueceMediaStream::Write:Frame is not complete (B), wait for next write";

			iStreamData->iFrameHeaderStartPos -= iStreamData->iFrameHeaderLen;

		    memmove(iStreamData->iBuffer,
		    		iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos,
		    		iStreamData->iBufferLen - iStreamData->iFrameHeaderStartPos);

		    iStreamData->iBufferLen -= iStreamData->iFrameHeaderStartPos;
		    iStreamData->iFrameHeaderStartPos =  0;

		    *written = data_len;

			return SR_SUCCESS;
		}


		//Enable this when receiving and decoding are both OK
		//////////////////////////////////////////////////////////////////////////////////////

		//write the whole frame including the length header to the active chunk file
		if(iStreamData->bIsReceivingAudioPacket)
		{
			WriteAudioFrameToChunkFile(
							iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos - iStreamData->iFrameHeaderLen,
							frame_len + iStreamData->iFrameHeaderLen,
							iStreamData);
		}
		else
		{
			write_video_frame_to_chunk_file(
							iStreamData->iBuffer + iStreamData->iFrameHeaderStartPos - iStreamData->iFrameHeaderLen,
							frame_len + iStreamData->iFrameHeaderLen,
							iStreamData);
		}

		if(BUFFER_WINDOW_ENABLED)
		{
			if(iStreamData->bBufWindowFull)
			{
				//UUUUUUUUU
				LOG(LS_VERBOSE) << "VueceMediaStream::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
				LOG(LS_VERBOSE) << "VueceMediaStream::Write:Buffer window is full, return SR_EOS, current session will be terminated.";
				LOG(LS_VERBOSE) << "VueceMediaStream::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";

				iStreamData->bBufWindowFull = false;

				return SR_EOS;
			}
		}

		//move to the next frame
		iStreamData->iFrameHeaderStartPos += frame_len;

		//check if we have reached the end of the buffer
		if(iStreamData->iFrameHeaderStartPos == iStreamData->iBufferLen)
		{
//			LOG(LS_VERBOSE) << "VueceMediaStream::Write:Buffer end reached, internal buffer has been totally consumed.";

			iStreamData->iFrameHeaderStartPos 	= 0;
			iStreamData->iBufferLen = 0;

			 *written = data_len;

			return SR_SUCCESS;
		}

		//just double check but this should not happen
		if(iStreamData->iFrameHeaderStartPos > iStreamData->iBufferLen)
		{
			VueceLogger::Fatal("VueceMediaStream::Write:Something is wrong, write position is beyond buffer boundary!");
			*written = 0;
			return SR_ERROR;
		}
	}

	*written = data_len;

	return SR_SUCCESS;
#else
	return SR_ERROR;
#endif
}

void VueceMediaStream::Close()
{

	LOG(LS_VERBOSE) << "VueceMediaStream::Close";

#ifdef ANDROID
	VueceThreadUtil::MutexLock(&mutex_wait_session_release);

	bAllowWrite = false;

	LOG(LS_VERBOSE) << "VueceMediaStream::Close - Write is NOT allowed anymore.";

	VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);
#endif

	InternalRelease();
}

bool VueceMediaStream::GetTimePositionInSecond(size_t* position) const
{
	iStreamData->iCurrentFramePositionSec = iStreamData->iTotalAudioFrameCounter *
			iStreamData->iAACRawFrameBytes / iStreamData->lTotalAudioBytesConsumedPerSecond;

//	LOG(LS_VERBOSE) << "VueceMediaStream::GetTimePositionInSecond: iTotalAudioFrameCounter = " << iStreamData->iTotalAudioFrameCounter
//			<< ", iAACRawFrameBytes = " << iStreamData->iAACRawFrameBytes
//			<< ", lTotalAudioBytesConsumedPerSecond = " << iStreamData->lTotalAudioBytesConsumedPerSecond;

	*position = iStreamData->iCurrentFramePositionSec + iStreamData->iFirstFramePosSec;

//	LOG(LS_VERBOSE) << "VueceMediaStream::GetTimePositionInSecond - returning: " << *position;
	return true;
}

bool VueceMediaStream::SetPosition(size_t position) {
	VueceLogger::Fatal( "VueceMediaStream::SetPosition - Not supported.");
	return false;
}

bool VueceMediaStream::GetPosition(size_t* position) const {
	VueceLogger::Fatal( "VueceMediaStream::GetPosition - Not supported.");
//	*position = iStreamData->iCurrentFramePositionSec;
	*position = iStreamData->iAudioBytesRead;
	LOG(LS_VERBOSE) << "VueceMediaStream::GetPosition: " << *position << " in bytes.";
	return true;
}

bool VueceMediaStream::GetSize(size_t* size) const {

	VueceLogger::Fatal( "VueceMediaStream::GetSize - Not supported.");

	LOG(LS_VERBOSE) << "VueceMediaStream::GetSize: " << iStreamData->iFileSize;
	*size = iStreamData->iFileSize;
	return true;
}

bool VueceMediaStream::GetAvailable(size_t* size) const {

	VueceLogger::Debug( "VueceMediaStream::GetAvailable - Return max size");

	if (size)
	{
//		*size = iStreamData->iFileSize - iStreamData->iAudioBytesRead;
		//always return max value
		*size = iStreamData->iFileSize;
		LOG(LS_VERBOSE) << "VueceMediaStream::GetAvailable = " << *size;
	}

	return true;
}

bool VueceMediaStream::ReserveSize(size_t size) {
	// TODO: extend the file to the proper length
	//VueceLogger::Fatal( "VueceMediaStream::ReserveSize - Note supported.");

	return true;
}

bool VueceMediaStream::GetSize(const std::string& filename, size_t* size) {
	VueceLogger::Fatal(  "VueceMediaStream::GetSize - Not supported.");

	return true;
}

bool VueceMediaStream::Flush() {
	VueceLogger::Fatal( "VueceMediaStream::Flush - Not supported." );

	return false;
}



#if defined(POSIX)

bool VueceMediaStream::TryLock() {
	return false;
}

bool VueceMediaStream::Unlock() {
	return false;
}

#endif

void VueceMediaStream::DoClose() {
	LOG(LS_VERBOSE) << "VueceMediaStream::DoClose";
}

}
