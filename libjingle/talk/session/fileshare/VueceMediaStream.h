/*
 * VueceMediaStream.h
 *
 *  Created on: Jul 14, 2012
 *      Author: Jingjing Sun
 */

#ifndef VUECEMEDIASTREAM_H_
#define VUECEMEDIASTREAM_H_

#include "talk/base/basictypes.h"
#include "talk/base/criticalsection.h"
#include "talk/base/logging.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#ifdef ANDROID
#include "VueceThreadUtil.h"
#endif

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/fifo.h"
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
}


namespace talk_base {


typedef struct _VueceStreamData {
	AVFormatContext *pFormatCtx;
	AVCodecContext *pAudioCodecCtx;
	AVStream* pTargetAudioStream;

	int targetAudioStreamIdx;
	int targetVideoStreamIdx;
	int iAACRawFrameBytes;

	/**
	 * Number of bytes of a decoded mp3 packet read by ffmpeg
	 */
	int iMP3RawFrameBytes;

	AVCodecContext *pAudioTranscodeEncCtx;
	AVCodec *pAudioTranscodeEnc;
	AVCodec *pAudioTranscodeDec;
	int16_t *pAudioDecOutBuf;
	uint8_t *pAudioOutBufTranscoded;
	uint8_t *pTmpBuf;

	AVFifoBuffer *pAudioEncodeFifo;

	size_t iFileSize;
	size_t iAudioBytesRead;

	//current length of the buffer
	int iBufferLen;

	int iFrameHeaderStartPos;

	/*
	 * a counter used to count the number of received frames, if it
	 * reaches a predefined value - VUECE_AUDIO_FRAMES_PER_CHUNK, current active chunk
	 * file will be closed and a new chunk file will be create to save
	 * incoming frames, meanwhile the counter will be reset to 0.
	 */
	int iAudioFrameCounterInCurrentChunk;


	/*
	 * A variable used to remember the id of the latest available chunk
	 */
	int iLastAvailableAudioChunkFileIdx;

	/*
	 * A variable used to remember the id of the first available chunk file id
	 * in current buffer window
	 */
	int iFirstAudioChunkFileIdxInBufWindow;

	int iReceivedVideoFrameCounter;
	int iCurrentVideoChunkFileIdx;

	uint8_t* iBuffer;
	uint8_t* iBigFrameBuf;

	FILE* fActiveAudioChunkFile;
	FILE* fActiveVideoChunkFile;

	/*
	 * A variable used to remember the total number of received frames
	 */
	long iTotalAudioFrameCounter;

	long lTotoalVideoFrameCounter;
	long lTotalAudioBytesConsumedPerSecond;
	long iCurrentFramePositionSec;
	int iCurrentTimeStamp;
	size_t iFrameHeaderLen;

	int iAudioSampleRate;
	int iNChannels;
	int iBitRate; //not used for now
	int iBitDepth;
	int iDuration;
	int iFrameDurationInMs;

	bool bBufWindowFull;
	bool bIsDownloadCompleted;
	bool bIsReceivingAudioPacket;

	bool bIsReadingBigAudioFrame;
	bool bIsReadingBigVideoFrame;

	long lCurrentBigFrameReadPos;
	long lBigFrameLen;

	bool bReceivingFirstFrame;

	/*
	 * This variable is used for streaming/download progress calculation, see function
	 * VueceMediaStream::GetTimePositionInSecond() for more details, the return value of
	 * this function will be reflected on UI player (the streaming progress bar)
	 *
	 * It's also injected to audio writer and used by audio writer to calculate play progress, see
	 * function VueceStreamPlayer::InjectFirstFramePosition() for more details
	 */
	int iFirstFramePosSec;


} VueceStreamData;


class VueceMediaStream: public StreamInterface,  public sigslot::has_slots<> {
public:
	VueceMediaStream(const std::string& session_id_, int sample_rate, int bit_rate, int nchannels, int duration);
	virtual ~VueceMediaStream();

	// The semantics of filename and mode are the same as stdio's fopen
	virtual bool Open(const std::string& filename, const char* mode);
	virtual bool OpenShare(const std::string& filename, const char* mode, int shflag);
	virtual bool Open(const std::string& filename, const char* mode, int start_pos);
	// By default, reads and writes are buffered for efficiency.  Disabling
	// buffering causes writes to block until the bytes on disk are updated.
	virtual bool DisableBuffering();

	virtual StreamState GetState() const;
	virtual StreamResult Read(void* buffer, size_t buffer_len, size_t* read, int* error);
	virtual StreamResult Write(const void* data, size_t data_len, size_t* written, int* error);
	virtual void Close();
	virtual bool SetPosition(size_t position);
	virtual bool GetPosition(size_t* position) const;
	virtual bool GetSize(size_t* size) const;
	virtual bool GetAvailable(size_t* size) const;
	virtual bool ReserveSize(size_t size);

	bool Flush();

#if defined(POSIX)
	// Tries to aquire an exclusive lock on the file.
	// Use OpenShare(...) on win32 to get similar functionality.
	bool TryLock();
	bool Unlock();
#endif

	// Note: Deprecated in favor of Filesystem::GetFileSize().
	static bool GetSize(const std::string& filename, size_t* size);

	bool GetTimePositionInSecond(size_t* position) const;


protected:
	virtual void DoClose();

private:
	bool InternalInit(bool isServer);
	void InternalRelease();
	void WriteAudioFrameToChunkFile(uint8_t* frame, size_t len, VueceStreamData* d);

private:
	StreamState iStreamState;
	bool bIsServer;
	bool bIsAllDataConsumed;
	int iStartPosSec;
	int sample_rate;
	int bit_rate;
	int nchannels;
	int duration;
	bool bAllowWrite;
	std::string session_id;
	VueceStreamData* iStreamData;

#ifdef ANDROID
  JMutex	mutex_wait_session_release;
#endif


private:
	DISALLOW_EVIL_CONSTRUCTORS(VueceMediaStream);
};

} // namespace talk_base

#endif /* VUECEMEDIASTREAM_H_ */
