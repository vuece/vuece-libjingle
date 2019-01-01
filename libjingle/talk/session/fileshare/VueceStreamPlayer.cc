/*
 * VueceStreamPlayer.h
 *
 *  Created on: Sep 14, 2014
 *      Author: jingjing
 */


#include "talk/session/fileshare/VueceShareCommon.h"
#include "talk/p2p/base/constants.h"
#include "talk/base/logging.h"
#include "talk/base/common.h"

#include "VueceMediaStreamSessionClient.h"
#include "VueceStreamPlayerMonitorThread2.h"
#include "VueceStreamPlayer.h"
#include "VueceGlobalSetting.h"
#include "VueceConstants.h"
#include "VueceLogger.h"
#include "VueceStreamEngine.h"

#include "VueceMediaDataBumper.h"
#include "VueceAudioWriter.h"

#include "VueceNetworkPlayerFsm.h"

#include "coffeecatch.h"
#include "coffeejni.h"


#define MODULE_NAME_PLAYER "VueceStreamPlayer"
#define TIMEOUT_WAIT4PLAYER_STOPPED 47

static VueceStreamPlayerMonitorThread2 *player_monitor;
static VuecePlayerFsmState current_fsm_state;
static char current_song_uri[256+1];
static int used_frame_dur_ms;

static VueceStreamEngine* stream_engine;
/*
 * The position of the first frame in current buffer window
 * Note - Local seek is allowed only when the target position
 * falls into current buffer window which is [start_pos_current_buf_win, last frame position]
 * */
static int start_pos_current_buf_win;
static bool notify_end_of_song;
static bool allow_stream;
static JNIEnv* jni_env;
static VueceStreamPlayerStopReason stop_reason;

/*
 * use this flag to remember the last player progress, when playing is resumed, this flag will
 * be used as the start position
 */

static int current_play_progress;

static JMutex mutex_state;
static JMutex mutex_allow_streaming;

void VueceStreamPlayer::OnAudioWriterStateNotification(VueceStreamAudioWriterExternalEventNotification *event)
{
	VueceLogger::Debug("-------------------------------------");
	VueceLogger::Debug("VueceStreamPlayer - OnAudioWriterStateNotification: event id =  %d", event->id);
	VueceLogger::Debug("-------------------------------------");

	switch(event->id)
	{
	//TODO - Add 'PLAYING' state notification here
	case VueceStreamAudioWriterExternalEvent_Playing:
	{
		LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_Playing - Fire notification to subscriber.";

		if(VueceNetworkPlayerFsm::GetNetworkPlayerState() == vuece::NetworkPlayerState_Playing)
		{
			VueceLogger::Debug("VueceStreamPlayer - OnAudioWriterStateNotification, already in PLAYING state, no need to send duplication notification");
		}
		else
		{
			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Playing);
			VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_Started, vuece::NetworkPlayerState_Playing);
		}

		break;
	}
	case VueceStreamAudioWriterExternalEvent_BufWindowThresholdReached:
	{
		LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_BufWindowThresholdReached - Fire notification to subscriber.";

		if(!StateTransition(StreamPlayerFsmEvent_BufBelowThreshold))
		{
			VueceLogger::Fatal("VueceStreamPlayer - VueceStreamAudioWriterStateEventID_BufWindow_Threshold_Reached, state transition is not allowed.");
		}
		else
		{

			VueceStreamPlayerNotificaiontMsg notification;

			memset(&notification, 0, sizeof(VueceStreamPlayerNotificaiontMsg));

			notification.notificaiont_type  = VueceStreamPlayerNotificationType_Request;
			notification.request_type = VueceStreamPlayerRequest_Download_Next_BufWin;
			notification.value1 = event->value;// + 1; //start position

			LOG(LS_INFO) << "VueceStreamPlayer - Start position of next buffer window is: " << notification.value1;

			//update the start position of current buffer window
			start_pos_current_buf_win = notification.value1;

			LOG(LS_INFO) << "VueceStreamPlayer - Current buffer start portion is updated to: " << start_pos_current_buf_win;

			//TODO - Continue your work here...
//			SignalStreamPlayerNotification(&notification);

			VueceNetworkPlayerFsm::FireNotification(&notification);
		}

		break;
	}
	case VueceStreamAudioWriterExternalEvent_BufWinConsumed:
	{
		LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_BufWinConsumed";

		//we should wait in this case
		if(!StateTransition(StreamPlayerFsmEvent_BufWinConsumed))
		{
			VueceLogger::Fatal("VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_BufWinConsumed, state transition is not allowed.");
		}

		break;
	}
	case VueceStreamAudioWriterExternalEvent_Completed:
	{
		LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_Completed - Stop streamer now.";

		//note state transition is done in Stop() UUU
		if(player_monitor != NULL)
		{
			SetStopReason(VueceStreamPlayerStopReason_Completed);

			//player_monitor->StopAsync();
			LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_Completed - calling player_monitor->StopStreamPlayer()";
			player_monitor->StopStreamPlayer();
			LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_Completed - calling player_monitor->StopStreamPlayer() returned";
		}

		//NOTE - Stop operation is finished in another thread and the EndOfSong notification
		//is fired when Stop is finished.
		notify_end_of_song = true;

		VueceLogger::Debug("VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_Completed: End OF SONG notification will be fired once stop is finished.");

		break;
	}
	case VueceStreamAudioWriterExternalEvent_WatchdogExpired:
	{
		LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_WatchdogExpired - Stop streamer now.";

		//Note this notification is from audio writer thread, we have to use another thread to stop the player
		//otherwise the Stop() on player will trigger a Stop() call back to audio writer in the same thread
		//note state transition is done in Stop()
		if(player_monitor != NULL)
		{
			SetStopReason(VueceStreamPlayerStopReason_WatchdogExpired);

			//player_monitor->StopAsync();
			LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_WatchdogExpired - calling player_monitor->StopStreamPlayer()";
			player_monitor->StopStreamPlayer();
			LOG(LS_INFO) << "VueceStreamPlayer - VueceStreamAudioWriterExternalEvent_WatchdogExpired - calling player_monitor->StopStreamPlayer() returned";

		}

		break;
	}
	default:
	{
		VueceLogger::Fatal("VueceStreamPlayer - Unknown event id: %d, abort now.", event->id);
		break;
	}
	}

}

