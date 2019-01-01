/*
 * VueceAudioWriter.cc
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#include <sys/resource.h>

#include <errno.h>

#include "talk/base/logging.h"
#include "VueceLogger.h"
#include "VueceStreamPlayer.h"
#include "VueceAudioWriter.h"
#include "VueceConstants.h"
#include "VueceConfig.h"
#include "VueceJni.h"

#define MODUE_NAME_AUDIO_WRITER "VueceAudioWriter"

#define THREAD_TAG_AU_WRITER "VueceAudioWriter"
#define THREAD_TAG_AU_PLAYER "VueceAudioPlayer"
#define THREAD_TAG_AU_PROGRESS_CHECKER "VueceAudioProgressChecker"

#define WATCHDOG_TIMEOUT 60

static const float sndwrite_flush_threshold=0.020;	//ms


static void set_high_prio(void){
	/*
		This pthread based code does nothing on linux. The linux kernel has
		sched_get_priority_max(SCHED_OTHER)=sched_get_priority_max(SCHED_OTHER)=0.
		As long as we can't use SCHED_RR or SCHED_FIFO, the only way to increase priority of a calling thread
		is to use setpriority().
	*/
#if 0
	struct sched_param param;
	int result=0;
	memset(&param,0,sizeof(param));
	int policy=SCHED_OTHER;
	param.sched_priority=sched_get_priority_max(policy);
	if((result=pthread_setschedparam(pthread_self(),policy, &param))) {
		VueceLogger::Warn("VueceAudioWriter - Set sched param failed with error code(%i)\n",result);
	} else {
		VueceLogger::Debug("VueceAudioWriter thread priority set to max (%i, min=%i)",sched_get_priority_max(policy),sched_get_priority_min(policy));
	}
#endif
	if (setpriority(PRIO_PROCESS,0,-20)==-1){
		VueceLogger::Warn("VueceAudioWriter set_high_prio() failed: %s",strerror(errno));
	}
}

static void* audio_write_cb(VueceAndroidSndWriteData* d) {

	jbyteArray 	write_buff = 0;

	int return_code=-1;
	jobject listener_object = 0;

	//Vuece - This tmp buffer contains 20ms raw data
	int min_size=-1;
	int count;
	int bufferizer_size = 0;
	//Vuece - max_size is size of raw data for 20ms
	int max_size         = sndwrite_flush_threshold*(float)d->rate*(float)d->nchannels*2.0;
	//Vuece - size of raw data for 3 seconds
	int check_point_size = 3                       *(float)d->rate*(float)d->nchannels*2.0; /*3 seconds*/

	set_high_prio();

	int buff_size = d->write_chunk_size;
	int result = 0;

	uint8_t tmpBuff[buff_size];

	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - Debug 1");

	VueceJni::AttachCurrentThreadToJniEnv(THREAD_TAG_AU_WRITER);

	JNIEnv *jni_env = VueceJni::GetJniEnv(THREAD_TAG_AU_WRITER);

	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - Debug 2");

	write_buff = jni_env->NewByteArray(buff_size);

    return_code = jni_env->CallIntMethod(d->audio_track,jni_env->GetMethodID(d->audio_track_class,"setPositionNotificationPeriod", "(I)I"), d->rate);
	if (return_code != 0) {
		VueceLogger::Fatal("VueceAudioWriter - setPositionNotificationPeriod failed");
		goto end;
	}

	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - Debug 3");


    jni_env->CallVoidMethod(d->audio_track,jni_env->GetMethodID(d->audio_track_class,"setPlaybackPositionUpdateListener", "(Landroid/media/AudioTrack$OnPlaybackPositionUpdateListener;)V"), listener_object);


	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - Debug 4");

	//start playing
	jni_env->CallVoidMethod(d->audio_track,d->play_id);

	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - Debug 5");

	// notify state
    jni_env->CallVoidMethod(d->jabber_client_object, d->on_player_state_id,(jint)VueceAudioWriterFsmState_Playing);

	d->player_state = VueceAudioWriterFsmState_Ready;

	d->started = true;
	d->check_thread_exit_flag = false;
	d->bAllDataAvailable = false;

	VueceLogger::Debug("VueceAudioWriter::audio_write_cb - writer thread started, wake up waiting thread now");

	VueceThreadUtil::MutexLock(&d->writer_thread_lock);
//	VueceThreadUtil::CondSignal(&d->writer_thread_cond);
	d->writer_thread_started = true;
	VueceThreadUtil::MutexUnlock(&d->writer_thread_lock);

	//this will perform one time transition to put FSM into its initial working state: READY ---> BUFFERING
	d->StateTranstition(VueceAudioWriterFsmEvent_Start);

	while(d->started)
	{
		VueceThreadUtil::MutexLock(&d->mutex);
		min_size=-1;
		count=0;

		//consume all data in buffer
		while(true)
		{
//				VueceLogger::Debug("audio_write_cb - reading data, consumer_q size: %d, bulk count: %d, requested data size: %d",
//						d->consumer_q->Size(), d->consumer_q->BulkCount(), d->write_chunk_size);

				bufferizer_size = d->consumer_q->Size();



				if(bufferizer_size < d->write_chunk_size)
				{

					//Note - No need to fire this event if download is already completed
					if( d->StateTranstition(VueceAudioWriterFsmEvent_DataNotAvailable) )
					{
						VueceThreadUtil::MutexLock(&d->audiotrack_mutex);

						VueceLogger::Debug("audio_write_cb - buffer ran out, start wait for new data from bumper.");

						d->waiting_for_new_data = true;

						VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);

						//go to sleep now
						break;
					}
					else
					{
						VueceLogger::Fatal("audio_write_cb - Transition DataNotAvailable is not allowed, sth is wrong.");
					}
				}

				if (min_size==-1) min_size=bufferizer_size;
				else if (bufferizer_size<min_size) min_size=bufferizer_size;

				d->consumer_q->Read(tmpBuff, d->write_chunk_size);

				VueceThreadUtil::MutexUnlock(&d->mutex);

				jni_env->SetByteArrayRegion(write_buff,0,d->write_chunk_size,(jbyte*)tmpBuff);

				//NOTE(Vuece) - This is where data is written to AudioTrack for playing
				VueceThreadUtil::MutexLock(&d->audiotrack_mutex);
				result = jni_env->CallIntMethod(d->audio_track,d->write_id,write_buff,0,d->write_chunk_size);
				VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);

				d->writtenBytes+=result;
				if (result <= 0) {
//					VueceLogger::Error("write operation has failed [%i]",result);
				}

				VueceThreadUtil::MutexLock(&d->mutex);

				count += d->write_chunk_size;

				if (count>check_point_size){
					if (min_size > max_size) {
						//For music streaming, we should not discard any data
						if(vuece_current_stream_mode == VUECE_ANDROID_AUDIO_STREAM_MODE_VOICE_CALL)
						{
							VueceLogger::Warn("~~~~~~~~~~~~~ we are late in voice call, flushing %i bytes ~~~~~~~~~~~~",min_size);
							//TODO - We don't have 'skip' function for now
							//ms_bufferizer_skip_bytes(d->bufferizer,min_size);
						}
						else
						{
							//NOTE - Waiting for new data does not mean the playing is stopped because the play may be playing
							//data that has already been written into its internal buffer, we must use pause() if we want to
							//stop/pause playing
							VueceLogger::Warn("~~~~~~~~~~~~~ we are late in file streaming, start buffering data now ~~~~~~~~~~~~");
						}
					}
					count=0;
				}

				//At the end of each loop, we check if we have received any command that pauses
				//audiotrack playing, we suspend this thread if we have
				VueceThreadUtil::MutexLock(&d->audiotrack_mutex);
				if(d->player_state == VueceAudioWriterFsmState_Paused)
				{
					VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);

					break;
				}
				VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);

		}//end of inner while

		///////////////////////////////////////////////////////////////
		VueceLogger::Debug("audio_write_cb ---------------------------- SUSPEND PLAYING NOW --------------------------------");
		//start waiting for data to be available
		if (d->started)
		{
			d->sleeping=true;
			//why crash here?????

			VueceThreadUtil::CondWait(&d->cond,&d->mutex);

			d->sleeping=false;
		}


		VueceLogger::Debug("audio_write_cb ----------------------- PLAYING RESUMED! ----------------------------");

		VueceThreadUtil::MutexUnlock(&d->mutex);

	}//end of outter while

	VueceLogger::Debug("audio_write_cb - play loop exited!");

	goto end;

	end: {

		if(write_buff != 0)
		{
			VueceLogger::Debug("audio_write_cb - delete local ref write_buff");

			jni_env->DeleteLocalRef(write_buff);
		}

		VueceJni::ThreadExit(NULL, THREAD_TAG_AU_WRITER);
		return NULL;
	}
}

