/*
 * VueceStreamEngine.cc
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#include "talk/base/logging.h"
#include "VueceLogger.h"
#include "VueceStreamEngine.h"
#include "VueceConstants.h"
#include "VueceMediaDataBumper.h"
#include "VueceAACDecoder.h"
#include "VueceAudioWriter.h"
#include "VueceStreamPlayer.h"

#ifndef VUECE_APP_ROLE_HUB
#include "VueceGlobalSetting.h"
#endif

#define THREAD_TAG_STREAM_ENGINE "VueceStreamEngine"

VueceStreamEngine::VueceStreamEngine()
{
	LOG(LS_VERBOSE) << "VueceStreamEngine - Constructor called";
	bumper = NULL;
	decoder = NULL;
	writer = NULL;
	running = false;
	released = false;
	stop_cmd_issued = false;
}

VueceStreamEngine::~VueceStreamEngine()
{
	LOG(LS_VERBOSE) << "VueceStreamEngine - Destructor called";

	if(bumper != NULL) delete bumper;
	LOG(LS_VERBOSE) << "VueceStreamEngine - bumper deleted";

	if(decoder != NULL) delete decoder;
	LOG(LS_VERBOSE) << "VueceStreamEngine - decoder deleted";

	if(writer != NULL) delete writer;
	LOG(LS_VERBOSE) << "VueceStreamEngine - writer deleted";

	LOG(LS_VERBOSE) << "VueceStreamEngine - Destructor Done";

}

bool VueceStreamEngine::Init(
		int sample_rate,
		int bit_rate,
		int channel_num,
		int duration,
		int frame_dur_ms,
		bool startup_standalone,
		bool download_finished,
		int last_avail_chunk_idx,
		int last_chunk_frame_count,
		int resume_pos
		)
{
	bool ret = true;

	VueceLogger::Info("VueceStreamEngine::Init - Start");

	bumper = new VueceMediaDataBumper();
	if(!bumper->Init())
	{
		VueceLogger::Fatal("VueceStreamEngine::Init - bumper cannot be initialized.");
		return false;
	}

	decoder = new VueceAACDecoder();
	if(!decoder->Init(sample_rate, bit_rate, channel_num))
	{
		VueceLogger::Fatal("VueceStreamEngine::Init - decoder cannot be initialized.");
		return false;
	}

	//setup modules
	VueceLogger::Debug("VueceStreamEngine::Init - Setting up bumper");

	bumper->SetFrameDuration(frame_dur_ms);

	if(startup_standalone)
	{
		VueceLogger::Debug("VueceStreamEngine::Init - Started up as a standalone module, last_avail_chunk_idx = %d", last_avail_chunk_idx);

		VueceStreamPlayer::LogCurrentStreamingParams();

		bumper->MarkAsStandalone();
		bumper->SetLastAvailChunkFileIdx(last_avail_chunk_idx);
		bumper->SetFirstFramePosition(VueceGlobalContext::GetFirstFramePositionSec());

		if(download_finished)
		{
			VueceLogger::Debug("VueceStreamEngine::Create - Download finished, last_chunk_frame_count = %d", last_chunk_frame_count);

			bumper->MarkAsCompleted();
			bumper->SetFrameCountOfLastChunk(last_chunk_frame_count);
		}

		VueceLogger::Debug("VueceStreamEngine::Create - resume_pos = %d", resume_pos);

		bumper->SetSeekPosition(resume_pos);
	}

	LOG(LS_VERBOSE) << "VueceStreamEngine::Create  - setting callback on bumper";

	//set state notification callback
	bumper->SignalBumperNotification.connect(this, &VueceStreamEngine::OnBumperExternalEventNotification);

	VueceLogger::Debug("VueceStreamEngine::Init - Setting up bumper - Done");

	//set channel config - stereo for now, we need to inject actual value according to the file info
	//or handshake info from the other side
	int channel_mode = (channel_num==1) ? VUECE_ANDROID_CHANNEL_CONFIGURATION_MONO:VUECE_ANDROID_CHANNEL_CONFIGURATION_STEREO;

	LOG(LS_VERBOSE) << "VueceStreamEngine::Create  - channel_mode is determined: " << channel_mode;

	writer = new VueceAudioWriter();
	if(!writer->Init(channel_mode, channel_num, VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC, duration, sample_rate, resume_pos))
	{
		VueceLogger::Fatal("VueceStreamEngine::Init - writer cannot be initialized.");
		return false;
	}

	LOG(LS_VERBOSE) << "VueceStreamEngine::Create - setting callback on player";
	writer->d->SignalWriterEventNotification.connect(this, &VueceStreamEngine::OnAudioWriterExternalEventNotification);

	VueceLogger::Info("VueceStreamEngine::Init - Writer configuration  - Done.");

	VueceThreadUtil::InitMutex(&mutex_running);
	VueceThreadUtil::InitMutex(&mutex_release);

	released = false;

	return ret;
}

void* VueceStreamEngine::Thread()
{
	VueceLogger::Debug("VueceStreamEngine::Thread - Start");

	VueceMemQueue bumper_q;
	VueceMemQueue decoder_q;
	VueceMemQueue writer_q;

	uint64_t orig = 0;
	int interval = 10; /* in miliseconds*/
	int64_t diff = 0;
	uint64_t realtime = 0;
	uint64_t time = 0;	/* a time since the start of the ticker expressed in milisec*/

	int lastlate = 0;
	int late = 0;
	int loop_counter = 1;

	VueceJni::AttachCurrentThreadToJniEnv(THREAD_TAG_STREAM_ENGINE);

	ThreadStarted();

	VueceLogger::Debug("VueceStreamEngine::Thread - Started");

	orig = VueceThreadUtil::GetCurTimeMs();

	mutex_running.Lock();

	if(stop_cmd_issued)
	{
		VueceLogger::Debug("VueceStreamEngine::Thread - Started but stop command already issued, thread will end now");

		running = false;
	}
	else
	{
		running = true;
	}

	VueceLogger::Debug("VueceStreamEngine::Thread - GetCurTimeMs(orig) = %llu", orig);

	while (running)
	{
		mutex_running.Unlock();

//		VueceLogger::Debug("--------------- VueceStreamEngine PROCESS LOOP %d START ---------------", loop_counter);

		bumper->Process(NULL, &bumper_q);
		decoder->Process(&bumper_q, &decoder_q);
		writer->Process(&decoder_q, NULL);

		time += interval;
		while(1){
			realtime = VueceThreadUtil::GetCurTimeMs() - orig;

			diff = time-realtime;

//			VueceLogger::Debug("VueceStreamEngine::Thread - GetCurTimeMs(realtime) = %llu, time = %llu, diff = %lld", realtime, time, diff);

			if (diff>0)
			{
				/* sleep until next tick */
				VueceThreadUtil::SleepMs((int)diff);
			}
			else
			{
				late = (int) -diff;
				if (late > interval * 5 && late > lastlate)
				{
					VueceLogger::Warn("We are late of %d miliseconds.", late);
				}

				lastlate = late;
				break; /*exit the while loop */
			}
		}

//		VueceLogger::Debug("--------------- VueceStreamEngine PROCESS LOOP %d END ---------------", loop_counter++);

		mutex_running.Lock();
	}

	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamEngine::Thread - Loop exited, releasing resources");
	VueceLogger::Debug("VueceStreamEngine::Thread - bumer_q bulk count: %d", bumper_q.BulkCount());
	VueceLogger::Debug("VueceStreamEngine::Thread - decoder_q bulk count: %d", decoder_q.BulkCount());
	VueceLogger::Debug("VueceStreamEngine::Thread - writer_q bulk count: %d", writer_q.BulkCount());

	//make all memory bulks are release in all queues
	bumper_q.FreeQueue();
	decoder_q.FreeQueue();
	writer_q.FreeQueue();

	bumper->Uninit();
	decoder->Uninit();
	writer->Uninit();