void VueceStreamPlayer::StopAndResetPlayerAsync()
{
	LOG(LS_INFO) << "VueceStreamPlayer::StopAndResetPlayerAsync";
	if(player_monitor != NULL)
	{
		player_monitor->StopAndResetStreamPlayer();
	}
	else
	{
		LOG(LS_INFO) << "VueceStreamPlayer::StopPlayerAsync - player_monitor is null, do nothing";
	}
}

void VueceStreamPlayer::SetStopReason(VueceStreamPlayerStopReason reason)
{
	LOG(LS_INFO) << "VueceStreamPlayer::SetStopReason code = " << reason;
	stop_reason = reason;
}

void VueceStreamPlayer::Init()
{
	LOG(LS_INFO) << "VueceStreamPlayer - Init";

	used_frame_dur_ms = 0;

	current_play_progress = 0;

	start_pos_current_buf_win = 0;

	memset(current_song_uri, 0, sizeof(current_song_uri));

	stream_engine = NULL;

	player_monitor = NULL;

	notify_end_of_song = false;
	allow_stream = false;

	VueceThreadUtil::InitMutex(&mutex_state);
	VueceThreadUtil::InitMutex(&mutex_allow_streaming);

	current_fsm_state = VuecePlayerFsmState_Stopped;

	LogCurrentFsmState();
}

void VueceStreamPlayer::UnInit()
{
	LOG(LS_INFO) << "VueceStreamPlayer::UnInit - Stop monitor thread";

	if( player_monitor != NULL )
	{
		delete player_monitor;
		player_monitor = NULL;
	}

	LOG(LS_INFO) << "VueceStreamPlayer::UnInit - Unlock mutex";

	if(mutex_state.IsInitialized() )
	{
		mutex_state.Unlock();
	}

	if(mutex_allow_streaming.IsInitialized() )
	{
		mutex_allow_streaming.Unlock();
	}

	LOG(LS_INFO) << "VueceStreamPlayer::UnInit - Done";
}

bool VueceStreamPlayer::IsStreamingAllowed()
{
	bool ret = false;

	VueceThreadUtil::MutexLock(&mutex_allow_streaming);

	ret = allow_stream;

	//commented out to avoid massive trace output
//	if(ret)
//	{
//		VueceLogger::Debug("VueceStreamPlayer::IsStreamingAllowed - YES");
//	}
//	else
//	{
//		VueceLogger::Debug("VueceStreamPlayer::IsStreamingAllowed - NO");
//	}

	VueceThreadUtil::MutexUnlock(&mutex_allow_streaming);

	return ret;
}

void VueceStreamPlayer::SetStreamAllowed(bool allowed)
{
	VueceThreadUtil::MutexLock(&mutex_allow_streaming);

	allow_stream = allowed;

	if(allow_stream)
	{
		VueceLogger::Debug("VueceStreamPlayer::SetStreamAllowed - YES");
	}
	else
	{
		VueceLogger::Debug("VueceStreamPlayer::SetStreamAllowed - NO");
	}

	VueceThreadUtil::MutexUnlock(&mutex_allow_streaming);
}


bool VueceStreamPlayer::HasStreamEngine()
{
	if(stream_engine != NULL)
	{
		return true;
	}

	return false;
}

