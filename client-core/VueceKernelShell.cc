#define _CRT_SECURE_NO_DEPRECATE 1

#ifdef POSIX
#include <unistd.h>
#endif  // POSIX
#include <cassert>
#include "talk/base/logging.h"
#include "talk/base/messagequeue.h"
#include "talk/base/stringutils.h"

#include "talk/session/fileshare/VueceMediaStreamSessionClient.h"

#include "VueceCoreClient.h"

#include "VueceGlobalSetting.h"

#include "VueceKernelShell.h"
#include "VueceConstants.h"

#include "VueceLogger.h"

#ifdef POSIX
#include <signal.h>
#endif

#ifdef ANDROID
#include "VueceStreamPlayer.h"
#include "VueceNetworkPlayerFsm.h"
#endif


#define VUECE_FOLDER_SEPARATOR 0x1C

VueceKernelShell::VueceKernelShell(talk_base::Thread *thread,
		VueceCoreClient *client) :
		core_client(client), client_thread_(thread)
{
	LOG(LS_VERBOSE)
			<< "VueceKernelShell - Constructor called, create working thread";

	console_thread_ = new talk_base::Thread();
	console_thread_->SetName("VueceKernelShell Thread", NULL);

	timeout_wait_session_released = VUECE_TIMEOUT_WAIT_SESSION_RELEASED;
	session_release_received = false;
	player_release_received = false;

	VueceThreadUtil::InitMutex(&mutex_wait_session_release);
	VueceThreadUtil::InitMutex(&mutex_wait_player_release);

	LOG(LS_VERBOSE) << "VueceKernelShell - Constructor called";
}

VueceKernelShell::~VueceKernelShell()
{
	LOG(LS_VERBOSE) << "VueceKernelShell - Destructor called";
	Stop();
}

void VueceKernelShell::Start()
{
	LOG(LS_VERBOSE) << "------------------------";
	LOG(LS_VERBOSE) << "VueceKernelShell - Start";
	LOG(LS_VERBOSE) << "------------------------";

//  if (!console_thread_) {
//    // stdin was closed in Stop(), so we can't restart.
//    LOG(LS_ERROR) << "VueceKernelShell - Cannot re-start";
//    return;
//  }

	if (console_thread_->started())
	{
		LOG(LS_WARNING) << "VueceKernelShell - Already started";
		return;
	}

	LOG(LS_VERBOSE) << "VueceKernelShell - Start: Post VUECE_MSG_START";

	console_thread_->Start();
	console_thread_->Post(this, VUECE_MSG_START);

	LOG(LS_VERBOSE) << "------------------------";
	LOG(LS_VERBOSE) << "VueceKernelShell - Started";
	LOG(LS_VERBOSE) << "------------------------";
}

void VueceKernelShell::Stop()
{
	LOG(LS_VERBOSE) << "------------------------";
	LOG(LS_VERBOSE) << "VueceKernelShell::Stop";
	LOG(LS_VERBOSE) << "------------------------";

	if (console_thread_->started())
	{

		LOG(LS_VERBOSE) << "VueceKernelShell::Stop - Console thread started, stop it now";

//#ifdef WIN32
//    CloseHandle(GetStdHandle(STD_INPUT_HANDLE));
//#else
//    close(fileno(stdin));
//    // This forces the read() in fgets() to return with errno = EINTR. fgets()
//    // will retry the read() and fail, thus returning.
//    pthread_kill(console_thread_->GetPThread(), SIGUSR1);
//#endif
		console_thread_->Stop();
//    console_thread_->reset();
	}

	LOG(LS_VERBOSE) << "------------------------";
	LOG(LS_VERBOSE) << "VueceKernelShell::Stop";
	LOG(LS_VERBOSE) << "------------------------";
}