static void* audio_play_progress_checker_cb(VueceAndroidSndWriteData *d)
{
	int current_player_pos = 0;
	int timer = 0;
	int buf_win = 0;

	bool bPlayFinished = false;
	bool bTriggerNextBufWinDld = false;

	VueceLogger::Debug("Starting AudioTrack checker thread...");

	VueceJni::AttachCurrentThreadToJniEnv(THREAD_TAG_AU_PROGRESS_CHECKER);

	JNIEnv *jni_env = VueceJni::GetJniEnv(THREAD_TAG_AU_PROGRESS_CHECKER);

	VueceLogger::Debug("Starting AudioTrack checker thread, waken up.");

	while(true)
	{
		bTriggerNextBufWinDld = false;

		VueceThreadUtil::SleepSec(1);

		if(d->check_thread_exit_flag)
		{
			VueceLogger::Debug("audio_play_progress_checker_cb - ---------------------------------------------------");
			VueceLogger::Debug("audio_play_progress_checker_cb - audio writer exited, exit checker thread now.");
			break;
		}

		if(d->watchdog_enabled)
		{
			d->watchdog_timer++;
		}

		if( !d->bAllDataAvailable )
		{

			if(d->player_state != VueceAudioWriterFsmState_Playing)
			{

				//TODO - MOre comment here.
				if(!d->buffer_win_enabled)
				{
					//if watchdog is enabled but download is already finished, then sth is wrong, we log an error then finish current play
					if(d->download_completed)
					{
						VueceLogger::Error("audio_play_progress_checker_cb - watchdog is enabled but download is finished, sth is wrong, finish current play now.");

						VueceStreamAudioWriterExternalEventNotification event;

						bPlayFinished = true;

						VueceLogger::Debug("audio_play_progress_checker_cb - ****************************************");
						VueceLogger::Debug("audio_play_progress_checker_cb - Play is finished with error, fire notification anyway");
						VueceLogger::Debug("audio_play_progress_checker_cb - ***************************************");

						event.id = VueceStreamAudioWriterExternalEvent_Completed;
						d->SignalWriterEventNotification(&event);

						break;
					}

					VueceLogger::Debug("audio_play_progress_checker_cb - currently not playing(state: %d), progress will not be updated, watchdog = %d",
							d->player_state, d->watchdog_timer);

					//Check watchdog timer, hard-code 1 minute for now
					if(d->watchdog_timer > WATCHDOG_TIMEOUT)
					{
						VueceStreamAudioWriterExternalEventNotification event;
						event.id = VueceStreamAudioWriterExternalEvent_WatchdogExpired;

						bPlayFinished = true;

						VueceLogger::Debug("audio_play_progress_checker_cb - watchdog expired, give up waiting and stop stream player now.");

						d->SignalWriterEventNotification(&event);

						VueceLogger::Debug("audio_play_progress_checker_cb - watchdog expired notification returned.");


						//exit loop
						break;

					}
					else
					{
						continue;
					}
				}
				else
				{
					VueceLogger::Debug("audio_play_progress_checker_cb - currently not playing(state: %d), buffer buffer window is enabled, progress will be updated anyway.", d->player_state);
				}

			}
		}
		else
		{
			// all data are available, so there is no need to check if player_state is in VueceAudioWriterFsmState_Playing state
			VueceLogger::Debug("audio_play_progress_checker_cb - all data are available");
		}

		timer++;

		current_player_pos = timer;

		if( d->iResumePosSec > 0 )
		{
			current_player_pos += d->iResumePosSec;
		}
		else if(  d->iFirstFramePosSec > 0 )
		{
			current_player_pos += d->iFirstFramePosSec;
		}

		VueceThreadUtil::MutexLock(&d->mutex_play_progress);
		d->current_player_pos = current_player_pos;
		VueceThreadUtil::MutexUnlock(&d->mutex_play_progress);

		//TODO - Fix this use dynamic progress
		if(d->iStreamTerminationPosSec <= 0)
		{
			VueceLogger::Debug("audio_play_progress_checker_cb - audio_play_progress_checker_cb: Stream termination position is not set yet, buffer window will not be used");
			buf_win = -1;
		}
		else
		{
			if(d->buffer_win_enabled)
			{
				buf_win = d->iStreamTerminationPosSec - current_player_pos;

				if(buf_win < 0)
				{
					VueceLogger::Fatal("audio_play_progress_checker_cb - buffer window is enabled but width is a negative value: %d, abort now.", buf_win);
				}
			}
			else
			{
				buf_win = -1;
			}

		}

		VueceLogger::Debug("audio_play_progress_checker_cb - current_player_pos = %d, resume pos = %d, first frame pos = %d, total duration = %d, termination pos = %d, buffer window = %d seconds",
				current_player_pos, d->iResumePosSec, d->iFirstFramePosSec, d->totoal_duration_in_sec, d->iStreamTerminationPosSec, buf_win);

		jni_env->CallVoidMethod(d->jabber_client_object,d->on_player_progress_id, current_player_pos);

		if(d->totoal_duration_in_sec == -1)
		{
			VueceLogger::Debug("audio_play_progress_checker_cb: duration is not injected yet, continue loop");
			continue;
		}

		//check buffer window
		//Note - do not use <= to compare otherwise it will trigger multiple duplicated notifications
		//because we are in a loop here

		if(d->bTriggerNextBufWinDldAfterStart)
		{
			d->bTriggerNextBufWinDldAfterStart = false;

			VueceLogger::Debug("audio_play_progress_checker_cb: Next buffer window is required to start now, probably because resume pos is beyond threshold");

			bTriggerNextBufWinDld = true;
		}
		//NOTE - I need to handle a special case here, if a local seek succeeds, the buffer window could directly fall into < VUECE_BUFWIN_THRESHOLD_SEC,
		//in this case following code for downloading next buffer window will never be triggered, so we need to use resumed_by_local_seek...
		else if(buf_win > 0 && buf_win == VUECE_BUFWIN_THRESHOLD_SEC)
		{
			VueceLogger::Debug("audio_play_progress_checker_cb: buffer window threshold(%d seconds) reached, trigger notification", VUECE_BUFWIN_THRESHOLD_SEC);

			bTriggerNextBufWinDld = true;
		}
		else if(d->resumed_by_local_seek)
		{
			d->resumed_by_local_seek = false;

			VueceLogger::Debug("audio_play_progress_checker_cb: resumed_by_local_seek is true, check current buffer window");

			if(buf_win < VUECE_BUFWIN_THRESHOLD_SEC)
			{
				VueceLogger::Debug("audio_play_progress_checker_cb: resumed_by_local_seek is true, buf win is below threshold, trigger download");
				bTriggerNextBufWinDld = true;
			}
		}


		if(bTriggerNextBufWinDld)
		{
			VueceStreamAudioWriterExternalEventNotification event;
			event.id = VueceStreamAudioWriterExternalEvent_BufWindowThresholdReached;
			event.value = d->iStreamTerminationPosSec;

			d->SignalWriterEventNotification(&event);

			//disable buffer window check here, we don't want to trigger another notification while we are already downloading next buffer window
			//buffer window check will be re-enabled when next buffer window is downloaded, see VueceMediaStream::write_audio_frame_to_chunk_file()
			//where VueceStreamPlayer::EnableBufWinCheckInAudioWriter() is called
			d->buffer_win_enabled = false;
			VueceLogger::Debug("audio_play_progress_checker_cb: buffer window check is disabled during download of next buffer window");

			bTriggerNextBufWinDld = false;
		}


		if(current_player_pos >= d->totoal_duration_in_sec
				|| (d->iStreamTerminationPosSec > 0 && current_player_pos >= d->iStreamTerminationPosSec))
		{
//			VueceLogger::Debug("audio_play_progress_checker_cb - current_player_pos = %d, total duration = %d", current_player_pos, d->duration_in_sec);

			if(current_player_pos >= d->totoal_duration_in_sec)
			{
				VueceStreamAudioWriterExternalEventNotification event;

				//if we have reached the final termination point, we sent COMPLETE event to stop current playing anyway.
				VueceLogger::Debug("audio_play_progress_checker_cb - current play progress reached total duration");

				bPlayFinished = true;

				VueceLogger::Debug("audio_play_progress_checker_cb - ****************************************");
				VueceLogger::Debug("audio_play_progress_checker_cb - Play is finished! Fire notification now");
				VueceLogger::Debug("audio_play_progress_checker_cb - ***************************************");

				event.id = VueceStreamAudioWriterExternalEvent_Completed;
				d->SignalWriterEventNotification(&event);

				break;
			}
			else
			{

				if(d->iStreamTerminationPosSec > 0 && current_player_pos >= d->iStreamTerminationPosSec)
				{
					VueceLogger::Debug("audio_play_progress_checker_cb - current play progress reached current termination position");

					//after this transation, state should be changed to BUFFERING and progress
					//will not be updated
					d->StateTranstition(VueceAudioWriterFsmEvent_DataNotAvailable);

					//confirm transation is successful
					if(d->player_state != VueceAudioWriterFsmState_Buffering)
					{
						VueceLogger::Fatal("audio_play_progress_checker_cb - state is not switched to BUFFERING, sth is wrong, abort.");
					}
				}

				VueceStreamAudioWriterExternalEventNotification event;

				VueceLogger::Debug("VueceAudioWriter - Fire notification now");

				if( d->bAllDataAvailable )
				{
					bPlayFinished = true;

					VueceLogger::Debug("*********************************************************");
					VueceLogger::Debug("VueceAudioWriter - Play is finished!");
					VueceLogger::Debug("*********************************************************");

					event.id = VueceStreamAudioWriterExternalEvent_Completed;
					d->SignalWriterEventNotification(&event);
					break;
				}
				else
				{
					event.id = VueceStreamAudioWriterExternalEvent_BufWinConsumed;
					d->SignalWriterEventNotification(&event);

					//in this cause, we should stopped updating progress value because there is no data to play
				}
			}
		}

	}//end of while loop

	if(bPlayFinished)
	{
		VueceLogger::Debug("audio_play_progress_checker_cb - play loop exited, play is finished - waiting for upper layer to stop the stream");
	}
	else
	{
		VueceLogger::Debug("audio_play_progress_checker_cb - progress checker loop exited for other reason(pause)");
	}

	//IMPORTANT NOTE - The following event notification is a blocked call, it returns when the event notification process
	//is finished, this notification finalizes audio stream instance and ends audio writer thread, there is NO need to
	//join this check thread because that will cause audio thread and check thread to wait for each other's death, this
	//is a deadlock

	VueceLogger::Debug("audio_play_progress_checker_cb - AudioTrack checker thread finished.");

	VueceJni::ThreadExit(NULL, THREAD_TAG_AU_PROGRESS_CHECKER);

    return NULL;

}