bool VueceStreamPlayer::CreateStreamEngine(
		int sample_rate,
		int bit_rate, //not used for now
		int nchannels,
		int duration,
		int frame_dur_ms,
		bool startup_standalone,
		bool download_finished,
		int last_avail_chunk_idx,
		int last_chunk_frame_count,
		int resume_pos
		)
{
	bool ret = false;
	bool wait_released_event = false;

	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - List all input parameters:");
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - sample_rate = %d", sample_rate);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - bit_rate = %d", bit_rate);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - nchannels = %d", nchannels);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - duration = %d", duration);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - frame_dur_ms = %d", frame_dur_ms);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - startup_standalone = %d", (int)startup_standalone);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - download_finished = %d", (int)download_finished);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - last_avail_chunk_idx = %d", last_avail_chunk_idx);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - last_chunk_frame_count = %d", last_chunk_frame_count);
	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - resume_pos = %d", resume_pos);

	used_frame_dur_ms = frame_dur_ms;

	start_pos_current_buf_win = resume_pos;

	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - start_pos_current_buf_win is updated to: %d", resume_pos);

	LogCurrentFsmState();

	VueceThreadUtil::MutexLock(&mutex_state);

	//handle initial state
	if(current_fsm_state != VuecePlayerFsmState_Ready && current_fsm_state != VuecePlayerFsmState_Stopped)
	{
//		VueceLogger::Fatal("VueceStreamPlayer::Create - Engine is not in READY|STOPPED state, sth is wrong, abort.");
		VueceLogger::Warn("VueceStreamPlayer::Create - Engine is not in READY|STOPPED state, wait ...");
//		return ret;

		wait_released_event = true;
	}

	VueceThreadUtil::MutexUnlock(&mutex_state);

	if(wait_released_event)
	{
		int time_passed = 0;

		VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - ************************************");
		VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - Need to wait until player is stopped with timeout: %d", TIMEOUT_WAIT4PLAYER_STOPPED);
		VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - ************************************");

		while(true)
		{
			VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - Waiting loop");

			VueceThreadUtil::SleepSec(1);

			VueceThreadUtil::MutexLock(&mutex_state);

			LogCurrentFsmState();

			if(current_fsm_state == VuecePlayerFsmState_Stopped)
			{
				VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - Player is stopped, we can continue now.");
				VueceThreadUtil::MutexUnlock(&mutex_state);
				break;
			}

			VueceThreadUtil::MutexUnlock(&mutex_state);

			time_passed++;

			if(time_passed >= TIMEOUT_WAIT4PLAYER_STOPPED)
			{
				VueceLogger::Info("VueceStreamPlayer::Waiting for player to be stopped, timed out.");
				break;
			}
		}
	}


	VueceThreadUtil::MutexLock(&mutex_state);
	current_fsm_state = VuecePlayerFsmState_Ready;

//	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Playing);

	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - current_fsm_state is updated to READY, external_network_player_state is updated to PLAYING.");

	VueceThreadUtil::MutexUnlock(&mutex_state);

	VueceLogger::Info("VueceStreamPlayer::CreateStreamEngine - Current state is forced to READY");

	stream_engine = new VueceStreamEngine();

	ret = stream_engine->Init(sample_rate,
			bit_rate,
			nchannels,
			duration,
			frame_dur_ms,
			startup_standalone,
			download_finished,
			last_avail_chunk_idx,
			last_chunk_frame_count,
			resume_pos);

	if(!ret)
	{
		VueceLogger::Fatal("VueceStreamPlayer::Create - Engine cannot be initialized.");
		return ret;
	}

	return ret;

}

void VueceStreamPlayer::StartStreamEngine()
{
	VueceLogger::Info("VueceStreamPlayer::StartStreamEngine");

	if( !StateTransition(StreamPlayerFsmEvent_Start) )
	{
		VueceLogger::Fatal("VueceStreamPlayer::StartStreamEngine - Operation is not allowed, sth is wrong, abort now.");
		return;
	}

	LOG(LS_VERBOSE) << "VueceStreamPlayer::Start - 2, start monitor";

	StartPlayerMonitor();

	LOG(LS_VERBOSE) << "VueceStreamPlayer::Start - Done";

	stream_engine->Start();

	if (stream_engine->IsRunning())
	{
		VueceLogger::Info("VueceStreamPlayer::StartStreamEngine - Engine is running now.");
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::StartStreamEngine - Engine is not running, sth is wrong");
	}

	ResumeStreamBumper();

	VueceLogger::Info("VueceStreamPlayer::StartStreamEngine - Done");

}