#ifdef ANDROID
vuece::VueceResultCode VueceKernelShell::PlayMusic(const std::string &jid, const std::string& song_uuid)
{
	vuece::VueceResultCode ret = vuece::RESULT_OK;

	bool pause_st_check = true;
	bool skip_pase = false;

	char msg[512];
	int i;

	LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Start";

	memset(msg, 0, sizeof(msg));

	/*
	 * TODO
	 * 1. Deny if network player is in WAITING state
	 * 2. Immediately switch to WAITING state if player is in acceptable state.
	 */
	VueceGlobalContext::SetCurrentTransactionCancelled(false);

	vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();
	if(ns == vuece::NetworkPlayerState_Waiting)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Player is in WAITING state, op not allowed.";
		ret = vuece::RESULT_FUNC_NOT_ALLOWED;
		return ret;
	}
	else if(ns == vuece::NetworkPlayerState_Buffering || ns == vuece::NetworkPlayerState_Playing)
	{
		pause_st_check = false;
		LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Player is current in a running state(buffering or playing), need to pause it at first";
	}
	if(ns == vuece::NetworkPlayerState_Idle)
	{
//		pause_st_check = false;
		LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Player is in IDLE so PAUSE can be skipped.";
		skip_pase = true;
	}

	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_OpStarted, vuece::NetworkPlayerState_Waiting);

	strcpy(msg, jid.c_str());
	i = strlen(msg);
	msg[i] = VUECE_FOLDER_SEPARATOR;
	strcat(msg, song_uuid.c_str());

	bool need_wait = false;
	int wait_timer = 0;

	if(!skip_pase)
	{
		PauseMusic(pause_st_check);
	}

	LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Start, issue play message now.";

	if(VueceGlobalContext::IsCurrentTransactionCancelled())
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic TRICKYDEBUG - Current transaction has been cancelled, operation will not proceed.";

		return vuece::RESULT_FUNC_NOT_ALLOWED;
	}

	//make sure current state is WAITING because we don't allow interruption here
	ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();
	if(ns != vuece::NetworkPlayerState_Waiting)
	{
		VueceLogger::Fatal("VueceKernelShell::PlayMusic - network player state(%d) is not WAITING after PAUSE, abort now", (int)ns);
		return ret;
	}

	OnVueceCommandReceived(VUECE_CMD_PLAY_MUSIC, msg);

	LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - End";

	ret = vuece::RESULT_OK;

	return ret;
}

vuece::VueceResultCode VueceKernelShell::SeekMusic(int pos)
{
	vuece::VueceResultCode ret = vuece::RESULT_OK;

	char msg[24];
	int i;

	LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Start";

	vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();
	if(ns == vuece::NetworkPlayerState_Waiting)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Player is in WAITING state, op not allowed.";
		return vuece::RESULT_FUNC_NOT_ALLOWED;
	}

	//switch to WAITING immediately so Ui layer can enter WAITING state as early as possible to block potential user
	//unusual behavior
	LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Switch to WAITING state now";

	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_OpStarted, vuece::NetworkPlayerState_Waiting);

	VueceGlobalContext::SetCurrentTransactionCancelled(false);

	if(ns != vuece::NetworkPlayerState_Idle)
	{
		//TODO - Do a sync stop if current state is PLAYING
		if(ns == vuece::NetworkPlayerState_Playing)
		{
			LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - -------------------------------------------------";
			LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Player is in PLAYING state, stop at first.";

			VueceStreamPlayer::LogCurrentFsmState();
			VuecePlayerFsmState s = VueceStreamPlayer::FsmState();

			if(s == VuecePlayerFsmState_WaitingForNextBufWin)
			{
				LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic, TRICKYDEBUG, wait for next buf win, incoming session will be rejected.";
				VueceGlobalContext::SetDeclineSessionForNextBufWin(true);
			}

			//TTT
			PauseMusic(false);
		}
		else if(ns == vuece::NetworkPlayerState_Buffering)
		{
			LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - -------------------------------------------------";
			LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Player is in BUFFERING state, stop at first.";
			//TTT

			VueceStreamPlayer::LogCurrentFsmState();
			VuecePlayerFsmState s = VueceStreamPlayer::FsmState();

			if(s == VuecePlayerFsmState_WaitingForNextBufWin)
			{
				LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic, TRICKYDEBUG, wait for next buf win, incoming session will be rejected.";
				VueceGlobalContext::SetDeclineSessionForNextBufWin(true);
			}

			PauseMusic(false);
		}
		else
		{
			VueceLogger::Warn("VueceKernelShell::SeekMusic - current network player state is not IDLE or PLAYING, operation not allowed");
			return vuece::RESULT_FUNC_NOT_ALLOWED;
		}

	}

	LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - -------------------------------------------------------------";
	LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Player is guaranteed to be stopped now, continue.";

	if(VueceGlobalContext::IsCurrentTransactionCancelled())
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - Current transaction has been cancelled, operation will not proceed.";

		return vuece::RESULT_FUNC_NOT_ALLOWED;
	}

	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_OpStarted, vuece::NetworkPlayerState_Waiting);

	memset(msg, 0, sizeof(msg));

	sprintf(msg, "%d", pos);
	i = strlen(msg);
	msg[i] = VUECE_FOLDER_SEPARATOR;

	OnVueceCommandReceived(VUECE_CMD_SEEK_MUSIC, msg);

	LOG(LS_VERBOSE) << "VueceKernelShell::SeekMusic - End";

	return ret;
}