//class and method definitions
//--------------------------------------
// Sound Data Definition
//--------------------------------------
VueceAndroidSndData::VueceAndroidSndData() : bits(16),rate(8000),nchannels(1),started(false),writer_thread_id(0)
{

	bPostProcessCalled = false;
	bAllDataAvailable = false;
	download_completed = false;

	bTriggerNextBufWinDldAfterStart = false;
	writer_thread_started = false;
	waiting_for_new_data = false;
	iFirstFramePosSec = -1;
	iResumePosSec = -1;
	resumed_by_local_seek = false;
	buffer_win_enabled = false;
	buff_size = 0;
	player_state = VueceAudioWriterFsmState_Stopped;
	iStreamTerminationPosSec = 0;

	check_thread_exit_flag = false;

	watchdog_timer = 0;
	watchdog_enabled = false;

	VueceThreadUtil::InitMutex(&mutex);
	VueceThreadUtil::InitMutex(&audiotrack_mutex);
	VueceThreadUtil::InitMutex(&writer_thread_lock);
	VueceThreadUtil::InitMutex(&mutex_fsm_state);
	VueceThreadUtil::InitMutex(&mutex_play_progress);

	pthread_cond_init(&writer_thread_cond,NULL);
}

VueceAndroidSndData::~VueceAndroidSndData() {

	VueceLogger::Debug("VueceAndroidSndData Destructor - Start");

//	VueceThreadUtil::DestroyMutex(&mutex);
//	VueceLogger::Debug("VueceAndroidSndData Deconstructor - Start");
//
//	VueceThreadUtil::DestroyMutex(&audiotrack_mutex);
//	VueceLogger::Debug("VueceAndroidSndData Deconstructor - Start");
//
//	VueceThreadUtil::DestroyMutex(&writer_thread_lock);
//	VueceLogger::Debug("VueceAndroidSndData Deconstructor - Start");
//
//	VueceThreadUtil::DestroyMutex(&mutex_fsm_state);
//	VueceLogger::Debug("VueceAndroidSndData Deconstructor - Start");
//
//	VueceThreadUtil::DestroyMutex(&mutex_play_progress);

	pthread_cond_destroy(&writer_thread_cond);

	VueceLogger::Debug("VueceAndroidSndData Destructor - Done");
}