//	VueceLogger::Debug("VueceStreamEngine::Thread - Deleting bumper");
//	if(bumper != NULL) delete bumper;
//	LOG(LS_VERBOSE) << "VueceStreamEngine - bumper deleted";
//
//	VueceLogger::Debug("VueceStreamEngine::Thread - Deleting decoder");
//	if(decoder != NULL) delete decoder;
//	LOG(LS_VERBOSE) << "VueceStreamEngine - decoder deleted";
//
//	VueceLogger::Debug("VueceStreamEngine::Thread - Deleting writer");
//	if(writer != NULL) delete writer;
//	LOG(LS_VERBOSE) << "VueceStreamEngine - writer deleted";

//	VueceLogger::Debug("VueceStreamEngine::Thread - Calling pthread_exit");
//
//	pthread_exit(0);


	VueceLogger::Debug("VueceStreamEngine::Thread - Unlock release tag");

	mutex_release.Lock();
	released = true;
	mutex_release.Unlock();

#ifdef ANDROID
	// due to a bug in old Bionic version
	// cleanup of jni manually
	// works directly with Android 2.2

	VueceLogger::Debug("VueceStreamEngine::Thread - Calling AndroidKeyCleanup");

	//TODO - Double check this call, why detach without attach, i think this is not right....
//	VueceJni::AndroidKeyCleanup(NULL);
	VueceJni::ThreadExit(NULL, THREAD_TAG_STREAM_ENGINE);

