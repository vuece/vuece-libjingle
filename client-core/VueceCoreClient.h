#ifndef VUECE_CORE_CLIENT_H_
#define VUECE_CORE_CLIENT_H_

#include <map>
#include <stack>

#include "talk/p2p/base/session.h"
#include "talk/session/fileshare/VueceMediaStreamSessionClient.h"
#include "talk/session/fileshare/VueceFileShareSessionClient.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/xmpp/xmppclient.h"
#include "status.h"
#include "talk/base/thread.h"
#ifdef USE_TALK_SOUND
#include "talk/sound/soundsystemfactory.h"
#endif

#ifdef VUECE_APP_ROLE_HUB
#include "vuecemediaitem.h"
#include "VueceKeyValuePair.h"
#endif

#include "VueceNativeInterface.h"


typedef enum _CoreClientFsmEvent{
	CoreClientFsmEvent_Start = 0,
	CoreClientFsmEvent_LoggedIn,
	CoreClientFsmEvent_LogOut,
	CoreClientFsmEvent_LoggedOut,
	CoreClientFsmEvent_AuthFailed,
	CoreClientFsmEvent_ConnectionFailed,
	CoreClientFsmEvent_SysErr,
	CoreClientFsmEvent_None
}CoreClientFsmEvent;

namespace buzz
{
class PresencePushTask;
class PresenceOutTask;

#ifdef MUC_ENABLED
class MucInviteRecvTask;
class MucInviteSendTask;
#endif

class FriendInviteSendTask;
class RosterQuerySendTask;
class RosterQueryResultRecvTask;
class RosterSubResponseRecvTask;

#ifdef CHAT_ENABLED
class ChatPushTask;
#endif

class VCardPushTask;
class VHubGetTask;
class VHubResultTask;
class VoicemailJidRequester;
class DiscoInfoQueryTask;
class Muc;
class Status;
class MucStatus;
class JingleInfoTask;
struct AvailableMediaEntry;
}

namespace talk_base
{
class Thread;
class NetworkManager;
}

namespace cricket
{
class PortAllocator;
class MediaEngine;
class MediaSessionClient;
class VueceFileShareSessionClient;
struct CallOptions;
class SessionManagerTask;
}

//A map used to remember which remote node(jid) and which content(uri)
//is being served, this is used to avoid duplicated request on the
//same content from the same remote client
typedef std::map<std::string, std::string> RemoteNodeServingMap;

typedef std::map<std::string, VueceStreamingDevice*> RemoteActiveDeviceMap;


#ifdef VUECE_APP_ROLE_HUB
class VueceMediaDBManager;
#endif

class VueceConnectionKeeper;
class VueceKernelShell;

class VueceCoreClient: public sigslot::has_slots<>
{
public:
	explicit VueceCoreClient(
			vuece::VueceNativeInterface* vNativeInstance,
			const char* userName);
	~VueceCoreClient();

	void SetAutoAccept(bool auto_accept)
	{
		auto_accept_ = auto_accept;
	}

	static std::string utf8_encode(const std::wstring &wstr);
	static std::wstring s2ws(const std::string& s);

	//MUC related public methods
#ifdef MUC_ENABLED
	void SetPmucDomain(const std::string &pmuc_domain)
	{
		pmuc_domain_ = pmuc_domain;
	}

	void JoinMuc(const std::string& room);
	void InviteToMuc(const std::string& user, const std::string& room);
	void LeaveMuc(const std::string& room);
#endif

	buzz::Jid GetCurrentTargetJid();

#ifdef VCARD_ENABLED
	void SendVCardRequest(const std::string& to);
#endif

	void FireNetworkPlayerNotification(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s);

	void OnXmmpSocketClosedEvent(int err);

	bool SendVHubMessage(const std::string& to, const std::string& type,
			const std::string& message);
	void SendVHubPlayRequest(const std::string& to, const std::string& type,
			const std::string& message, const std::string& uri);

	void RequestNextBufferWindow(int start_pos);

	void SendSignature(const std::string& signature);
	void SendPresence(const std::string& status, const std::string& signature);
	void SendPresence(const std::string& status);
	void InviteFriend(const std::string& user);
	void SendSubscriptionMessage(const std::string& user, int type);
	void QueryRoster();

	cricket::CallOptions* GetCurrentCallOption(void);
	void CancelFileShare(const std::string &share_id);

