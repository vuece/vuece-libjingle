/*
 * VueceNativeInterface.h
 *
 *  Created on: 2014-9-28
 *      Author: Jingjing Sun
 */

#ifndef VUECENATIVEINTERFACE_H_
#define VUECENATIVEINTERFACE_H_

#include <map>

#include "status.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/base/sigslot.h"
#include "talk/session/phone/mediasessionclient.h"
#include "VueceConstants.h"

namespace vuece
{

typedef enum _VueceResultCode {
   RESULT_FUNC_NOT_ALLOWED 	 	= -1,
   RESULT_OK  		            = 0,
   RESULT_INVALID_PARAM         = 1,
   RESULT_NO_CONNECTION         = 2,
   RESULT_UNAUTHORIZED 	 	    = 3,
   RESULT_GENERAL_ERR		    = 4
}VueceResultCode ;

typedef enum _ClientState{
   CLIENT_STATE_OFFLINE  	    	= 0,
   CLIENT_STATE_CONNECTING   		= 1,
   CLIENT_STATE_ONLINE,
   CLIENT_STATE_DISCONNECTING,
   CLIENT_STATE_NONE
}ClientState;

typedef enum _ClientEvent {
   CLIENT_EVENT_CLIENT_INITIATED        = 100,
   CLIENT_EVENT_LOGGING_IN			   	= 190,
   CLIENT_EVENT_LOGGING_OUT  			= 191,
   CLIENT_EVENT_LOGIN_OK                = 200,
   CLIENT_EVENT_LOGOUT_OK 				= 201,
   CLIENT_EVENT_OPERATION_TIMEOUT 	    = 400,
   CLIENT_EVENT_AUTH_MISSING_PARAM      = 500,
   CLIENT_EVENT_AUTH_ERR                = 501,
   CLIENT_EVENT_NETWORK_ERR             = 502,
   CLIENT_EVENT_SYSTEM_ERR              = 503,
   CLIENT_EVENT_NONE
}ClientEvent;


//see same definitions in VTalkListener.java
typedef enum _Vuece_NetworkPlayerEvent{
	NetworkPlayerEvent_PlayReqSent       	= 100,
	NetworkPlayerEvent_StreamSessionStarted = 101,
	NetworkPlayerEvent_Buffering         	= 102,
	NetworkPlayerEvent_Started           = 200,
	NetworkPlayerEvent_Resumed           = 210,
	NetworkPlayerEvent_Paused            = 220,
	NetworkPlayerEvent_EndOfSong         = 230,
	NetworkPlayerEvent_OpStarted         = 231,
	NetworkPlayerEvent_OperationTimedout = 400,
	NetworkPlayerEvent_MediaNotFound     = 500,
	NetworkPlayerEvent_NetworkErr        = 501
}NetworkPlayerEvent;


typedef enum _Vuece_NetworkPlayerState{
	NetworkPlayerState_Idle = 0,
	NetworkPlayerState_Waiting,
	NetworkPlayerState_Playing,
	NetworkPlayerState_Buffering,
	NetworkPlayerState_None
}NetworkPlayerState;

typedef struct InitData_{
	void *p0;
	void *p1;
	void *p2;
	void *p3; //device name
	void *p4; //application version
	int logging_level;
}InitData;

struct RosterItem {
  buzz::Jid jid;
  buzz::Status::Show show;
  std::string status;
  std::string device_name;
  std::string version;
  std::string full_name;
  std::string image_data;
};

typedef std::map<std::string, RosterItem> RosterMap;

class VueceNativeInterface : public sigslot::has_slots<>
{
public:
//	VueceNativeInterface();
//	virtual ~VueceNativeInterface() = 0;

public:
	//Log in interface
	virtual int Start(const char* name, const char* pwd, const int auth_type) = 0;
	virtual int LogOut(void) = 0;

	//log in state interface
	virtual vuece::ClientState GetClientState(void) = 0;
	virtual void OnClientStateChanged(vuece::ClientEvent event, vuece::ClientState state) = 0;