bool VueceStreamPlayer::StateTransition(StreamPlayerFsmEvent e)
{
	bool allowed = false;

	char current_state_s[64];
	char new_state_s[64];
	char event_s[64];

	VueceLogger::Info("VueceStreamPlayer::StateTransition - Event code - %d", (int)e);

	LogFsmEvent(e);

	VueceLogger::Debug("VueceStreamPlayer::StateTransition - Debug 1");

	LogCurrentFsmState();

	VueceLogger::Debug("VueceStreamPlayer::StateTransition - Debug 2");

	VueceThreadUtil::MutexLock(&mutex_state);

	VueceLogger::Debug("VueceStreamPlayer::StateTransition - Debug 3");


	GetFsmEventString(e, event_s);
	GetFsmStateString(current_fsm_state, current_state_s);

	VueceLogger::Debug("VueceStreamPlayer::StateTransition - Debug 4");


	switch(current_fsm_state)
	{
	case VuecePlayerFsmState_Ready:
	{
		switch(e)
		{
		case StreamPlayerFsmEvent_Start:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Playing;

//			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Playing);

//			external_network_player_state = vuece::NetworkPlayerState_Playing;
			break;
		}
		case StreamPlayerFsmEvent_Stop:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}

		break;
	}

	case VuecePlayerFsmState_Playing:
	{
		switch(e)
		{
		//this case is possible when part of current buf window is downloaded, but not finished
		//in this case event StreamPlayerFsmEvent_BufBelowThreshold cannot be triggered
		//so player is in PLAYING state, but eventually data will be consumed, and it will receive BufWinConsumed event
		case StreamPlayerFsmEvent_BufWinConsumed:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_WaitingForNextBufWin;
			break;
		}
		case StreamPlayerFsmEvent_BufBelowThreshold:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_WaitingForNextBufWin;
			break;
		}
		case StreamPlayerFsmEvent_BufWinAvailable:
		{
			//ignore, state in the same state
			VueceStreamPlayer::LogIgnoredEvent(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		//DDD
		case StreamPlayerFsmEvent_Stop:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VuecePlayerFsmState_WaitingForNextBufWin:
	{
		switch(e)
		{
		case StreamPlayerFsmEvent_BufWinConsumed:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_WaitingAllDataConsumed;

			//TODO - Probably need a new state for buffering because now waiting can also mean waiting for
			// an user operation to finish
			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Buffering);
			VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_Buffering, vuece::NetworkPlayerState_Buffering);

			break;
		}
		case StreamPlayerFsmEvent_BufWinAvailable:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Playing;

			break;
		}
		case StreamPlayerFsmEvent_Stop:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Stopping;
			break;
		}
		case StreamPlayerFsmEvent_Released:
		{
			//This case can happen when player is forced to stop because of network error
			allowed = true;

			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}
		break;
	}
	case VuecePlayerFsmState_WaitingAllDataConsumed:
	{
		switch(e)
		{
		case StreamPlayerFsmEvent_BufWinAvailable:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Playing;
			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Playing);

			break;
		}
		case StreamPlayerFsmEvent_Stop:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Stopping;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}
		break;
	}
	case VuecePlayerFsmState_Stopping:
	{
		switch(e)
		{
		case StreamPlayerFsmEvent_BufWinAvailable:
		case StreamPlayerFsmEvent_Stop:
		case StreamPlayerFsmEvent_BufBelowThreshold:
		{
			VueceStreamPlayer::LogIgnoredEvent(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		case StreamPlayerFsmEvent_Released:
		{
			allowed = true;
			current_fsm_state = VuecePlayerFsmState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}
		break;
	}
	case VuecePlayerFsmState_Stopped:
	{
		switch(e)
		{
		case StreamPlayerFsmEvent_Stop:
		{
			VueceStreamPlayer::LogIgnoredEvent(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODULE_NAME_PLAYER, event_s, current_state_s);
			break;
		}
		}
		break;
	}

	default:
	{
		VueceLogger::Fatal("VueceStreamPlayer::StateTransition - Unknown state: %d", (int)current_fsm_state); break;
	}

	}

	if(allowed)
	{
		GetFsmStateString(current_fsm_state, new_state_s);
		VueceStreamPlayer::LogStateTranstion(MODULE_NAME_PLAYER, event_s, current_state_s, new_state_s);
	}

	VueceThreadUtil::MutexUnlock(&mutex_state);

	VueceLogger::Debug("VueceStreamPlayer::StateTransition - End");

	return allowed;
}

void VueceStreamPlayer::LogFsmEvent(StreamPlayerFsmEvent e)
{
	switch (e)
	{
	case StreamPlayerFsmEvent_Start:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_Start"); break;
	}
	case StreamPlayerFsmEvent_Stop:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_Stop"); break;
	}
	case StreamPlayerFsmEvent_BufBelowThreshold:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_BufBelowThreshold"); break;
	}
	case StreamPlayerFsmEvent_BufWinConsumed:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_BufWinConsumed"); break;
	}
	case StreamPlayerFsmEvent_BufWinAvailable:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_BufWinAvailable"); break;
	}
	case StreamPlayerFsmEvent_Released:
	{
		VueceLogger::Debug("VueceStreamPlayer::LogFsmEvent - StreamPlayerFsmEvent_Released"); break;
	}
	default:
	{
		VueceLogger::Fatal("VueceStreamPlayer::LogFsmEvent - Unexpected event: %d, abort now!", (int) e);
	}
	}
}


VuecePlayerFsmState VueceStreamPlayer::FsmState()
{
	VuecePlayerFsmState state;

	VueceThreadUtil::MutexLock(&mutex_state);
	state  = current_fsm_state;
	VueceThreadUtil::MutexUnlock(&mutex_state);

	return state;
}

void VueceStreamPlayer::StartPlayerMonitor()
{
	LOG(INFO) << "VueceStreamPlayer::StartPlayerMonitor";

	if(player_monitor == NULL)
	{
		LOG(INFO) << "VueceStreamPlayer::StartPlayerMonitor - Creating monitor thread";

		player_monitor = new VueceStreamPlayerMonitorThread2();

		player_monitor->Start();

		if (player_monitor->IsRunning())
		{
			VueceLogger::Info("VueceStreamPlayer::StartPlayerMonitor - Engine is running now.");
		}
		else
		{
			VueceLogger::Fatal("VueceStreamPlayer::StartPlayerMonitor - Engine is not running, sth is wrong");
		}
	}
	else
	{
		LOG(INFO) << "VueceStreamPlayer::StartPlayerMonitor - Already running";
	}
}

void VueceStreamPlayer::SetJniEnv(JNIEnv* e)
{
	jni_env = e;
}