VueceAndroidSndWriteData::VueceAndroidSndWriteData() :audio_track_class(0),audio_track(0),write_chunk_size(0),writtenBytes(0),last_sample_date(0),jabber_client_class(0)
{

	VueceLogger::Debug("VueceAndroidSndWriteData constructor - Start");

	write_id = 0;
	play_id = 0;
	pause_id = 0;
	flush_id = 0;
	stop_id = 0;
	get_header_pos_id = 0;
	set_header_pos_id = 0;
	on_player_progress_id = 0;
	on_player_state_id = 0;
	get_client_id = 0;
	jabber_client_object = 0;
	bPostProcessCalled=false;
	iResumePosSec=0;
	iFirstFramePosSec=0;
	iStreamTerminationPosSec=-1;

	sleeping = false;

	totoal_duration_in_sec = -1; //not set
	current_player_pos = 0; // if not updated, use 0 as default

	consumer_q = NULL;

	consumer_q = new VueceMemQueue();

	pthread_cond_init(&cond,0);
	JNIEnv *jni_env = VueceJni::GetJniEnv("VueceAndroidSndWriteData:Constructor");

	audio_track_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("android/media/AudioTrack"));
	if (audio_track_class == 0) {
		VueceLogger::Fatal("VueceAndroidSndWriteData - cannot find  android/media/AudioTrack\n");
		return;
	}

	jabber_client_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("com/vuece/vtalk/android/jni/JabberClient"));
	if (jabber_client_class == 0) {
		VueceLogger::Fatal("VueceAndroidSndWriteData - cannot find  com/vuece/vtalk/android/jni/JabberClient\n");
		return;
	}

	jmethodID hwrate_id = jni_env->GetStaticMethodID(audio_track_class,"getNativeOutputSampleRate", "(I)I");
	if (hwrate_id == 0) {
		VueceLogger::Fatal("VueceAndroidSndWriteData - cannot find  int AudioRecord.getNativeOutputSampleRate(int streamType)");
		return;
	}

	rate = jni_env->CallStaticIntMethod(
			audio_track_class,
			hwrate_id,
//				0 /*STREAM_VOICE_CALL*/
			vuece_current_stream_mode
			);

	if(vuece_current_stream_mode == VUECE_ANDROID_AUDIO_STREAM_MODE_VOICE_CALL)
	{
		VueceLogger::Debug("VueceAndroidSndWriteData - stream mode is VOICE CALL");
	}
	else if(vuece_current_stream_mode == VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC)
	{
		VueceLogger::Debug("VueceAndroidSndWriteData - stream mode is MUSIC");
	}
	else
	{
		VueceLogger::Fatal("VueceAndroidSndWriteData - unknown stream mode: %d",vuece_current_stream_mode);
	}

	VueceLogger::Debug("VueceAndroidSndWriteData - Hardware sample rate is %i",rate);

}


VueceAndroidSndWriteData::~VueceAndroidSndWriteData()
{
	VueceLogger::Debug("VueceAndroidSndWriteData - Destructor called");

	if(consumer_q != NULL)
	{
		consumer_q->FreeQueue();
		delete consumer_q;

		consumer_q = NULL;
	}


	pthread_cond_destroy(&cond);

	if (audio_track_class!=0){
		JNIEnv *env = VueceJni::GetJniEnv("~VueceAndroidSndWriteData");

		VueceLogger::Debug("VueceAndroidSndWriteData - Deleting global JNI refs audio_track_class");
		env->DeleteGlobalRef(audio_track_class);

		VueceLogger::Debug("VueceAndroidSndWriteData - Deleting global JNI refs jabber_client_class");
		env->DeleteGlobalRef(jabber_client_class);

		VueceLogger::Debug("VueceAndroidSndWriteData - Deleting global JNI refs jabber_client_object");
        env->DeleteGlobalRef(jabber_client_object);

        VueceLogger::Debug("VueceAndroidSndWriteData - Deleting global JNI refs Done");
	}

	SignalWriterEventNotification.disconnect_all();

	VueceLogger::Debug("VueceAndroidSndWriteData - Destructor Done");
}


VueceAudioWriter::VueceAudioWriter()
{
	VueceLogger::Debug("VueceAudioWriter - Constructor called");

	d = NULL;

	vuece_current_channel_config 	= VUECE_ANDROID_CHANNEL_CONFIGURATION_MONO;
	vuece_current_stream_mode 		= VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC;
}

VueceAudioWriter::~VueceAudioWriter()
{
	LOG(LS_VERBOSE) << "VueceAudioWriter - Destructor called";

	if( d != NULL)
	{
		VueceLogger::Debug("VueceAudioWriter - Destructor 1");

		delete d;

		VueceLogger::Debug("VueceAudioWriter - Destructor 2");

		d = NULL;

		VueceLogger::Debug("VueceAudioWriter - Destructor 3");
	}

	VueceLogger::Debug("VueceAudioWriter - Destructor Done");
}