vuece::VueceResultCode VueceKernelShell::PauseMusic(bool pause_st_check)
{
	vuece::VueceResultCode ret = vuece::RESULT_OK;
	VuecePlayerFsmState play_st = VuecePlayerFsmState_Ready;

	LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Start";

	//validate current state
	if(pause_st_check)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Start, state check is needed";

		vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();

		if(ns == vuece::NetworkPlayerState_Waiting)
		{
			LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Player is in WAITING state, op not allowed.";
			ret = vuece::RESULT_FUNC_NOT_ALLOWED;
			return ret;
		}

		if(ns == vuece::NetworkPlayerState_Idle)
		{
			LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Player is in IDLE state, op not allowed.";
			ret = vuece::RESULT_FUNC_NOT_ALLOWED;
			return ret;
		}

		if(ns == vuece::NetworkPlayerState_None)
		{
			LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Player is in NONE state, op not allowed.";
			ret = vuece::RESULT_FUNC_NOT_ALLOWED;
			return ret;
		}

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
		core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_OpStarted, vuece::NetworkPlayerState_Waiting);
	}

	VueceGlobalContext::SetDeclineSessionForNextBufWin(true);
	VueceGlobalContext::SetStopPlayStarted(true);
	LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - TROUBLESHOOTING - bStopPlayStarted -> true";

	core_client->GetStreamSessionClien()->ListAllSessionIDs();

	int session_nr = core_client->GetStreamSessionClien()->GetSessionNr();
	bool need_wait = false;
	int wait_timer = 0;

	if(session_nr > 0)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Current active session number is: " << session_nr << ", need to wait for session release message.";
	}
	else
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - There is no active session, no need to wait for session release.";
	}

	VueceThreadUtil::MutexLock(&mutex_wait_player_release);
	player_release_received = false;
	VueceThreadUtil::MutexUnlock(&mutex_wait_player_release);

	LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - issue VUECE_CMD_PAUSE_MUSIC command";

	OnVueceCommandReceived(VUECE_CMD_PAUSE_MUSIC, NULL);

	if(session_nr > 0)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Kernel cmd issued, start waiting with timeout: " << timeout_wait_session_released;

		VueceThreadUtil::MutexLock(&mutex_wait_session_release);
		if(session_release_received)
		{
			LOG(INFO) << "VueceKernelShell::PauseMusic - session_release_received is currently true, will be reset to false";
			session_release_received = false;
		}
		VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);


		/*
		 * Note - 	VueceGlobalContext::SetSessionDeclinedLocally(true) in VueceMediaStreamSessionClient.cc could
		 * happen before this because it's called in different thread, in this case, VueceGlobalContext::IsSessionDeclinedLocally()
		 * will return false here, then we have to wait until the session release timer expires
		 */
		if(VueceGlobalContext::IsSessionDeclinedLocally())
		{
			VueceGlobalContext::SetSessionDeclinedLocally(false);
			LOG(INFO) << "VueceKernelShell::PauseMusic  - TRICKYDEBUG: session declined locally, no need to wait for release message from HUB";
		}
		else
		{
			while(true)
			{
				LOG(INFO) << "VueceKernelShell::PauseMusic  - Waiting remote session release message, current timer = " << wait_timer;

				//check flag
				VueceThreadUtil::MutexLock(&mutex_wait_session_release);

				if(session_release_received)
				{
					LOG(INFO) << "VueceKernelShell::PauseMusic  - Session release msg received.";
					//reset this flag
					session_release_received = false;

					//must reset this otherwise next normal incomming session will be declined
					VueceGlobalContext::SetDeclineSessionForNextBufWin(false);

					VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);
					break;
				}

				VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

				VueceThreadUtil::SleepSec(1);
				wait_timer++;

				if(wait_timer >= timeout_wait_session_released)
				{
					LOG(INFO) << "VueceKernelShell::PauseMusic  - Waiting remote session release message timed out, give up waiting.";
					break;
				}
			}

			LOG(INFO) << "VueceKernelShell::PauseMusic  - Waiting remote session release message, loop exited, current timer = " << wait_timer;

		}
	}

	LOG(INFO) << "VueceKernelShell::PauseMusic  - D2";

	session_release_received = false;

	VueceGlobalContext::SetDeclineSessionForNextBufWin(false);

	//now check if player is released
	VueceStreamPlayer::LogCurrentFsmState();

	play_st = VueceStreamPlayer::FsmState();
	if(play_st == VuecePlayerFsmState_Ready || play_st == VuecePlayerFsmState_Stopped)
	{
		LOG(INFO) << "VueceKernelShell::PauseMusic  - No need to wait for player release signal because player is already stopped or idle.";
	}
	else
	{
		LOG(INFO) << "VueceKernelShell::PauseMusic  - Start checking player release signal with timeout: " << timeout_wait_session_released;

		wait_timer = 0;

		while(true)
		{
			LOG(INFO) << "VueceKernelShell::PauseMusic  - Waiting player release signal";

			//check flag
			VueceThreadUtil::MutexLock(&mutex_wait_player_release);

			if(player_release_received)
			{
				LOG(INFO) << "VueceKernelShell::PauseMusic  - player release signal received.";
				//reset this flag
				player_release_received = false;
				VueceThreadUtil::MutexUnlock(&mutex_wait_player_release);
				break;
			}

			VueceThreadUtil::MutexUnlock(&mutex_wait_player_release);

			VueceThreadUtil::SleepSec(1);

			wait_timer++;

			if(wait_timer >= timeout_wait_session_released)
			{
				LOG(LS_ERROR) << "VueceKernelShell::PauseMusic  - Waiting remote session release message timed out, give up waiting.";
				break;
			}

		}

		player_release_received = false;

	}

	if(pause_st_check)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Firing notification [PAUSED, IDLE]";

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_Paused, vuece::NetworkPlayerState_Idle);
	}
	else
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - Finished but notification was skipped.";
	}

	//reset some flags
	VueceGlobalContext::SetStopPlayStarted(false);
	VueceGlobalContext::SetSessionDeclinedLocally(false);

	LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - TROUBLESHOOTING - bStopPlayStarted -> false";

	LOG(LS_VERBOSE) << "VueceKernelShell::PauseMusic - End";

	return ret;
}