bool VueceStreamPlayer::CanLocalSeekProcceed(int targetPos)
{
	bool ret = false;
//	JNIEnv *env = VueceJni::GetJniEnv();
//	COFFEE_TRY_JNI(jni_env, ret = CanLocalSeekProcceedCrashTest(targetPos));
//	VueceLogger::Debug("VueceStreamPlayer::CanLocalSeekProcceed - Trigger crash here!!");

	int iLastStreamTerminationPostionSec = VueceGlobalContext::GetLastStreamTerminationPositionSec();

	VueceLogger::Debug("VueceStreamPlayer::CanLocalSeekProcceed - targetPost = %d, start_pos_current_buf_win = %d, current_termination_pos = %d",
			targetPos, start_pos_current_buf_win, iLastStreamTerminationPostionSec);

	if(iLastStreamTerminationPostionSec == VUECE_VALUE_NOT_SET)
	{
		VueceLogger::Debug("VueceStreamPlayer::CanLocalSeekProcceed - iLastStreamTerminationPostionSec NOT SET, return false, force remote seek");
		return false;
	}

	if(targetPos >= start_pos_current_buf_win && targetPos <= iLastStreamTerminationPostionSec)
	{
		ret = true;
	}

	return ret;
}


//TODO REMOVE THIS, NOT USED.
void VueceStreamPlayer::SeekStream(int posInSec)
{
#ifdef VUECE_APP_ROLE_HUB_CLIENT

	int pos =  posInSec;

	LOG(INFO) << "VueceStreamPlayer::SeekStream: " << posInSec << ", pause player at first";

	LogCurrentFsmState();

	VueceLogger::Fatal("VueceStreamPlayer::SeekStream - NOT IMPLEMENTED FOR NOW");

#endif
}

void VueceStreamPlayer::ResumeStreamPlayer(int resume_pos,
		int used_sample_rate,
		int used_bit_rate,
		int used_nchannels,
		int used_duration)
{
	LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer";

#ifdef VUECE_APP_ROLE_HUB_CLIENT

	LOG(INFO) << "VueceStreamPlayer::ResumeStreamPlayer:pos: " << resume_pos << ", used_duration: " << used_duration
			<< ", last available chunk file id: " << VueceGlobalContext::GetLastAvailableAudioChunkFileIdx()
			<< ", last stream termination position in seconds: " << VueceGlobalContext::GetLastStreamTerminationPositionSec();

	LogCurrentFsmState();

	//TODO - We need to validate the resume position at first
	//return an error if target chunk file doesn't exist
	if( VueceGlobalContext::GetLastAvailableAudioChunkFileIdx() < 0)
	{
		VueceLogger::Fatal("There is no available chunk file, player cannot be resumed, abort.");

		return;
	}

	LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - calling VueceStreamPlayer::Create(), used_frame_dur_ms = " << used_frame_dur_ms;

	bool bIsDowloadCompleted = VueceGlobalContext::IsDownloadCompleted();
//	int iLastAvailableAudioChunkFileIdx = VueceGlobalContext::GetLastAvailableAudioChunkFileIdx();
//	int iAudioFrameCounterInCurrentChunk = VueceGlobalContext::GetAudioFrameCounterInCurrentChunk();
	//--------------------
	//Create player instance
	VueceStreamPlayer::CreateStreamEngine(
			used_sample_rate,
			used_bit_rate,
			used_nchannels,
			used_duration,
			used_frame_dur_ms,
			true,//start up as standalone
			bIsDowloadCompleted,
			VueceGlobalContext::GetLastAvailableAudioChunkFileIdx(),
			VueceGlobalContext::GetAudioFrameCounterInCurrentChunk(),
			resume_pos
	   );

	VueceStreamPlayer::StartStreamEngine();

	int iLastStreamTerminationPostionSec = VueceGlobalContext::GetLastStreamTerminationPositionSec();

	LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Injecting last termination position";

	//inject last termination position otherwise buffer window will not be activated
	InjectStreamTerminationPosition(iLastStreamTerminationPostionSec, false);

	LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Injecting start postion";

	InjectFirstFramePosition(VueceGlobalContext::GetFirstFramePositionSec());

	if(VueceGlobalContext::IsDownloadCompleted())
	{
		LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Download finished, disable buffer win check";

		VueceStreamPlayer::DisableBufWinCheckInAudioWriter();
	}
	else
	{
		LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Download not finished yet, enable buffer win check";

		VueceStreamPlayer::EnableBufWinCheckInAudioWriter();

		int buf_win = iLastStreamTerminationPostionSec - resume_pos;

		LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Checking buffer window threshold, Resume position = " <<
				resume_pos << " second, stream terminate position = " << iLastStreamTerminationPostionSec << " second, buffer window = " <<
				buf_win << " seconds, predefined buffer window threshold = " << VUECE_BUFWIN_THRESHOLD_SEC << " second";

		if( buf_win < VUECE_BUFWIN_THRESHOLD_SEC )
		{
			int i = 1;

			LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - resume position is beyond buffer window threshold, next buffer window download will be triggered immediately";

			VueceStreamPlayer::EnableBufWinDownloadDuringStart();
		}
	}

	LOG(LS_VERBOSE) << "VueceStreamPlayer::ResumeStreamPlayer - Done";

#else
	VueceLogger::Fatal("FATAL ERROR!!! VueceStreamPlayer::ResumeStreamPlayer is not supported in non-hub client.");
#endif

}