bool VueceAudioWriter::Init(int channel_mode, int channel_num, int stream_mode, int duration, int sample_rate, int resume_pos)
{
	bool ret = true;

	jmethodID constructor_id=0;
	int rc;
	jmethodID min_buff_size_id;

	VueceLogger::Debug("VueceAudioWriter::Init - Input: channel_mode = %d, nr_channels = %d, stream_mode = %d, duration = %d, sample_rate = %d, resume_pos = %d",
			channel_mode, channel_num, stream_mode, duration, sample_rate, resume_pos);

	d = NULL;

	d = new VueceAndroidSndWriteData();

	SetChannelConfig(channel_mode);
	SetNchannels(channel_num);
	SetStreamMode(stream_mode);
	SetTotoalDuration(duration);
	SetWriteRate(sample_rate);
	SetResumePosition(resume_pos);


	JNIEnv *jni_env = VueceJni::GetJniEnv("VueceAudioWriter::Init");

	if (d->audio_track_class == 0) {

		VueceLogger::Fatal("VueceAudioWriter::Init - audio_track_class is null, abort now.");

		return false;
	}

	constructor_id = jni_env->GetMethodID(d->audio_track_class,"<init>", "(IIIIII)V");
	if (constructor_id == 0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find  AudioTrack(int streamType, int sampleRateInHz, \
		int channelConfig, int audioFormat, int bufferSizeInBytes, int mode)");
		return false;
	}

	min_buff_size_id = jni_env->GetStaticMethodID(d->audio_track_class,"getMinBufferSize", "(III)I");
	if (min_buff_size_id == 0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find  AudioTrack.getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)");
		return false;
	}


	//Vuece - code
	d->buff_size = jni_env->CallStaticIntMethod(
			d->audio_track_class,
			min_buff_size_id,
			d->rate,
			vuece_current_channel_config,
			2/*  ENCODING_PCM_16BIT */);

	VueceLogger::Debug("VueceAudioWriter::Init - min_buff_size  is [%i]", d->buff_size);

	d->buff_size = d->buff_size * 4;

	//Vuece - raw data of 20ms
	d->write_chunk_size= (d->rate*(d->bits/8)*d->nchannels)*0.02;

	//d->write_chunk_size=d->buff_size;
	if (d->buff_size > 0) {
		VueceLogger::Debug("VueceAudioWriter::Init - Configuring player with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->write_chunk_size);
	} else {
		VueceLogger::Debug("VueceAudioWriter::Init - Cannot configure player with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->write_chunk_size);

		ret = false;

		return false;
	}


	d->get_client_id=jni_env->GetStaticMethodID(d->jabber_client_class,"getInstance", "()Lcom/vuece/vtalk/android/jni/JabberClient;");
	if(d->get_client_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find getInstance method");

		ret = false;

		goto end;
	}

	d->on_player_progress_id = jni_env->GetMethodID(d->jabber_client_class,"onPlayingProgress", "(I)V");
	if(d->on_player_progress_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find onPlayingProgress method");

		ret = false;

		goto end;
	}

	d->on_player_state_id = jni_env->GetMethodID(d->jabber_client_class,"onStreamPlayerStateChanged", "(I)V");
	if(d->on_player_state_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find onStreamPlayerStateChanged method");

		ret = false;

		goto end;
	}

	d->write_id = jni_env->GetMethodID(d->audio_track_class,"write", "([BII)I");
	if(d->write_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.write() method");

		ret = false;

		goto end;
	}
	d->play_id = jni_env->GetMethodID(d->audio_track_class,"play", "()V");
	if(d->play_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.play() method");

		ret = false;

		goto end;
	}

	d->pause_id = jni_env->GetMethodID(d->audio_track_class,"pause", "()V");
	if(d->pause_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.pause() method");

		ret = false;

		goto end;
	}

	d->flush_id = jni_env->GetMethodID(d->audio_track_class,"flush", "()V");
	if(d->flush_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.flush() method");

		ret = false;

		goto end;
	}

	d->stop_id = jni_env->GetMethodID(d->audio_track_class,"stop", "()V");
	if(d->stop_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.stop() method");

		ret = false;

		goto end;
	}

	d->get_header_pos_id = jni_env->GetMethodID(d->audio_track_class,"getPlaybackHeadPosition", "()I");
	if(d->get_header_pos_id==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find AudioTrack.getPlaybackHeadPosition() method");

		ret = false;

		goto end;
	}

	d->jabber_client_object = jni_env->NewGlobalRef(jni_env->CallStaticObjectMethod(d->jabber_client_class, d->get_client_id));
    if (d->jabber_client_object==0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot find jabber_client_object");

		ret = false;

		goto end;
    }

	//Vuece customizations
	d->audio_track =  jni_env->NewObject(d->audio_track_class
			,constructor_id
			,vuece_current_stream_mode/*STREAM_MUSIC*/
			,d->rate
			,vuece_current_channel_config
			,2/*  ENCODING_PCM_16BIT */
			,d->buff_size
			,1/*MODE_STREAM */);

	d->audio_track = jni_env->NewGlobalRef(d->audio_track);
	if (d->audio_track == 0) {
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot instanciate AudioTrack");

		ret = false;

		return false;
	}


	d->bPostProcessCalled=false;

	//initial FSM state
	d->player_state = VueceAudioWriterFsmState_Ready;

	rc = VueceThreadUtil::CreateThread(&d->writer_thread_id, 0, (void*(*)(void*))audio_write_cb, d);
	if (rc){
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot create write thread return code  is [%i]", rc);

		ret = false;

		d->started = false;
		return false;
	}

	VueceLogger::Debug("VueceAudioWriter::Init - Starting AudioTrack writer thread, waiting until writer thread is started.");

	while(true)
	{
		VueceLogger::Debug("VueceAudioWriter::Init - Waiting in loop until writer thread is started.");

		VueceThreadUtil::MutexLock(&d->writer_thread_lock);

		if(d->writer_thread_started)
		{
			VueceLogger::Debug("VueceAudioWriter::Init - writer_thread_started is true, writer thread started, stop waiting now. ");
			VueceThreadUtil::MutexUnlock(&d->writer_thread_lock);
			break;
		}

		VueceThreadUtil::MutexUnlock(&d->writer_thread_lock);

		VueceThreadUtil::SleepSec(1);

	}

//Note: Following code is old implementation, problem is writer thread could release 'writer_thread_cond'
	//before we start waiting
//	VueceThreadUtil::MutexLock(&d->writer_thread_lock);
//
//	VueceLogger::Debug("VueceAudioWriter::Init - Mutex debug a");
//
//	VueceThreadUtil::CondWait(&d->writer_thread_cond, &d->writer_thread_lock);
//
//	VueceLogger::Debug("VueceAudioWriter::Init - Mutex debug b");
//
//	VueceThreadUtil::MutexUnlock(&d->writer_thread_lock);


	VueceLogger::Debug("VueceAudioWriter::Init - Writer thread started, now creating progress checker thread.");

	rc = VueceThreadUtil::CreateThread(&d->progress_checker_thread_id, 0, (void*(*)(void*))audio_play_progress_checker_cb, d);
	if (rc){
		VueceLogger::Fatal("VueceAudioWriter::Init - cannot create audiotrack checker thread return code  is [%i]", rc);

		d->started = false;

		ret = false;

		return false;
	}

	VueceLogger::Debug("VueceAudioWriter::Init - Done");

	end: {
		return ret;
	}

	return ret;
}

void VueceAudioWriter::Uninit()
{
	VueceLogger::Debug("VueceAudioWriter::Uninit - Start, synchronously release all resources");

	jmethodID flush_id=0;
	jmethodID stop_id=0;
	jmethodID release_id=0;
	JNIEnv *jni_env = VueceJni::GetJniEnv("VueceAudioWriter::Uninit");

	VueceLogger::Debug("VueceAudioWriter::Uninit - 1");

	if( !d->StateTranstition(VueceAudioWriterFsmEvent_Stop) )
	{
		VueceLogger::Warn("VueceAudioWriter::Uninit - State transition STOP is not allowed.");
		return;
	}

	if(d->bPostProcessCalled)
	{
		VueceLogger::Debug("VueceAudioWriter::Uninit - already called, do nothing and return.");
		return;
	}

	d->bPostProcessCalled=true;
	d->started=false;
	VueceThreadUtil::MutexLock(&d->mutex);
	VueceLogger::Debug("VueceAudioWriter::Uninit - 2");
	VueceThreadUtil::CondSignal(&d->cond);
	VueceLogger::Debug("VueceAudioWriter::Uninit - 3");
	VueceThreadUtil::MutexUnlock(&d->mutex);


	VueceLogger::Debug("VueceAudioWriter::Uninit - 4 - waiting writer thread to join");

	VueceThreadUtil::ThreadJoin(d->writer_thread_id);

	VueceLogger::Debug("VueceAudioWriter::Uninit - 4 - writer thread ended");

	d->check_thread_exit_flag = true;

	VueceLogger::Debug("VueceAudioWriter::Uninit - 5");

	// flush
	flush_id = jni_env->GetMethodID(d->audio_track_class,"flush", "()V");
	if(flush_id==0) {
		VueceLogger::Error("cannot find AudioTrack.flush() method");
		goto end;
	}

	VueceLogger::Debug("VueceAudioWriter::Uninit - 6");

	if (d->audio_track) {

		VueceLogger::Debug("VueceAudioWriter::Uninit - 7");

		jni_env->CallVoidMethod(d->audio_track,flush_id);

		VueceLogger::Debug("VueceAudioWriter::Uninit - 8");

		//stop playing
		stop_id = jni_env->GetMethodID(d->audio_track_class,"stop", "()V");
		if(stop_id==0) {
			VueceLogger::Error("VueceAudioWriter::Uninit - cannot find AudioTrack.stop() method");
			goto end;
		}

		VueceLogger::Debug("VueceAudioWriter::Uninit - 9");

		jni_env->CallVoidMethod(d->audio_track,stop_id);

		VueceLogger::Debug("VueceAudioWriter::Uninit - 10");

		//release playing
		release_id = jni_env->GetMethodID(d->audio_track_class,"release", "()V");
		if(release_id==0) {
			VueceLogger::Error("VueceAudioWriter::Uninit -cannot find AudioTrack.release() method");
			goto end;
		}

		VueceLogger::Debug("VueceAudioWriter::Uninit - 11");

		jni_env->CallVoidMethod(d->audio_track,release_id);
	}

	VueceLogger::Debug("VueceAudioWriter::Uninit - 12");

	//NOTE - See IMPORTANT NOTE in checker thread
	//wait progress checker thread to join
	VueceLogger::Debug("VueceAudioWriter::Uninit - 13 - waiting checker thread to join");

	VueceThreadUtil::ThreadJoin(d->progress_checker_thread_id);

	VueceLogger::Debug("VueceAudioWriter::Uninit - - checker thread exited");

	goto end;
end: {

	VueceLogger::Debug("VueceAudioWriter::Uninit - delete global ref audio_track");

	if (d->audio_track) jni_env->DeleteGlobalRef(d->audio_track);

	VueceLogger::Debug("VueceAudioWriter::Uninit - 15");

	d->StateTranstition(VueceAudioWriterFsmEvent_StopCompleted);

	return;
	}
}

void VueceAudioWriter::SetWriteRate(int proposed_rate)
{
#ifndef USE_HARDWARE_RATE
	VueceLogger::Debug("VueceAudioWriter - set_write_rate: %d",proposed_rate);
	d->rate=proposed_rate;
#else
/*audioflingler resampling is really bad
we prefer do resampling by ourselves if cpu allows it*/
	VueceLogger::Debug("VueceAudioWriter - set_write_rate: Hardware rate is used. return -1");
#endif
}

int VueceAudioWriter::GetRate()
{
	return d->rate;
}


void	VueceAudioWriter::SetNchannels(int n){
	VueceLogger::Debug("VueceAudioWriter - set_nchannels - %d", n);
	d->nchannels=n;
}


void	VueceAudioWriter::SetChannelConfig(int channel_config)
{
	VueceLogger::Debug("VueceAudioWriter - vuece_set_channel_config %d",channel_config);

	if(channel_config == VUECE_ANDROID_CHANNEL_CONFIGURATION_MONO)
	{
		vuece_current_channel_config = channel_config;
		VueceLogger::Debug("VueceAudioWriter - current channel configuration has been changed to MONO");
		return;
	}
	else if(channel_config == VUECE_ANDROID_CHANNEL_CONFIGURATION_STEREO)
	{
		vuece_current_channel_config = channel_config;
		VueceLogger::Debug("VueceAudioWriter - current channel configuration has been changed to STEREO");
		return;
	}
	else
	{
		VueceLogger::Fatal("VueceAudioWriter - vuece_set_channel_config: unknown channel configuration value: %d", channel_config);
		return;
	}
}

void	VueceAudioWriter::SetStreamMode(int mode)
{
	VueceLogger::Debug("VueceAudioWriter - vuece_set_stream_mode %d",mode);

	if(mode == VUECE_ANDROID_AUDIO_STREAM_MODE_VOICE_CALL)
	{
		vuece_current_stream_mode = mode;
		VueceLogger::Debug("VueceAudioWriter - current stream mode has been changed to VOICE_CALL");
		return;
	}
	else if(mode == VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC)
	{
		vuece_current_stream_mode = mode;
		VueceLogger::Debug("VueceAudioWriter - current stream mode has been changed to MUSIC");
		return;
	}
	else
	{
		VueceLogger::Fatal("VueceAudioWriter - vuece_set_stream_mode: unknown stream mode value: %d", mode);
		return;
	}
}

void VueceAudioWriter::PausePlayer()
{
	VueceLogger::Debug("VueceAudioWriter - PausePlayer");

	JNIEnv *jni_env = VueceJni::GetJniEnv("PausePlayer");

	VueceLogger::Debug("VueceAudioWriter - PausePlayer: call pause() now");

	if(d->StateTranstition(VueceAudioWriterFsmEvent_Pause))
	{
		jni_env->CallVoidMethod(d->audio_track, d->pause_id);
	}

	VueceLogger::Debug("VueceAudioWriter - PausePlayer: pause() called");
}

void VueceAudioWriter::ResumePlayer(int resume_pos, bool resumed_by_local_seek)
{
	VueceLogger::Debug("VueceAudioWriter::ResumePlayer");

	JNIEnv *jni_env = VueceJni::GetJniEnv("ResumePlayer");

	if(!d->StateTranstition(VueceAudioWriterFsmEvent_Resume))
	{
		VueceLogger::Debug("VueceAudioWriter::ResumePlayer - StateTranstition not allowed");
		return;
	}

	VueceLogger::Debug("VueceAudioWriter::ResumePlayer, resume position is set: %d",
			resume_pos);

	d->iResumePosSec = resume_pos;

	if(d->resumed_by_local_seek)
	{
		VueceLogger::Fatal("VueceAudioWriter::ResumePlayer - resumed_by_local_seek should not be true at this point, abort.");
		return;
	}

//	if(resumed_by_local_seek)
//	{
//		d->resumed_by_local_seek = true;
//	}

	VueceThreadUtil::MutexLock(&d->audiotrack_mutex);

	VueceLogger::Debug("VueceAudioWriter::ResumePlayer, call play() on audiotrack");

	jni_env->CallVoidMethod(d->audio_track,d->play_id);
//	d->player_state = VueceAudioWriterFsmState_Playing;

    jni_env->CallVoidMethod(d->jabber_client_object,d->on_player_state_id,(jint)VueceAudioWriterFsmState_Playing);

	VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);

	VueceLogger::Debug("VueceAudioWriter::ResumePlayer, resume player thread");

	VueceThreadUtil::MutexLock(&d->mutex);

	//thread MUST be currently sleeping
	if (!d->sleeping)
	{
		VueceLogger::Debug("VueceAudioWriter::ResumePlayer, player thread is not sleeping.");
	}
	else
	{
		VueceLogger::Debug("VueceAudioWriter::ResumePlayer, player thread is sleeping, wake it up now.");
		VueceThreadUtil::CondSignal(&d->cond);
		VueceThreadUtil::MutexUnlock(&d->mutex);
	}
}