vuece::VueceResultCode VueceKernelShell::ResumeMusic()
{
	vuece::VueceResultCode ret = vuece::RESULT_OK;

	LOG(LS_VERBOSE) << "VueceKernelShell::ResumeMusic - Start";

	vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();
	if(ns == vuece::NetworkPlayerState_Waiting)
	{
		LOG(LS_VERBOSE) << "VueceKernelShell::PlayMusic - Player is in WAITING state, op not allowed.";
		ret = vuece::RESULT_FUNC_NOT_ALLOWED;
		return ret;
	}

	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_OpStarted, vuece::NetworkPlayerState_Waiting);

	OnVueceCommandReceived(VUECE_CMD_RESUME_MUSIC, NULL);

	LOG(LS_VERBOSE) << "VueceKernelShell::ResumeMusic - End";

	return ret;
}

#endif

void VueceKernelShell::OnMessage(talk_base::Message *msg)
{

	LOG(LS_VERBOSE) << "VueceKernelShell:OnMessage:message_id = "
			<< msg->message_id;

	uint32 cmd_id = msg->message_id;

	switch (cmd_id)
	{

	case VUECE_MSG_START:
	{
		LOG(INFO)
				<< "VueceKernelShell:OnMessage:VUECE_MSG_START, do nothing";
		break;
	}

#ifdef ENABLE_PHONE_ENGINE
		case VUECE_CMD_ACCEPT_CALL:
		{
			LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_ACCEPT_CALL";
			core_client->VueceCmdAcceptCall();
			break;
		}

		case VUECE_CMD_REJECT_CALL:
		{
			LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_REJECT_CALL";
			core_client->VueceCmdRejectCall();
			break;
		}

		case VUECE_CMD_HANG_UP:
		{
			LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_HANG_UP";
			core_client->VueceCmdDestroyCall();
			break;
		}

		case VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET:
		{
			char param1[128];
			char param2[128];
			int i = 0;
			int j = 0;
			int k = 0;

			LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET";

			memset(param1, '\0', sizeof(param1));
			memset(param2, '\0', sizeof(param2));

			talk_base::TypedMessageData<std::string> *data =
			static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

			std::string msgData = data->data();
			LOG(INFO) << "VueceKernelShell:OnMessage:Voice call target is: " << msgData;

			const char* msgPtr = msgData.c_str();

			j = strlen(msgPtr);

			//split msg to param1 and param2
			for(i = 0; i < j; i++)
			{
				if(msgPtr[i] == VUECE_FOLDER_SEPARATOR)
				{
					break;
				}

				param1[i] = msgPtr[i];
			}

			LOG(INFO) << "VueceKernelShell:OnMessage:Voice call command: param1 extracted: " << param1;

			i++; //skip FS
			for(k = 0; i < j; i++)
			{
				if(msgPtr[i] == VUECE_FOLDER_SEPARATOR)
				{
					break;
				}
				param2[k++] = msgPtr[i];
			}

			LOG(INFO) << "VueceKernelShell:OnMessage:Voice call command: param2 extracted: " << param2;

			core_client->VueceCmdMakeCallTo(param1, param2, false);
			break;
		}
#endif

	case VUECE_CMD_PAUSE_MUSIC:
	{
		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_PAUSE_MUSIC";

		core_client->PauseInternal();

		break;
	}

	case VUECE_CMD_RESUME_MUSIC:
	{
		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_RESUME_MUSIC";

		core_client->ResumeInternal();

		break;
	}

	case VUECE_CMD_SEEK_MUSIC:
	{
		int i = 0;
		int j = 0;

		char param1[24]; // jid
		memset(param1, '\0', sizeof(param1));

		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_SEEK_MUSIC";

		talk_base::TypedMessageData<std::string> *data =
				static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

		std::string msg = data->data();
		const char* msgPtr = msg.c_str();

		LOG(INFO) << "VueceKernelShell:OnMessage:Seek music command received:" << msg;

		j = strlen(msgPtr);

		//split msg to param1 and param2
		for (i = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}

			param1[i] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage: - Seek postion: "
				<< param1;

		int seek_pos = atoi(param1);

		core_client->SeekInternal(seek_pos);

		break;
	}
	case VUECE_CMD_PLAY_MUSIC:
	{
		char param1[256]; // jid
		char param2[256]; // song_uuid

		int i = 0;
		int j = 0;
		int k = 0;

		memset(param1, '\0', sizeof(param1));
		memset(param2, '\0', sizeof(param2));

		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_PLAY_MUSIC";

		talk_base::TypedMessageData<std::string> *data =
				static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

		std::string msg = data->data();
		const char* msgPtr = msg.c_str();

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:Play song command received:"
				<< msg;

		j = strlen(msgPtr);

		//split msg to param1 and param2
		for (i = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}

			param1[i] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command: jid extracted: "
				<< param1;

		i++; //skip FS
		for (k = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}
			param2[k++] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command: song uuid extracted: "
				<< param2;

		std::string jid(param1);
		std::string uuid(param2);

		core_client->PlayInternal(jid, uuid);

		break;
	}

	case VUECE_CMD_ACCEPT_FILE:
	{
		char param1[54]; // share_id
		char param5[64]; // target_folder
		char param6[64]; // target_file_name

		int i = 0;
		int j = 0;
		int k = 0;

		memset(param1, '\0', sizeof(param1));
		memset(param5, '\0', sizeof(param5));
		memset(param6, '\0', sizeof(param6));

		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_ACCEPT_FILE";

		talk_base::TypedMessageData<std::string> *data =
				static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

		std::string msg = data->data();
		const char* msgPtr = msg.c_str();

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command received:"
				<< msg;

		j = strlen(msgPtr);

		//split msg to param1 and param2
		for (i = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}

			param1[i] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command: param1 extracted: "
				<< param1;

		i++; //skip FS
		for (k = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}
			param5[k++] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command: param5 extracted: "
				<< param5;

		i++; //skip FS
		for (k = 0; i < j; i++)
		{
			if (msgPtr[i] == VUECE_FOLDER_SEPARATOR)
			{
				break;
			}
			param6[k++] = msgPtr[i];
		}

		LOG(INFO)
				<< "VueceKernelShell:OnMessage:accept file command: param6 extracted: "
				<< param6;

		core_client->AcceptFileShare(param1, param5, param6);

		break;
	}
	case VUECE_CMD_CANCEL_FILE:
	{
		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_CANCEL_FILE";
		talk_base::TypedMessageData<std::string> *data =
				static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

		std::string target = data->data();
		LOG(INFO) << "VueceKernelShell:OnMessage:cancel sharing file: "
				<< target;

		core_client->CancelFileShare(target);
		break;
	}
	case VUECE_CMD_DECLINE_FILE:
	{
		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_DECLINE_FILE";
		talk_base::TypedMessageData<std::string> *data =
				static_cast<talk_base::TypedMessageData<std::string>*>(msg->pdata);

		std::string target = data->data();
		LOG(INFO) << "VueceKernelShell:OnMessage:decline file share: "
				<< target;

		core_client->DeclineFileShare(target);
		break;
	}

	case VUECE_CMD_SIGN_OUT:
	{
		LOG(INFO) << "VueceKernelShell:OnMessage:VUECE_CMD_SIGN_OUT";
		core_client->Quit();
		break;
	}

	default:
	{
		LOG(INFO)
				<< "VueceKernelShell:OnMessage:This message is not handled";
		break;
	}

	} // end of switch

} // end of VueceKernelShell::OnMessage

