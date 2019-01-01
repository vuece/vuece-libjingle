/*
 * VueceStreamPlayerMonitorThread2.cc
 *
 *  Created on: Sep 18, 2014
 *      Author: jingjing
 */

#include "talk/base/logging.h"
#include "VueceStreamPlayer.h"
#include "VueceStreamPlayerMonitorThread2.h"
#include "VueceThreadUtil.h"
#include "VueceLogger.h"
#include "VueceJni.h"
#include "VueceNetworkPlayerFsm.h"

VueceStreamPlayerMonitorThread2::VueceStreamPlayerMonitorThread2()
{
	released = false;
	running = false;
	stop_cmd_issued = false;
	enable_reset = false;
	VueceThreadUtil::InitMutex(&mutex_running);
	VueceThreadUtil::InitMutex(&mutex_release);
}
VueceStreamPlayerMonitorThread2::~VueceStreamPlayerMonitorThread2()
{
	LOG(LS_VERBOSE) << "VueceStreamPlayerMonitorThread2 - Destructor called 1";

	StopSync();

	LOG(LS_VERBOSE) << "VueceStreamPlayerMonitorThread2 - Destructor called 2";

}

#define THREAD_TAG_PLAYER_MONITOR "VueceStreamPlayerMonitor"

void* VueceStreamPlayerMonitorThread2::Thread()
{
	int log_counter = 0;

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Start");

	VueceJni::AttachCurrentThreadToJniEnv(THREAD_TAG_PLAYER_MONITOR);

	ThreadStarted();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Started");

	mutex_running.Lock();

	if(stop_cmd_issued)
	{
		VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Started but stop command already issued, thread will end now");
	}
	else
	{
	}

	running = true;


	while (running)
	{
		log_counter++;

		if(log_counter % 30 == 0)
			VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - checking stop command from external thread.");

		mutex_running.Unlock();

		VueceThreadUtil::SleepSec(1);

		mutex_running.Lock();

		if(stop_cmd_issued)
		{
			VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Stop cmd issued, stop player now.");

			/*
			 * Note this is a sync call to VueceStreamPlayer::Stop(), which will eventually raise a notification
			 * on VueceStreamPlayer itself, cause it to call
			 */
			StopStreamPlayerInternal();

			VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopStreamPlayerInternal() returned, player fully released.");

			stop_cmd_issued = false;

			if(enable_reset)
			{
				VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Reset is enabled, will reset all streaming params.");

				enable_reset = false;

				VueceStreamPlayer::ResetCurrentStreamingParams();

				VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Stream param reset is done, change to IDLE now.");

				VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
				VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_Paused, vuece::NetworkPlayerState_Idle);
			}
		}

	}

	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Running loop exited");

//#ifdef ANDROID
//	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Calling AndroidKeyCleanup");
//	VueceJni::AndroidKeyCleanup(NULL);
//#endif

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Unlock release tag");

	mutex_release.Lock();
	released = true;
	mutex_release.Unlock();

	VueceJni::ThreadExit(NULL, THREAD_TAG_PLAYER_MONITOR);

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - Stopped");

	//call detach here???

}


void VueceStreamPlayerMonitorThread2::StopStreamPlayerInternal()
{
	LOG(LS_VERBOSE) << "VueceStreamPlayerMonitorThread2::StopStreamPlayerInternal - Calling VueceStreamPlayer::StopStreamPlayer()";

	VueceStreamPlayer::Stop(false);

	LOG(LS_VERBOSE) << "VueceStreamPlayerMonitorThread2::StopStreamPlayerInternal - Done";
}


void VueceStreamPlayerMonitorThread2::StopSync()
{
	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopSync");

	mutex_running.Lock();
	running = false;
	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopSync - wait until all resources are released");

	mutex_release.Lock();
	while (!released)
	{
		mutex_release.Unlock();
		VueceThreadUtil::SleepSec(1);
		mutex_release.Lock();
	}
	mutex_release.Unlock();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopSync - Done");

}


void VueceStreamPlayerMonitorThread2::StopStreamPlayer()
{
	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopStreamPlayer 1");

	mutex_running.Lock();
	stop_cmd_issued = true;
	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopStreamPlayer 2");
}


void VueceStreamPlayerMonitorThread2::StopAndResetStreamPlayer()
{
	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopAndResetStreamPlayer 1");

	mutex_running.Lock();
	stop_cmd_issued = true;
	enable_reset = true;
	mutex_running.Unlock();

	VueceLogger::Debug("VueceStreamPlayerMonitorThread2::Thread - StopAndResetStreamPlayer 2");
}