void	VueceAudioWriter::SetTotoalDuration(int duration)
{
	//make sure duration_in_sec is set only once
	if(d->totoal_duration_in_sec != -1)
	{
		VueceLogger::Fatal("FATAL ERROR!!! VueceAudioWriter - SetTotoalDuration: total duration is already set (%d), sth is wrong", d->totoal_duration_in_sec);
	}

	VueceThreadUtil::MutexLock(&d->mutex);

	d->totoal_duration_in_sec = duration;

	VueceThreadUtil::MutexUnlock(&d->mutex);

}

void	VueceAudioWriter::SetResumePosition(int pos)
{

	VueceThreadUtil::MutexLock(&d->mutex);

	d->iResumePosSec = pos;
	d->current_player_pos = pos;

	VueceLogger::Debug("VueceAudioWriter - vuece_set_resume_position: %d(second), current termination position: %d(second)", d->iResumePosSec, d->iStreamTerminationPosSec);

	VueceThreadUtil::MutexUnlock(&d->mutex);

}

void VueceAudioWriter::SetFirstFramePosition(int pos)
{

	VueceThreadUtil::MutexLock(&d->mutex);

	d->iFirstFramePosSec = pos;

	VueceLogger::Debug("VueceAudioWriter - vuece_set_first_frame_position, start position of the first frame is set to: %d(second)", d->iFirstFramePosSec );

	VueceThreadUtil::MutexUnlock(&d->mutex);

}