void VueceStreamPlayer::HandleForcedStop()
{
	LOG(INFO) << ("VueceStreamPlayer::HandleForcedStop");

	if(HasStreamEngine())
	{
		VueceLogger::Info("VueceStreamPlayer::HandleForcedStop - forced, currently has no stream engine instance, simply return to IDLE");

		StopStreamEngine();
	}
	else
	{
		VueceLogger::Info("VueceStreamPlayer::Stop - forced, currently has no stream engine instance, simply return to IDLE");

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_NetworkErr, vuece::NetworkPlayerState_Idle);
	}
}

void VueceStreamPlayer::StopStreamEngine()
{

	//Note - although we are going to stop the stream player here, we still
	//pause the player at first so that the user can get a fast response, after
	//that we release/destroy the audio stream by calling audio_stream_stop()
	LOG(INFO) << ("VueceStreamPlayer::Stop 2");

	current_play_progress = GetCurrentPlayingProgress();

	VueceLogger::Info("VueceStreamPlayer::Stop - Remember current play progress: %d", current_play_progress);

	stream_engine->writer->PausePlayer();

	stream_engine->bumper->PauseBumper();

	VueceLogger::Debug("VueceStreamPlayer::Stop - stop stream now.");

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Note - We need to sync resource release here, should not immediately send STOPPED signal
	//back to java layer.

	//TODO - Destroy stream engine instance here
	stream_engine->StopSync();

	VueceLogger::Debug("VueceStreamPlayer::Stop - StopSync() returned, deleting stream_engine");

	//TODO We need to delete the engine instance to release JNI local refs!!!!
	delete stream_engine;

	stream_engine = NULL;

	VueceLogger::Debug("VueceStreamPlayer::Stop - Engine successfully deleted");

	VueceLogger::Debug("VueceStreamPlayer::Stop - Perform state transition");

	//stop monitor
	//BUG - The notification will trigger nested callback
	StateTransition(StreamPlayerFsmEvent_Released);

	VueceLogger::Debug("VueceStreamPlayer::Stop - Done 1");

	VueceStreamPlayer::LogCurrentStreamingParams();

	VueceLogger::Debug("VueceStreamPlayer::Stop - Done 2 - Sending notification");

	switch(stop_reason)
	{
	case VueceStreamPlayerStopReason_Completed:
	{
		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_EndOfSong, vuece::NetworkPlayerState_Idle);
		break;
	}
	case VueceStreamPlayerStopReason_WatchdogExpired:
	{
		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_OperationTimedout, vuece::NetworkPlayerState_Idle);
		break;
	}
	case VueceStreamPlayerStopReason_NetworkErr:
	{
		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_NetworkErr, vuece::NetworkPlayerState_Idle);
		break;
	}
	case VueceStreamPlayerStopReason_PausedByUser:
	{
		VueceLogger::Debug("VueceStreamPlayer::Stop - Reason is PausedByUser, notification will not be fired HERE.");
		break;
	}
	default:
	{
		VueceLogger::Fatal("VueceStreamPlayer::Stop - Done 3, unknown reason code: %d, abort now.", stop_reason);
		break;
	}
	}
}

void VueceStreamPlayer::Stop(bool forced)
{
	LOG(INFO) << ("VueceStreamPlayer::Stop");

#ifdef VUECE_APP_ROLE_HUB
	return;
#else

	if(VueceNetworkPlayerFsm::GetNetworkPlayerState() == vuece::NetworkPlayerState_Idle)
	{
		VueceLogger::Info("VueceStreamPlayer::Stop - Called when already in IDLE, ignore and return");
		return;
	}

	LOG(INFO) << ("VueceStreamPlayer::Stop 1");

	if(forced)
	{
		LOG(INFO) << ("VueceStreamPlayer::Stop - forced flag is true, calling HandleForcedStop");

		HandleForcedStop();

		return;
	}

	if(!StateTransition(StreamPlayerFsmEvent_Stop))
	{
		VueceLogger::Info("VueceStreamPlayer::Stop - Operation not allowed.");
	}
	else if(!HasStreamEngine())
	{
		VueceLogger::Info("VueceStreamPlayer::Stop - Stream engine is null, already stopped, do nothing");
	}
	else
	{
		StopStreamEngine();
	}

	//WARNING - DO NOT DELETE player_monitor here!!!

	VueceLogger::Debug("VueceStreamPlayer::Stop - Completed.");

#endif
}

void VueceStreamPlayer::ResumeStreamBumper()
{
	VueceLogger::Debug("VueceStreamPlayer::ResumeStreamBumper - Do nothing");

	LogCurrentFsmState();

//	stream_engine->bumper->ResumeBumper();

}