	//hub messages
	virtual int 	SendVHubMessage(const std::string& to, const std::string& type,const std::string& message) = 0;
	virtual int 	SendVHubPlayRequest(const std::string& to, const std::string& type,const std::string& message, const std::string& uri) = 0;

	virtual void 	OnVHubGetMessageReceived(const buzz::Jid& jid, const std::string& message) = 0;
	virtual void 	OnVHubResultMessageReceived(const buzz::Jid& jid, const std::string& message) = 0;

	//roster management
	virtual void OnRosterStatusUpdate(const buzz::Status& status) = 0;
	virtual void OnRosterReceived(const buzz::XmlElement* stanza) = 0;
	virtual void OnRosterSubRespReceived(VueceRosterSubscriptionMsg* m) = 0;
	virtual void SendSubscriptionMessage(const std::string& to, int type) = 0;
	virtual void AddBuddy(const std::string& jid) = 0;
	virtual void SendPresence(const std::string& status, const std::string& signature) = 0;
	virtual void SendPresence(const std::string& status) = 0;
	virtual void SendSignature(const std::string& sig) = 0;
	virtual RosterMap* GetRosterMap() = 0;

#ifdef VCARD_ENABLED
	virtual void SendVCardRequest(const std::string& to) = 0;
	virtual void OnRosterVCardReceived(const std::string& jid, const std::string& fn, const std::string& img_b64) = 0;
#endif

	//music playing
	virtual vuece::NetworkPlayerState  GetNetworkPlayerState(void) = 0;
	virtual void OnNetworkPlayerStateChanged(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s) = 0;
	virtual void OnStreamPlayerStateChanged(const std::string& share_id, int state) = 0;
//	virtual void StopStreamPlayer(const std::string& sid) = 0;
//	virtual void ResumeStreamPlayer(const std::string& pos) = 0;
	virtual bool IsMusicStreaming(void) = 0;
	virtual int  GetCurrentPlayingProgress(void) = 0;

	//file share/streaming
	virtual void AcceptFileShare(const std::string& share_id,const std::string& target_folder, const std::string& target_file_name) = 0;
	virtual void DeclineFileShare(const std::string& share_id) = 0;
	virtual void CancelFileShare(const std::string& share_id) = 0;
	virtual void TestSendFile() = 0;

	//network player
	virtual int Play(const std::string &jid, const std::string &song_uuid) = 0;
	virtual int Pause() = 0;
	virtual int Resume() = 0;
	virtual int Seek(const int position) = 0;

	//share events
	virtual void OnFileShareRequestReceived(const std::string& share_id, const buzz::Jid& target, int type,  const std::string& fileName, int size, bool needPreview) = 0;
	virtual void OnFileShareProgressUpdated(const std::string& share_id, int percent) = 0;
	virtual void OnMusicStreamingProgressUpdated(const std::string& share_id, int progress) = 0;
	virtual int  GetCurrentMusicStreamingProgress(const std::string& share_id) = 0;

	virtual void OnFileSharePreviewReceived(const std::string& share_id, const std::string& path, int w, int h) = 0;
	virtual void OnFileShareStateChanged(const std::string& remote_jid,  const std::string& share_id, int state) = 0;

	//call
	virtual cricket::CallOptions* GetCurrentCallOption(void) = 0;
	virtual buzz::Jid GetCurrentJidInCall(void) = 0;

	//file system
	virtual void OnFileSystemChanged(void) = 0;

	//active streaming devices
	virtual void OnRemoteDeviceActivity(VueceStreamingDevice* d) = 0;

	static VueceNativeInterface* CreateInstance(InitData* init_data);
	static void LogClientState(int state);
	static void LogClientEvent(int event);
public:
	sigslot::signal1<VueceEvent> SigUserEvent;
	sigslot::signal1<VueceStreamingDevice*> SigRemoteDeviceActivity;
	sigslot::signal3<const std::string&, const std::string&, const std::string&> SigRosterVCardReceived;
	sigslot::signal1<VueceRosterSubscriptionMsg*> SigRosterSubscriptionMsgReceived;
};

}

#endif /* VUECENATIVEINTERFACE_H_ */