void VueceAudioWriter::SetStreamTerminationPosition(int pos, bool force_download_complete)
{

	VueceThreadUtil::MutexLock(&d->mutex);

	d->iStreamTerminationPosSec = pos;

	VueceLogger::Debug("VueceAudioWriter - SetStreamTerminationPosition, termination position is set to: %d, total duration is: %d",
			d->iStreamTerminationPosSec, d->totoal_duration_in_sec);

	/*
	 * THis is for error handling purpose - We might meet the situation that bumper stops bumping data into writer for some reason(bug),
	 * writer depends on bumper's notification method call MarkAsAllDataAvailable() to mark the flag 'bAllDataAvailable' to true, in this
	 * error case writer will never be able to mark this flag to true, so if we figure out here that if the download is finished, we
	 * can simply let the progress checker thread finish current problematic song play, in this way, player can continue to play
	 * next song instead of hanging
	 */
	if(d->iStreamTerminationPosSec >= d->totoal_duration_in_sec)
	{
		VueceLogger::Debug("VueceAudioWriter - The newly injected termination position indicates that all data are available, download is completed.");
		d->download_completed = true;
	}

	if(force_download_complete)
	{
		VueceLogger::Debug("VueceAudioWriter - Force download to complete");

		if(d->iStreamTerminationPosSec != d->totoal_duration_in_sec)
		{
			VueceLogger::Debug("VueceAudioWriter - Actual termination position does not match the claimed total duration, update total duration and force to complete anyway");

			d->totoal_duration_in_sec = d->iStreamTerminationPosSec;

			d->download_completed = true;
		}
	}

	VueceThreadUtil::MutexUnlock(&d->mutex);

}

void VueceAudioWriter::MarkAsAllDataAvailable()
{
	VueceLogger::Debug("VueceAudioWriter - mark_as_all_data_available");

	VueceThreadUtil::MutexLock(&d->mutex);
	d->bAllDataAvailable = true;
	VueceThreadUtil::MutexUnlock(&d->mutex);
}

void VueceAudioWriter::EnableBufWin(int enable_flag)
{
	int i = enable_flag;

	VueceThreadUtil::MutexLock(&d->mutex);

	VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win, input is: %d", i );

	if(i == 1)
	{
		d->buffer_win_enabled = true;

		VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win, buffer window is enabled");

	}
	else
	{
		d->buffer_win_enabled = false;

		VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win, buffer window is disabled");

	}

	VueceThreadUtil::MutexUnlock(&d->mutex);

}

void VueceAudioWriter::EnableBufWinDownloadDuringStart(int enable_flag)
{
	int i = enable_flag;

	VueceThreadUtil::MutexLock(&d->mutex);

	VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win_download_during_start, input is: %d", i );

	if(i == 1)
	{
		d->bTriggerNextBufWinDldAfterStart = true;

		VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win_download_during_start, immediate buffer window download is enabled");

	}
	else
	{
		d->bTriggerNextBufWinDldAfterStart = false;

		VueceLogger::Debug("VueceAudioWriter - vuece_enable_buf_win_download_during_start, immediate buffer window download is disabled");

	}

	VueceThreadUtil::MutexUnlock(&d->mutex);

}


int VueceAudioWriter::GetCurrentPlayingProgress(void)
{
	int ret = -1;

	VueceThreadUtil::MutexLock(&d->mutex_play_progress);
	ret = d->current_player_pos;
	VueceThreadUtil::MutexUnlock(&d->mutex_play_progress);

	return ret;

}



void VueceAudioWriter::Process(VueceMemQueue* in_q, VueceMemQueue* out_q)
{
	VueceMemBulk* m = NULL;

	if(in_q->IsEmpty())
	{
//		VueceLogger::Debug("VueceAudioWriter:: Process -Input queue is empty, do nothing and return");
		return;
	}

	if(in_q->BulkCount() > 0)
	{
//		VueceLogger::Debug("VueceAudioWriter:: Process - Input queue bulk count: %d", in_q->BulkCount());
	}

	while ((m = in_q->Remove()) != NULL)
	{
		if (d->started)
		{
			VueceThreadUtil::MutexLock(&d->mutex);

			d->consumer_q->Put(m);

//			VueceLogger::Debug("VueceAudioWriter:: Process - adding bulk to consume_q, current size = %d", d->consumer_q->Size());

			//Vuece - wake up AudioTrack writing thread if it's waiting for data
			if (d->sleeping)
			{

				if( d->StateTranstition(VueceAudioWriterFsmEvent_DataReadyForConsumption) )
				{
					VueceThreadUtil::MutexLock(&d->audiotrack_mutex);

					//fire the event
					VueceStreamAudioWriterExternalEventNotification event;
					event.id = VueceStreamAudioWriterExternalEvent_Playing;

					d->SignalWriterEventNotification(&event);


					if(d->waiting_for_new_data)
					{
						VueceLogger::Debug("VueceAudioWriter:: Process - currently waiting for new data, wake up player thread now.");

						d->waiting_for_new_data = false;

						//TODO - Do state transition here.
						VueceThreadUtil::CondSignal(&d->cond);
					}

					VueceThreadUtil::MutexUnlock(&d->audiotrack_mutex);
				}
			}

//			VueceLogger::Debug("VueceAudioWriter:: Process - added a bulk into consume_q, size = %d", d->consumer_q->Size());

			VueceThreadUtil::MutexUnlock(&d->mutex);
		}
		else
		{
			VueceLogger::Debug("VueceAudioWriter:: Process - Not started yet, abandoning size = %d", d->consumer_q->Size());

			VueceMemQueue::FreeMemBulk(m);
		}
	}
}