void VueceStreamPlayer::InjectBumperTerminationInfo(int idx_of_last_chunk_file, int num_framecount_of_last_chunk)
{
	int i1 = idx_of_last_chunk_file;
	int i2 = num_framecount_of_last_chunk;

	VueceLogger::Debug("VueceStreamPlayer::InjectBumperTerminationInfo - idx_of_last_chunk_file = %d, num_framecount_of_last_chunk = %d", idx_of_last_chunk_file, num_framecount_of_last_chunk);

	LogCurrentFsmState();

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->bumper->SetTerminateInfo(i1, i2);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::InjectBumperTerminationInfo - No stream engine, show we abort here?");
	}
}

void VueceStreamPlayer::InjectLastAvailChunkIdIntoBumper(int idx)
{
	VueceLogger::Debug("VueceStreamPlayer::InjectLastAvailChunkIdIntoBumper: %d", idx);

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->bumper->SetLastAvailChunkFileIdx(idx);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::InjectLastAvailChunkIdIntoBumper - No stream engine, show we abort here?");
	}
}

void VueceStreamPlayer::InjectFirstFramePosition(int sec)
{
	int ts = sec;
	VueceLogger::Debug("VueceStreamPlayer::InjectFirstFramePosition: %d", ts);

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->bumper->SetFirstFramePosition(ts);
		stream_engine->writer->SetFirstFramePosition(ts);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::InjectFirstFramePosition - No stream engine, show we abort here?");
	}
}

void VueceStreamPlayer::InjectStreamTerminationPosition(int second, bool force_download_complete)
{
	int ts = second;
	VueceLogger::Debug("VueceStreamPlayer::InjectStreamTerminationPosition: %d", ts);

	//no matter current stream player instance exists or not, we remember this position as a global variable
	//at first for later possible use

	VueceGlobalContext::SetLastStreamTerminationPositionSec(second);

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->writer->SetStreamTerminationPosition(ts, force_download_complete);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::InjectStreamTerminationPosition - No stream engine, should we abort here?");
	}
}

void VueceStreamPlayer::OnStreamingCompleted(void)
{
	VueceLogger::Debug("VueceStreamPlayer::OnStreamingCompleted");

	//TODO - Continue your work here.
}

void VueceStreamPlayer::OnNewDataAvailable()
{
	VueceLogger::Debug("VueceStreamPlayer::OnNewDataAvailable");

	StateTransition(StreamPlayerFsmEvent_BufWinAvailable);
}

int VueceStreamPlayer::GetCurrentPlayingProgress(void)
{
	VueceLogger::Debug("VueceStreamPlayer::GetCurrentPlayingProgress");

	if(VueceStreamPlayer::HasStreamEngine())
	{
		return stream_engine->writer->GetCurrentPlayingProgress();
	}
	else
	{
//		VueceLogger::Fatal("VueceStreamPlayer::GetCurrentPlayingProgress - No stream engine, abort for unusual behavior");
		VueceLogger::Warn("VueceStreamPlayer::GetCurrentPlayingProgress - No stream engine, will return dummpy zero");
	}

	return 0;
}

void VueceStreamPlayer::EnableBufWinCheckInAudioWriter()
{
	VueceLogger::Debug("VueceStreamPlayer::EnableBufWinCheckInAudioWriter");

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->writer->EnableBufWin(1);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::EnableBufWinCheckInAudioWriter - No stream engine, show we abort here?");
	}

}

void VueceStreamPlayer::EnableBufWinDownloadDuringStart()
{
	VueceLogger::Debug("VueceStreamPlayer::DisableBufWinCheckInAudioWriter");

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->writer->EnableBufWinDownloadDuringStart(1);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::EnableBufWinDownloadDuringStart - No stream engine, show we abort here?");
	}

}


void VueceStreamPlayer::DisableBufWinCheckInAudioWriter()
{
	int i = 0;
	VueceLogger::Debug("VueceStreamPlayer::DisableBufWinCheckInAudioWriter");

	if(VueceStreamPlayer::HasStreamEngine())
	{
		stream_engine->writer->EnableBufWin(0);
	}
	else
	{
		VueceLogger::Fatal("VueceStreamPlayer::EnableBufWinDownloadDuringStart - No stream engine, show we abort here?");
	}


}

bool VueceStreamPlayer::StillInTheSamePlaySession(void)
{
	bool ret = false;

//	VueceStreamPlayer::LogCurrentFsmState();

	VuecePlayerFsmState player_state = VueceStreamPlayer::FsmState();

	if(player_state == VuecePlayerFsmState_WaitingForNextBufWin
			||player_state == VuecePlayerFsmState_WaitingAllDataConsumed)
	{
		ret = true;

		VueceLogger::Debug("VueceStreamPlayer::StillInTheSamePlaySession - YES");
	}
	else
	{
		VueceLogger::Debug("VueceStreamPlayer::StillInTheSamePlaySession - NO");
	}

	return ret;
}


