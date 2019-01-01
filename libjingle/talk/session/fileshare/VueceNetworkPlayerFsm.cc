

#include "talk/base/logging.h"
#include "VueceNetworkPlayerFsm.h"
#include "VueceLogger.h"
#include "VueceMediaStreamSessionClient.h"

static JMutex mutex_state;
static vuece::NetworkPlayerState external_network_player_state;
static sigslot::signal1<VueceStreamPlayerNotificaiontMsg*> SignalStreamPlayerNotification;

void VueceNetworkPlayerFsm::Init()
{
	VueceLogger::Info("VueceNetworkPlayerFsm::Init");

	external_network_player_state = vuece::NetworkPlayerState_Idle;

	SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);

	VueceThreadUtil::InitMutex(&mutex_state);
}

void VueceNetworkPlayerFsm::UnInit()
{
	VueceLogger::Info("VueceNetworkPlayerFsm::UnInit");

	if(mutex_state.IsInitialized() )
	{
		mutex_state.Unlock();
	}

	VueceLogger::Info("VueceNetworkPlayerFsm::UnInit - Done");
}

void VueceNetworkPlayerFsm::RegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client)
{
	LOG(LS_INFO) << "VueceNetworkPlayerFsm - RegisterUpperLayerNotificationListener";
	SignalStreamPlayerNotification.connect(client, &cricket::VueceMediaStreamSessionClient::OnStreamPlayerNotification);
}

void VueceNetworkPlayerFsm::UnRegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client)
{
	LOG(LS_INFO) << "VueceNetworkPlayerFsm - UnRegisterUpperLayerNotificationListener";
	SignalStreamPlayerNotification.disconnect(client);
}

void VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s)
{
	VueceStreamPlayerNotificaiontMsg notification;

	VueceLogger::Debug("VueceNetworkPlayerFsm - FireNetworkPlayerStateChangeNotification: event id =  %d", e);

	memset(&notification, 0, sizeof(VueceStreamPlayerNotificaiontMsg));

	notification.notificaiont_type  = VueceStreamPlayerNotificationType_StateChange;
	notification.value1 = e;
	notification.value2 = s;

	SignalStreamPlayerNotification(&notification);
}

void VueceNetworkPlayerFsm::FireNotification(VueceStreamPlayerNotificaiontMsg* notification)
{
	SignalStreamPlayerNotification(notification);
}

vuece::NetworkPlayerState VueceNetworkPlayerFsm::GetNetworkPlayerState(void)
{
	vuece::NetworkPlayerState s;

	LOG(INFO) << "VueceNetworkPlayerFsm::GetNetworkPlayerState";

	VueceThreadUtil::MutexLock(&mutex_state);

	s = external_network_player_state;

	VueceThreadUtil::MutexUnlock(&mutex_state);

	LogNetworkPlayerState(external_network_player_state);

	return s;
}

void VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState new_state)
{
	VueceLogger::Debug("VueceNetworkPlayerFsm::SetNetworkPlayerState : %d", (int)new_state);

	VueceThreadUtil::MutexLock(&mutex_state);

	vuece::NetworkPlayerState pre_s = external_network_player_state;

	external_network_player_state = new_state;

	VueceThreadUtil::MutexUnlock(&mutex_state);

	LogStateTranstion(pre_s, new_state);
}


void VueceNetworkPlayerFsm::LogNetworkPlayerState(vuece::NetworkPlayerState s)
{

	switch(s)
	{
	case vuece::NetworkPlayerState_Idle:
		LOG(INFO) << "VueceNetworkPlayerFsm::LogNetworkPlayerState - IDLE"; break;
	case vuece::NetworkPlayerState_Waiting:
		LOG(INFO) << "VueceNetworkPlayerFsm::LogNetworkPlayerState - WAITING"; break;
	case vuece::NetworkPlayerState_Playing:
		LOG(INFO) << "VueceNetworkPlayerFsm::LogNetworkPlayerState - PLAYING"; break;
	case vuece::NetworkPlayerState_Buffering:
		LOG(INFO) << "VueceNetworkPlayerFsm::LogNetworkPlayerState - BUFFERING"; break;
	default:
		VueceLogger::Fatal("VueceNetworkPlayerFsm::LogNetworkPlayerState - Unknown state: %d", (int)s); break;
	}

}

void VueceNetworkPlayerFsm::LogStateTranstion(vuece::NetworkPlayerState pre_state, vuece::NetworkPlayerState new_state)
{
	char buf1[16];
	char buf2[16];

	GetNetworkPlayerStateString(pre_state, buf1);
	GetNetworkPlayerStateString(new_state, buf2);

	VueceLogger::Info("VueceNetworkPlayerFsm::STATE TRANSTION - State is switched from [%s] ---> [%s]", buf1, buf2);

}

void VueceNetworkPlayerFsm::GetNetworkPlayerStateString(vuece::NetworkPlayerState s, char* buf)
{
	switch(s)
	{
	case vuece::NetworkPlayerState_Idle:
		strcpy(buf,  "IDLE"); break;
	case vuece::NetworkPlayerState_Waiting:
		strcpy(buf,  "WAITING"); break;
	case vuece::NetworkPlayerState_Playing:
		strcpy(buf,  "PLAYING"); break;
	case vuece::NetworkPlayerState_Buffering:
		strcpy(buf,  "BUFFERING"); break;
	default:
		VueceLogger::Fatal("VueceNetworkPlayerFsm::GetNetworkPlayerStateString - Unknown s: %d", (int)s); break;
	}
}

void VueceNetworkPlayerFsm::GetNetworkPlayerEventString(vuece::NetworkPlayerEvent e, char* buf)
{
	switch(e)
	{
	case vuece::NetworkPlayerEvent_PlayReqSent:
		strcpy(buf,  "PlayReqSent"); break;
	case vuece::NetworkPlayerEvent_StreamSessionStarted:
		strcpy(buf,  "StreamSessionStarted"); break;
	case vuece::NetworkPlayerEvent_Buffering:
		strcpy(buf,  "Buffering"); break;
	case vuece::NetworkPlayerEvent_Started:
		strcpy(buf,  "Started"); break;
	case vuece::NetworkPlayerEvent_Resumed:
		strcpy(buf,  "Resumed"); break;
	case vuece::NetworkPlayerEvent_Paused:
		strcpy(buf,  "Paused"); break;
	case vuece::NetworkPlayerEvent_EndOfSong:
		strcpy(buf,  "EndOfSong"); break;
	case vuece::NetworkPlayerEvent_OperationTimedout:
		strcpy(buf,  "OperationTimedout"); break;
	case vuece::NetworkPlayerEvent_MediaNotFound:
		strcpy(buf,  "MediaNotFound"); break;
	case vuece::NetworkPlayerEvent_NetworkErr:
		strcpy(buf,  "NetworkErr"); break;
	case vuece::NetworkPlayerEvent_OpStarted:
		strcpy(buf,  "OperationStarted"); break;
	default:
		VueceLogger::Fatal("VueceNetworkPlayerFsm::GetNetworkPlayerEventString - Unknown e: %d", (int)e); break;
	}
}