bool VueceAndroidSndWriteData::StateTranstition(VueceAudioWriterFsmEvent e)
{
	bool allowed = false;
	char current_state_s[32];
	char new_state_s[32];
	char event_s[32];

	LogFsmEvent(e);

	VueceThreadUtil::MutexLock(&mutex_fsm_state);

	LogFsmState(player_state);

	GetFsmEventString(e, event_s);
	GetFsmStateString(player_state, current_state_s);

	switch(player_state)
	{
	case VueceAudioWriterFsmState_Ready:
	{
		switch(e)
		{
		case VueceAudioWriterFsmEvent_Start:
		{
			allowed = true;
			//one-time transition
			player_state = VueceAudioWriterFsmState_Buffering;

			if(watchdog_enabled)
			{
				VueceLogger::Warn("StateTranstition - watchdog is already enabled, sth is wrong");
			}

			watchdog_enabled = true;
			watchdog_timer = 0;

			VueceLogger::Debug("StateTranstition - watchdog timer is started.");

			break;
		}
		case VueceAudioWriterFsmEvent_Stop:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceAudioWriterFsmState_Buffering:
	{
		switch(e)
		{
		case VueceAudioWriterFsmEvent_DataNotAvailable:
		{
			//already buffering, ignore
			allowed = true;
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		case VueceAudioWriterFsmEvent_DataReadyForConsumption:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Playing;

			VueceLogger::Debug("StateTranstition - Data available, stop watchdog timer");

			watchdog_enabled = false;
			watchdog_timer = 0;

			break;
		}
		case VueceAudioWriterFsmEvent_Pause:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Paused;
			break;
		}
		case VueceAudioWriterFsmEvent_Resume:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Playing;
			break;
		}
		case VueceAudioWriterFsmEvent_Stop:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceAudioWriterFsmState_Playing:
	{
		switch(e)
		{

		case VueceAudioWriterFsmEvent_DataNotAvailable:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Buffering;

			VueceLogger::Debug("StateTranstition - Data not available, Start watchdog timer now.");

			//validate this flag at first
			if(watchdog_enabled)
			{
				VueceLogger::Warn("StateTranstition - watchdog is already enabled, sth is wrong");
			}

			watchdog_enabled = true;
			watchdog_timer = 0;

			VueceLogger::Debug("StateTranstition - watchdog timer is started.");

			break;
		}
		case VueceAudioWriterFsmEvent_DataReadyForConsumption:
		{
			//ignore
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		case VueceAudioWriterFsmEvent_Pause:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Paused;
			break;
		}
		case VueceAudioWriterFsmEvent_Stop:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
		}
		}

		break;
	}
	case VueceAudioWriterFsmState_Paused:
	{
		switch(e)
		{
		//Note this is acceptable because when this happens play might be paused at the same time.
		case VueceAudioWriterFsmEvent_DataNotAvailable:
		{
			//ignore
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		case VueceAudioWriterFsmEvent_DataReadyForConsumption:
		{
			//ignore
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			break;
		}
		case VueceAudioWriterFsmEvent_Resume:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Buffering;
			break;
		}
		case VueceAudioWriterFsmEvent_Stop:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
		}
		}

		break;
	}
	case VueceAudioWriterFsmState_Stopping:
	{
		switch(e)
		{
		case VueceAudioWriterFsmEvent_DataNotAvailable:
		{
			allowed = true;
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
			//ignore
			break;
		}
		case VueceAudioWriterFsmEvent_StopCompleted:
		{
			allowed = true;
			player_state = VueceAudioWriterFsmState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
		}
		}

		break;
	}
	case VueceAudioWriterFsmState_Stopped:
	{
		//this state doesn't accept any event
		VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s);
		break;
	}
	default:
	{
		VueceLogger::Fatal("VueceAndroidSndWriteData::StateTranstition - UNKNOWN state(code: %d)", (int)player_state); break;
	}

	}

	if(allowed)
	{
		GetFsmStateString(player_state, new_state_s);
		VueceStreamPlayer::LogStateTranstion(MODUE_NAME_AUDIO_WRITER, event_s, current_state_s, new_state_s);
	}

	VueceThreadUtil::MutexUnlock(&mutex_fsm_state);

	return allowed;
}

void VueceAndroidSndWriteData::LogFsmState(VueceAudioWriterFsmState s)
{
	switch(s)
	{
	case VueceAudioWriterFsmState_Ready:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - READY"); break;
	case VueceAudioWriterFsmState_Buffering:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - BUFFERING"); break;
	case VueceAudioWriterFsmState_Playing:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - PLAYING"); break;
	case VueceAudioWriterFsmState_Paused:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - PAUSED"); break;
	case VueceAudioWriterFsmState_Stopping:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - STOPPING"); break;
	case VueceAudioWriterFsmState_Stopped:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmState - STOPPED"); break;
	default:
		VueceLogger::Fatal("VueceAndroidSndWriteData::LogFsmState - UNKNOW (code: %d)", (int)s); break;
	}
}

void VueceAndroidSndWriteData::LogFsmEvent(VueceAudioWriterFsmEvent e)
{
	switch(e)
	{
	case VueceAudioWriterFsmEvent_Start:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - START"); break;
	case VueceAudioWriterFsmEvent_DataNotAvailable:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - DATA NOT AVAILABLE"); break;
	case VueceAudioWriterFsmEvent_DataReadyForConsumption:
	{
		//commented out to avoid massive debug output
//		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - DATA READY");
		break;
	}

	case VueceAudioWriterFsmEvent_Pause:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - PAUSE"); break;
	case VueceAudioWriterFsmEvent_Resume:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - RESUME"); break;
	case VueceAudioWriterFsmEvent_Stop:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - STOP"); break;
	case VueceAudioWriterFsmEvent_StopCompleted:
		VueceLogger::Debug("VueceAndroidSndWriteData::LogFsmEvent - STOP COMPLETED"); break;
	default:
		VueceLogger::Fatal("VueceAndroidSndWriteData::LogFsmEvent - UNKNOW (code: %d)", (int)e); break;
	}
}

void VueceAndroidSndWriteData::GetFsmStateString(VueceAudioWriterFsmState s, char* state_s)
{
	switch(s)
	{
	case VueceAudioWriterFsmState_Ready:
		strcpy(state_s, "READY"); return;
	case VueceAudioWriterFsmState_Buffering:
		strcpy(state_s, "BUFFERING"); return;
	case VueceAudioWriterFsmState_Playing:
		strcpy(state_s, "PLAYING"); return;
	case VueceAudioWriterFsmState_Paused:
		strcpy(state_s, "PAUSED"); return;
	case VueceAudioWriterFsmState_Stopping:
		strcpy(state_s, "STOPPING"); return;
	case VueceAudioWriterFsmState_Stopped:
		strcpy(state_s, "STOPPED"); return;
	default:
		VueceLogger::Fatal("VueceAndroidSndWriteData::GetFsmStateString - UNKNOW (code: %d)", (int)s); break;
	}
}
void VueceAndroidSndWriteData::GetFsmEventString(VueceAudioWriterFsmEvent e, char* event_s)
{
	switch(e)
	{
	case VueceAudioWriterFsmEvent_Start:
		strcpy(event_s, "START"); return;
	case VueceAudioWriterFsmEvent_DataNotAvailable:
		strcpy(event_s, "DATA NOT AVAILABLE"); return;
	case VueceAudioWriterFsmEvent_DataReadyForConsumption:
		strcpy(event_s, "DATA READY"); return;
	case VueceAudioWriterFsmEvent_Pause:
		strcpy(event_s, "PAUSE"); return;
	case VueceAudioWriterFsmEvent_Resume:
		strcpy(event_s, "RESUME"); return;
	case VueceAudioWriterFsmEvent_Stop:
		strcpy(event_s, "STOP"); return;
	case VueceAudioWriterFsmEvent_StopCompleted:
		strcpy(event_s, "STOPPED"); return;
	default:
		VueceLogger::Fatal("VueceAndroidSndWriteData::GetFsmEventString - UNKNOW (code: %d)", (int)e); break;
	}
}