	void AcceptFileShare(const std::string &share_id,
			const std::string & target_download_folder,
			const std::string &target_file_name);

	void DeclineFileShare(const std::string &share_id);

	void ResumeStreamPlayer(int resume_pos);
	void MediaStreamSeek(const std::string &pos_sec);

	int PlayInternal(const std::string &jid, const std::string &song_uuid);
	int PauseInternal();
	int ResumeInternal();
	int SeekInternal(int new_resume_pos);

#ifdef VUECE_APP_ROLE_HUB
	vuece::VueceMediaItem* LocateMediaItemInDB(const std::string& user, const std::string& targetUri);
	bool BrowseMediaItem(const std::string &id, std::ostringstream& os);
	int InitiateMusicStreamDB(
			const std::string& share_id,
			const std::string& user,
			const std::string& targetUri,
			const std::string& width,
			const std::string& height,
			const std::string& preview_file_path,
			const std::string& start_pos,
			const std::string& need_preview,
			vuece::VueceMediaItem* mediaItem
	);

	bool IsRemoteClientAlreadyInServe(const std::string& jid);

	bool AddActiveStreamingDevice(
			const std::string& jid,
			const std::string& content_id);

	bool ValidateStreamingDevice(
			VueceStreamingDevice* dev);

	bool ApplyLimitationOnRequestingClient(
			 const buzz::Jid& targetJid,
			const std::string& content_id);

	bool RemoveStreamingNode(
			const std::string& remote_jid);

#endif

	int SendFile(const std::string& share_id, const std::string& user,
			const std::string& pathname, const std::string& width,
			const std::string& height, const std::string& preview_file_path,
			const std::string& start_pos, const std::string& need_preview);

	void SetPortAllocatorFlags(uint32 flags)
	{
		portallocator_flags_ = flags;
	}
	void SetAllowLocalIps(bool allow_local_ips)
	{
		allow_local_ips_ = allow_local_ips;
	}

	void SetInitialProtocol(cricket::SignalingProtocol initial_protocol)
	{
		initial_protocol_ = initial_protocol;
	}

	void SetSecurePolicy(cricket::SecureMediaPolicy secure_policy)
	{
		secure_policy_ = secure_policy;
	}

	typedef std::map<buzz::Jid, buzz::Muc*> MucMap;

	const MucMap& mucs() const
	{
		return mucs_;
	}


	int Start(const char* name, const char* pwd,  const int auth_type);

	vuece::ClientState GetClientState(void);
	int LogOut();

	bool IsLoginFailed();
	void Quit();

	vuece::RosterMap* GetRosterMap();

	/**
	 * This is function is called when there is a streaming progress update
	 *
	 * Note when we are streaming music the progress value is in percent, if we are
	 * stream an actual file, the progress value is in byte
	 */
	void OnFileShareProgressUpdated(const std::string& share_id, int progress);
	void OnMusicStreamingProgressUpdated(const std::string& share_id, int progress);

	void OnFileSharePreviewReceived(const std::string& share_id,
			const std::string& file_path, int w, int h);
	void OnFileShareStateChanged(const std::string& remote_jid,
			const std::string& sid, int state);
	void OnStreamPlayerStateChanged(const std::string& share_id, int state);

	void OnFileShareRequestReceived(const std::string& share_id,
			const buzz::Jid& target, int type, const std::string& fileName,
			int size, bool need_preview);
	void OnVueceCommandReceived(int, const char*);

	void OnStreamPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg);

	bool IsMusicStreaming(void);

	int  GetCurrentMusicStreamingProgress(const std::string& share_id);

	int  GetCurrentPlayingProgress(void);

	void OnCurrentRemoteHubUnAvailable(void);

#ifdef CHAT_ENABLED
	void VueceCmdSendChat();
	void SendChat(const std::string& to, const std::string msg);
#endif

	//we have two char* arguments in this signal for necessary use
	sigslot::signal3<int, const char*, const char*> SignalVueceEvent;
	sigslot::signal1<VueceRosterSubscriptionMsg*> SigRosterSubscriptionMsgReceived;

	VueceKernelShell* GetShell(void);
	cricket::VueceMediaStreamSessionClient* GetStreamSessionClien(void);
private:
	vuece::ClientState client_state;
	vuece::NetworkPlayerState network_player_state;
	buzz::XmppClient* xmpp_client_;

	//private functions