void VueceKernelShell::OnVueceCommandReceived(int cmdIdx, const char* cmdString)
{
	LOG(INFO)
			<< "---------------------------------------------------------------------------------------------------------------";
	LOG(INFO) << "VueceKernelShell::OnVueceCommandReceived: " << cmdIdx;
	LOG(INFO)
			<< "---------------------------------------------------------------------------------------------------------------";

	switch (cmdIdx)
	{
	case VUECE_CMD_SEND_FILE:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_SEND_FILE";

		client_thread_->Post(this, VUECE_CMD_SEND_FILE,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}

	case VUECE_CMD_PLAY_MUSIC:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_PLAY_MUSIC";

		client_thread_->Post(this, VUECE_CMD_PLAY_MUSIC,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}

	case VUECE_CMD_PAUSE_MUSIC:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_PAUSE_MUSIC";

		client_thread_->Post(this, VUECE_CMD_PAUSE_MUSIC,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}

	case VUECE_CMD_RESUME_MUSIC:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_RESUME_MUSIC";

		client_thread_->Post(this, VUECE_CMD_RESUME_MUSIC,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}

	case VUECE_CMD_SEEK_MUSIC:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_SEEK_MUSIC";

		client_thread_->Post(this, VUECE_CMD_SEEK_MUSIC,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}

	case VUECE_CMD_ACCEPT_FILE:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_ACCEPT_FILE";

		client_thread_->Post(this, VUECE_CMD_ACCEPT_FILE,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}
	case VUECE_CMD_ACCEPT_CALL:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_ACCEPT_CALL";

		client_thread_->Post(this, VUECE_CMD_ACCEPT_CALL,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}
	case VUECE_CMD_REJECT_CALL:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_REJECT_CALL";

		client_thread_->Post(this, VUECE_CMD_REJECT_CALL,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}
	case VUECE_CMD_HANG_UP:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_HANG_UP";

		client_thread_->Post(this, VUECE_CMD_HANG_UP,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}
	case VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET";

		client_thread_->Post(this, VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}
	case VUECE_CMD_PLACE_VIDEO_CALL_TO_REMOTE_TARGET:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_PLACE_VIDEO_CALL_TO_REMOTE_TARGET";

		client_thread_->Post(this, VUECE_CMD_PLACE_VIDEO_CALL_TO_REMOTE_TARGET,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}
	case VUECE_CMD_SIGN_OUT:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_SIGN_OUT";

		client_thread_->Post(this, VUECE_CMD_SIGN_OUT,
				new talk_base::TypedMessageData<std::string>(""));

		return;
	}
	case VUECE_CMD_CANCEL_FILE:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_CANCEL_FILE";

		client_thread_->Post(this, VUECE_CMD_CANCEL_FILE,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}
	case VUECE_CMD_DECLINE_FILE:
	{
		LOG(INFO)
				<< "VueceKernelShell::OnVueceCommandReceived: VUECE_CMD_DECLINE_FILE";

		client_thread_->Post(this, VUECE_CMD_DECLINE_FILE,
				new talk_base::TypedMessageData<std::string>(cmdString));

		return;
	}
	default:
	{
		VueceLogger::Fatal("VueceKernelShell::OnVueceCommandReceived - Unknown command: %d, abort now.", cmdIdx);
		break;
	}

	}
} //end of VueceKernelShell::OnVueceCommandReceived

