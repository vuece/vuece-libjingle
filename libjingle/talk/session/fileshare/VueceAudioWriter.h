/*
 * VueceAudioWriter.h
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#ifndef VUECEAUDIOWRITER_H_
#define VUECEAUDIOWRITER_H_

#include <jni.h>

#include "talk/base/sigslot.h"

#include "VueceConstants.h"

#include "VueceMemQueue.h"
#include "VueceThreadUtil.h"
#include "VueceJni.h"

//mono by default
static int vuece_current_channel_config = VUECE_ANDROID_CHANNEL_CONFIGURATION_MONO;
//stream mode - AUDIO STREAM mode by default
static int vuece_current_stream_mode = VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC;


//VueceAudioWriterFsmState, this is only internal to
//android audio writer and is not used by outside entity
typedef enum _VueceAudioWriterFsmState{
	VueceAudioWriterFsmState_Ready = 0,
	VueceAudioWriterFsmState_Buffering,
	VueceAudioWriterFsmState_Playing,
	VueceAudioWriterFsmState_Paused,
	VueceAudioWriterFsmState_Stopping,
	VueceAudioWriterFsmState_Stopped
}VueceAudioWriterFsmState;

typedef enum _VueceAudioWriterFsmEvent{
	VueceAudioWriterFsmEvent_Start = 0,
	VueceAudioWriterFsmEvent_DataNotAvailable,
	VueceAudioWriterFsmEvent_DataReadyForConsumption,
	VueceAudioWriterFsmEvent_Pause,
	VueceAudioWriterFsmEvent_Resume,
	VueceAudioWriterFsmEvent_Stop,
	VueceAudioWriterFsmEvent_StopCompleted
}VueceAudioWriterFsmEvent;

class VueceAndroidSndData {

public:

	VueceAndroidSndData();
	~VueceAndroidSndData();

public:

	unsigned int	bits;
	unsigned int	rate;
	unsigned int	nchannels;
	bool			started;
	bool 			check_thread_exit_flag;
	bool 			bPostProcessCalled;
	bool 			waiting_for_new_data;

	VueceAudioWriterFsmState player_state;
	pthread_t     writer_thread_id;
	pthread_t     progress_checker_thread_id;

	JMutex	mutex; /** mutex used to protect the access to sound data structure */
	JMutex	audiotrack_mutex;
	JMutex	writer_thread_lock;
	JMutex	mutex_fsm_state;
	JMutex	mutex_play_progress;

	pthread_cond_t writer_thread_cond;
	bool writer_thread_started;

	/*
	 * The position where current play is resumed or started, it's used to calculate
	 * the actual player progress position
	 */
	int iResumePosSec;

	/*
	 *	The position of the first frame received in the same play session, it should be
	 *	updated whenever a new play session is started (probably triggered by a remote
	 *	seek)
	 */
	int iFirstFramePosSec;

	/* The position of the final available frame, it's dynamically injected by VueceMediaStream during
	 * download */
	int iStreamTerminationPosSec;


	bool bAllDataAvailable;
	bool buffer_win_enabled;
	bool download_completed;

	int watchdog_timer;
	bool watchdog_enabled;
	bool resumed_by_local_seek;

	/**
	 * This is a one-time flag used to indicate that an immediate download of
	 * next buffer window is required because user has resumed the playing at
	 * another position and this position is beyond buffer window threshold
	 */
	bool bTriggerNextBufWinDldAfterStart;

	int	buff_size; /*buffer size in bytes*/
};



class VueceAndroidSndWriteData : public VueceAndroidSndData{
public:

	jclass 			audio_track_class;
	jobject			audio_track;
	VueceMemQueue* consumer_q;
	pthread_cond_t		cond;
	int 			write_chunk_size;
	unsigned int	writtenBytes;
	unsigned long 	last_sample_date;
	bool 			sleeping;
	int 			totoal_duration_in_sec;
	jclass 			jabber_client_class;
	jobject 		jabber_client_object;

	jmethodID 		write_id;
	jmethodID 		play_id;
	jmethodID 		pause_id;
	jmethodID 		flush_id;
	jmethodID 		stop_id;
	jmethodID 		get_header_pos_id;
	jmethodID 		set_header_pos_id;
	jmethodID       on_player_progress_id;
	jmethodID       on_player_state_id;
	jmethodID 		get_client_id;

	int current_player_pos;

	sigslot::signal1<VueceStreamAudioWriterExternalEventNotification*> SignalWriterEventNotification;

public:
	VueceAndroidSndWriteData();
	~VueceAndroidSndWriteData();

	bool StateTranstition(VueceAudioWriterFsmEvent event);
	unsigned int getWriteBuffSize() {
		return buff_size;
	}
	int getWrittenFrames() {
		return writtenBytes/(nchannels*(bits/8));
	}

	static void LogFsmState(VueceAudioWriterFsmState state);
	static void LogFsmEvent(VueceAudioWriterFsmEvent event);

	static void GetFsmStateString(VueceAudioWriterFsmState s, char* state_s);
	static void GetFsmEventString(VueceAudioWriterFsmEvent e, char* event_s);
};


class VueceAudioWriter
{
public:
	VueceAudioWriter();
	virtual ~VueceAudioWriter();

	bool Init(int channel_mode, int nr_channels, int stream_mode, int duration, int sample_rate, int resume_pos);
	void Uninit();
	void Process(VueceMemQueue* in_q, VueceMemQueue* out_q);

	void 	SetWriteRate(int proposed_rate);
	int 	GetRate();
	void	SetNchannels(int n);
	void	SetChannelConfig(int channel_config);
	void	SetStreamMode(int n);
	void 	PausePlayer();
	void 	ResumePlayer(int resume_pos, bool resumed_by_local_seek);
	void	SetTotoalDuration(int n);
	void	SetResumePosition(int n);
	void	SetFirstFramePosition(int n);
	void	SetStreamTerminationPosition(int n, bool force_download_complete);
	void 	MarkAsAllDataAvailable();
	void 	EnableBufWin(int enable_flag);
	void 	EnableBufWinDownloadDuringStart(int enable_flag);
	int 	GetCurrentPlayingProgress(void);

	//TODO - give this a proper name later.
	VueceAndroidSndWriteData* d;
};


#endif /* VUECEAUDIOWRITER_H_ */