private:

	void StartConnectionKeeper();
	bool SendPlayRequest(const char* media_uri, int start_pos, bool need_preview);

	void StopStreamPlayer(const std::string &share_id);

//MUC related private methods and variables
#ifdef MUC_ENABLED
	void OnMucInviteReceived(const buzz::Jid& inviter, const buzz::Jid& room,
			const std::vector<buzz::AvailableMediaEntry>& avail);
	void OnMucJoined(const buzz::Jid& endpoint);
	void OnMucStatusUpdate(const buzz::Jid& jid, const buzz::MucStatus& status);
	void OnMucLeft(const buzz::Jid& endpoint, int error);

	buzz::MucInviteRecvTask* muc_invite_recv_;
	buzz::MucInviteSendTask* muc_invite_send_;

#endif

	bool StateTransition(CoreClientFsmEvent e);

	int Start_Test(const char* name, const char* pwd,  const int auth_type);

	void OnXmppClientStateChange(buzz::XmppEngine::State state);

	void OnVCardIQResultReceived(const buzz::Jid& jid, const std::string& fn, const std::string& imgData) ;
	void OnRosterSubRespReceived(const buzz::XmlElement* stanza);

	void InitPhone();
	void InitPresence();
	void StartJingleInfoTask();
	void RefreshStatus();
	void OnJingleInfo(const std::string & relay_token,
			const std::vector<std::string> &relay_addresses,
			const std::vector<talk_base::SocketAddress> &stun_addresses);

	void OnRequestSignaling();
	void OnSessionCreate(cricket::Session* session, bool initiate);
	void OnRosterStatusUpdate(const buzz::Status& status);

	void OnDevicesChange();
	void OnFoundVoicemailJid(const buzz::Jid& to, const buzz::Jid& voicemail);
	void OnVoicemailJidError(const buzz::Jid& to);

	static const std::string strerror(buzz::XmppEngine::Error err);

	void PrintRoster();

	talk_base::Thread* worker_thread_;
	talk_base::NetworkManager* network_manager_;
	cricket::PortAllocator* port_allocator_;
	cricket::SessionManager* session_manager_;
	cricket::SessionManagerTask* session_manager_task_;
	cricket::VueceMediaStreamSessionClient* media_stream_session_client_;

	VueceConnectionKeeper* connection_keeper;

	MucMap mucs_;

	cricket::BaseSession *session_;
	bool incoming_call_;
	bool auto_accept_;

	std::string pmuc_domain_;

	buzz::Status my_status_;
	buzz::PresencePushTask* presence_push_;
	buzz::PresenceOutTask* presence_out_;
	buzz::FriendInviteSendTask* friend_invite_send_;
	buzz::RosterQuerySendTask* roster_query_send_;
	buzz::RosterQueryResultRecvTask* roster_query_result_recv_;
	buzz::RosterSubResponseRecvTask* roster_sub_resp_recev_;

#ifdef CHAT_ENABLED
	buzz::ChatPushTask* chat_msg_push;
#endif

#ifdef VCARD_ENABLED
	buzz::VCardPushTask* vcard_iq_push;
#endif

	buzz::JingleInfoTask* jingle_info_push;

	buzz::VHubGetTask* vhub_get;
	buzz::VHubResultTask* vhub_result;

	vuece::RosterMap* roster_;

	//map[jid-->content_url]
	RemoteNodeServingMap* remoteNodeServingMap;
	RemoteActiveDeviceMap* remoteActiveDeviceMap;

	uint32 portallocator_flags_;

	bool allow_local_ips_;
	cricket::SignalingProtocol initial_protocol_;
	cricket::SecureMediaPolicy secure_policy_;
	bool bIsLoginFailed;
	bool bConnectionFailed;
	bool bAllowConcurrentStreaming;

	vuece::VueceNativeInterface* vNativeInstance;

	cricket::CallOptions* currentCallOption;
	buzz::Jid currentTargetJid;

	VueceKernelShell* shell;

	int current_file_download_progress;
	int current_music_streaming_progress;
	int current_music_streaming_state;

	std::string current_song_uuid;
	std::string audio_cache_location;
	std::string preview_file_name;
	std::string audio_file_name;
	std::string current_audio_share_id;

#ifdef VUECE_APP_ROLE_HUB
	VueceMediaDBManager* dbMgr;
#endif
};


#endif