void VueceKernelShell::OnRemoteSessionResourceReleased(const std::string& sid)
{
	LOG(INFO) << "VueceKernelShell::OnRemoteSessionResourceReleased:Session id passed in: " << sid;

	//terminate current active session, maybe we use the sid to locate the actual session later
	VueceThreadUtil::MutexLock(&mutex_wait_session_release);

	if(session_release_received)
	{
		LOG(WARNING) << "WARNING - VueceKernelShell::OnRemoteSessionResourceReleased - Already released";
//		abort();
	}

	session_release_received = true;
	VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

}

void VueceKernelShell::OnInvalidStreamingTargetMsgReceived()
{
	LOG(INFO) << "VueceKernelShell::OnInvalidStreamingTargetMsgReceived";

#ifdef ANDROID
	//No need to call PauseMusic at all, because we know the play has started yet.
	//simply change current state back to idle
	VueceStreamPlayer::LogCurrentFsmState();

	//At this point player state must be stopped or ready
	VuecePlayerFsmState play_st = VueceStreamPlayer::FsmState();
	if(play_st == VuecePlayerFsmState_Ready || play_st == VuecePlayerFsmState_Stopped)
	{
		LOG(INFO) << "VueceKernelShell::OnInvalidStreamingTargetMsgReceived  - Player is in expected state.";
	}
	else
	{
		VueceLogger::Fatal("VueceKernelShell::OnInvalidStreamingTargetMsgReceived  - Player is not in expected state, abort.");
	}

	LOG(INFO) << "VueceKernelShell::OnInvalidStreamingTargetMsgReceived  - Set state to IDLE";

	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_MediaNotFound, vuece::NetworkPlayerState_Idle);

	//PauseMusic(false);
#endif

	LOG(INFO) << "VueceKernelShell::OnInvalidStreamingTargetMsgReceived - Done";
}

void VueceKernelShell::OnPlayerReleased(void)
{
	LOG(INFO) << "VueceKernelShell::OnPlayerReleased";

	//terminate current active session, maybe we use the sid to locate the actual session later
	VueceThreadUtil::MutexLock(&mutex_wait_player_release);

	if(player_release_received)
	{
		LOG(WARNING) << "WARNING - VueceKernelShell::OnPlayerReleased - Already released";
//		abort();
	}

	player_release_received = true;
	VueceThreadUtil::MutexUnlock(&mutex_wait_player_release);
}