void VueceStreamPlayer::LogCurrentFsmState()
{
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentFsmState - Debug 1");

	VuecePlayerFsmState s = VueceStreamPlayer::FsmState();

	VueceLogger::Debug("VueceStreamPlayer::LogCurrentFsmState - Debug 2");

	switch(s)
	{
	case VuecePlayerFsmState_Ready:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_Stopped"; return;
	case VuecePlayerFsmState_Playing:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_Playing"; break;
	case VuecePlayerFsmState_WaitingForNextBufWin:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_WaitingForNextBufWin"; break;
	case VuecePlayerFsmState_WaitingAllDataConsumed:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_WaitingAllDataConsumed"; break;
	case VuecePlayerFsmState_Stopping:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_Stopping"; break;
	case VuecePlayerFsmState_Stopped:
		LOG(INFO) << "VueceStreamPlayer::LogCurrentFsmState - VuecePlayerFsmState_Stopped"; return;
	default:
		VueceLogger::Fatal("VueceStreamPlayer::LogCurrentFsmState - Unknown state: %d", (int)s); break;
	}

}


void VueceStreamPlayer::GetFsmStateString(VuecePlayerFsmState s, char* state_s)
{
	switch(s)
	{
	case VuecePlayerFsmState_Ready:
		strcpy(state_s, "READY"); return;
	case VuecePlayerFsmState_Playing:
		strcpy(state_s, "PLAYING"); return;
	case VuecePlayerFsmState_WaitingForNextBufWin:
		strcpy(state_s, "WAITING FOR NEXT BUFFER WINDOW"); return;
	case VuecePlayerFsmState_WaitingAllDataConsumed:
		strcpy(state_s, "WAITING ALL DATA CONSUMED"); return;
	case VuecePlayerFsmState_Stopping:
		strcpy(state_s, "STOPPING"); return;
	case VuecePlayerFsmState_Stopped:
		strcpy(state_s, "STOPPED"); return;
	default:
		VueceLogger::Fatal("VueceStreamPlayer::GetFsmStateString - Unknown State: %d", (int)s); break;
	}

}

void VueceStreamPlayer::GetFsmEventString(StreamPlayerFsmEvent e, char* event_s)
{
	switch(e)
	{
	case StreamPlayerFsmEvent_Start:
		strcpy(event_s, "START"); return;
	case StreamPlayerFsmEvent_BufBelowThreshold:
		strcpy(event_s, "BUFFER WINDOWN BELOW THRESHOLD"); return;
	case StreamPlayerFsmEvent_BufWinConsumed:
		strcpy(event_s, "BUFFER WINDOW CONSUMED"); return;
	case StreamPlayerFsmEvent_BufWinAvailable:
		strcpy(event_s, "BUFFER WINDOW AVAILABLE"); return;
	case StreamPlayerFsmEvent_Stop:
		strcpy(event_s, "STOP"); return;
	case StreamPlayerFsmEvent_Released:
		strcpy(event_s, "RELEASED"); return;
	default:
		VueceLogger::Fatal("VueceStreamPlayer::GetFsmEventString - Unknown event: %d", (int)e); break;
	}
}

void VueceStreamPlayer::LogCurrentStreamingParams()
{
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams --------- Current Streaming Parameters Start --------------");

	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - Predefined buffer window width: %d", VUECE_BUFFER_WINDOW);
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - Total audio frame number = %d", VueceGlobalContext::GetTotalAudioFrameCounter());
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - First frame position in current PLAY session = %d", VueceGlobalContext::GetFirstFramePositionSec());
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - Last available chunk file ID = %d", VueceGlobalContext::GetLastAvailableAudioChunkFileIdx());
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - Frame count in current chunk = %d", VueceGlobalContext::GetAudioFrameCounterInCurrentChunk());
	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams - Last frame termination position = %d", VueceGlobalContext::GetLastStreamTerminationPositionSec());

	VueceLogger::Debug("VueceStreamPlayer::LogCurrentStreamingParams --------- Current Streaming Parameters End --------------");
}

void VueceStreamPlayer::ResetCurrentStreamingParams()
{
	VueceLogger::Debug("VueceStreamPlayer::ResetCurrentStreamingParams***************************************");
	VueceLogger::Debug("VueceStreamPlayer::ResetCurrentStreamingParams***************************************");
	VueceLogger::Debug("VueceStreamPlayer::ResetCurrentStreamingParams***************************************");

	VueceGlobalContext::SetTotalAudioFrameCounter(VUECE_VALUE_NOT_SET);
	VueceGlobalContext::SetFirstFramePositionSec(VUECE_VALUE_NOT_SET);
	VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(VUECE_VALUE_NOT_SET);
	VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(VUECE_VALUE_NOT_SET);
	VueceGlobalContext::SetLastStreamTerminationPositionSec(VUECE_VALUE_NOT_SET);
	VueceGlobalContext::SetNewResumePos(0);
}

void VueceStreamPlayer::LogStateTranstion(const char* module, const char* event, const char* old_state, const char* new_state)
{
	VueceLogger::Info("%s::STATE TRANSTION - Event: [%s], State is switched from [%s] ---> [%s]", module, event, old_state, new_state);
}

void VueceStreamPlayer::AbortOnInvalidTranstion(const char* module, const char* event, const char* current_state)
{
	VueceLogger::Fatal("%s::AbortOnInvalidTranstion - Invalid event: [%s], in state: [%s]", module, event, current_state);
}

void VueceStreamPlayer::LogIgnoredEvent(const char* module, const char* event, const char* current_state)
{
//	VueceLogger::Info("%s::LogIgnoredEvent - Event: [%s] is ignored in state: [%s]", module, event, current_state);
}