#endif

	VueceLogger::Debug("VueceStreamEngine::Thread - Stopped");

	return NULL;
}



void VueceStreamEngine::StopSync()
{
	VueceLogger::Debug("VueceStreamEngine::Thread - StopSync");

	mutex_running.Lock();
	stop_cmd_issued = true;
	running = false;
	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamEngine::Thread - StopSync - wait until all resources are released");

	mutex_release.Lock();
	while (!released)
	{
		mutex_release.Unlock();
		VueceThreadUtil::SleepSec(1);
		mutex_release.Lock();
	}
	mutex_release.Unlock();

	VueceLogger::Debug("VueceStreamEngine::Thread - StopSync - Done");
}

void VueceStreamEngine::OnAudioWriterExternalEventNotification(VueceStreamAudioWriterExternalEventNotification *event)
{
	VueceLogger::Debug("VueceStreamEngine - -------------------------------------");
	VueceLogger::Debug("VueceStreamEngine - OnAudioWriterExternalEventNotification: event id =  %d, pass notification to VueceStreamPlayer", event->id);
	VueceLogger::Debug("VueceStreamEngine - -------------------------------------");

	//pass it to player
	VueceStreamPlayer::OnAudioWriterStateNotification(event);

}

void VueceStreamEngine::OnBumperExternalEventNotification(VueceBumperExternalEventNotification *notify)
{

	VueceLogger::Debug("VueceStreamEngine - -------------------------------------");
	VueceLogger::Debug("VueceStreamEngine::OnBumperExternalEventNotification:  %d", notify->event);
	VueceLogger::Debug("VueceStreamEngine - -------------------------------------");

	//WARNING - Be careful to use pVueceMediaStream here, it is already
	//destroyed if the state value is VueceBumperState_Stopped

	switch(notify->event)
	{
	case VueceBumperExternalEvent_SEEK_FINISHED:
	{
		int resume_pos = notify->data;

		VueceLogger::Debug("VueceStreamEngine::OnBumperExternalEventNotification - seek operation has succeeded, resume bumper and player now.");

		//resume player now.
		writer->ResumePlayer(resume_pos, true);
		break;
	}
	case VueceBumperExternalEvent_BUFFERING:
	{
		VueceLogger::Fatal("VueceStreamEngine::OnBumperExternalEventNotification - Cannot handle this event for now:  %d", notify->event);
		break;
	}
	case VueceBumperExternalEvent_DATA_AVAILABLE:
	{
		VueceLogger::Debug("VueceStreamEngine::OnBumperExternalEventNotification - DATA_AVAILABLE, stream player will react.");

		VueceStreamPlayer::OnNewDataAvailable();
		break;
	}
	case VueceBumperExternalEvent_COMPLETED:
	{
		VueceLogger::Debug("VueceStreamEngine::OnBumperExternalEventNotification - Bumper finished pumping data to next module, notify player that all data are available");
		writer->MarkAsAllDataAvailable();
		break;
	}
	default:
	{
		VueceLogger::Fatal("VueceStreamEngine::OnBumperExternalEventNotification - Invalid event:  %d", notify->event);
		break;
	}
	}


}


