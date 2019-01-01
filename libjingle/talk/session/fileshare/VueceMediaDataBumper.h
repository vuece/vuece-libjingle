/*
 * VueceMediaDataBumper.h
 *
 *  Created on: Oct 31, 2014
 *      Author: jingjing
 */

#ifndef VUECEMEDIADATABUMPER_H_
#define VUECEMEDIADATABUMPER_H_

#include "VueceConstants.h"
#include "talk/base/sigslot.h"
#include "jthread.h"
#include "VueceMemQueue.h"

/**
 * FSM states - internal use only
 */
typedef enum _VueceBumperFsmState{
	VueceBumperState_Ready = 0,
	VueceBumperState_Buffering,
	VueceBumperState_Bumping,
	VueceBumperState_Paused,
	VueceBumperState_Completed,
	VueceBumperState_Stopped,
	VueceBumperState_Err
}VueceBumperFsmState;

/**
 * FSM event - internal use only
 */
typedef enum _VueceBumperFsmEvent{
	VueceBumperFsmEvent_Tick = 0,
	VueceBumperFsmEvent_Pause,
	VueceBumperFsmEvent_Resume,
	VueceBumperFsmEvent_DataBelowThreshold,
	VueceBumperFsmEvent_AllDataConsumed,
	VueceBumperFsmEvent_LocalSeekSucceeded,
	VueceBumperFsmEvent_Stop
}VueceBumperFsmEvent;

typedef struct _VueceMediaBumperData{
	uint8_t* readBuf;
	int iFrameHeaderLen;
	int availBufFileCounter;
	bool bBufferReadable;
	bool bStandaloneFlag;
	int iReadCount;
	FILE* fActiveBufferFile;

	int iActiveBufFileIdx;
	int iLastAvailChunkFileIdx;
	int iFrameCountOfLastChunk;
	long iTotoalFrameCounter;
	VueceBumperFsmState bumperState;
	bool bIsDowloadCompleted;
	bool bIsMergingFiles;
	bool bIsChunkMerged;
	bool bSaveTestFlag; // a test flag for file save, remove later
	bool bTestFlag;

	long lAudioChunkDurationInMs;

	int iFrameDurationInMs;
	int iSeekTargetPosInMs;
	int iFirstFramePosMs;

}VueceMediaBumperData;


class VueceMediaDataBumper
{
public:
	VueceMediaDataBumper();
	virtual ~VueceMediaDataBumper();

	static int ByteArrayToInt(uint8_t* b);

	bool Init();

	void Uninit();

	void Process(VueceMemQueue* in_q, VueceMemQueue* out_q);

	void SetTerminateInfo(int last_avail_chunk_file_idx, int nr_frame_of_last_chunk);
	void OnAllDataConsumed();

	int SetLastAvailChunkFileIdx(int i);
	int SetFrameCountOfLastChunk(int i);
	int SetFrameDuration(int dur);
	int SetSeekPosition(int i);
	int SetFirstFramePosition(int pos);

	int PauseBumper();
	int ResumeBumper();
	int MarkAsCompleted();
	int MarkAsStandalone();

public:
	sigslot::signal1<VueceBumperExternalEventNotification*> SignalBumperNotification;

private:

	JMutex* mutex_bumper_state;
	JMutex* mutex_chunk_idx;
	VueceMediaBumperData* bumper_data;

	VueceMemQueue* out_q;

private:
	void GetChunkFileNameFromIdx(int idx, char* fname);
	bool ActivateBufferFile(VueceMediaBumperData *d);
	int  ReadFrame(VueceMediaBumperData *d);
	void ReadAndQueueFrame(VueceMediaBumperData *d);

	int  SaveFile(VueceMediaBumperData *d);
	int  StreamSeek(VueceMediaBumperData *d);

	bool StateTranstion(VueceBumperFsmEvent e);

	static void GetFsmStateString(VueceBumperFsmState s, char* state_s);
	static void GetFsmEventString(VueceBumperFsmEvent e, char* event_s);
	static void LogFsmState(VueceBumperFsmState state);
	static void LogFsmEvent(VueceBumperFsmEvent state);

};


#endif /* VUECEMEDIADATABUMPER_H_ */
