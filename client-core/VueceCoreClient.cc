
#include "VueceCoreClient.h"

#include <cstdio>
#include <iostream>
#include <limits>

#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppthread.h"
#include "talk/xmpp/xmppauth.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmppclientsettings.h"

#include "talk/p2p/base/constants.h"
#include "talk/base/base64.h"
#include "talk/base/logging.h"
#include "talk/base/helpers.h"
#include "talk/base/thread.h"
#include "talk/base/network.h"
#include "talk/base/socketaddress.h"
#include "talk/base/stringutils.h"
#include "talk/base/pathutils.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringdigest.h"
#include "talk/base/fileutils.h"

#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/p2p/client/httpportallocator.h"
#include "talk/p2p/client/sessionmanagertask.h"

#include "talk/session/fileshare/VueceShareCommon.h"

#ifdef ANDROID
#include "talk/session/fileshare/VueceStreamPlayer.h"
#include "talk/session/fileshare/VueceNetworkPlayerFsm.h"
#endif

#include "VueceLogger.h"

#include "talk/base/physicalsocketserver.h"
#include "talk/base/win32socketserver.h"
#include "talk/base/ssladapter.h"

#include "VueceConstants.h"
#include "VueceKernelShell.h"
#include "DebugLog.h"

#ifdef CHAT_ENABLED
#include "chatpushtask.h"
#endif

#ifdef VCARD_ENABLED
#include "vcardpushtask.h"
#endif

#include "presencepushtask.h"
#include "presenceouttask.h"

#ifdef MUC_ENABLED
#include "mucinviterecvtask.h"
#include "mucinvitesendtask.h"
#endif

#include "friendinvitesendtask.h"
#include "rosterquerysendtask.h"
#include "vhubgettask.h"
#include "vhubresulttask.h"
#include "rosterqueryresultrecvtask.h"
#include "RosterSubResponseRecvTask.h"
#include "muc.h"
#include "voicemailjidrequester.h"
#include "jingleinfotask.h"
#include "VueceGlobalSetting.h"
#include "VueceConnectionKeeper.h"
#include <sstream>

#ifdef VUECE_APP_ROLE_HUB
//headers from taglib
#include <fileref.h>
#include <tag.h>
#include <id3v2/id3v2tag.h>
#include <mpegfile.h>
#include <id3v2/id3v2frame.h>
#include <id3v2/id3v2header.h>
#include <id3v2/frames/attachedpictureframe.h>
#include "sqlite3.h"
#include "VueceWinUtilities.h"
#include "VueceMediaDBManager.h"
#endif

//#define MAX_CONCURRENT_SESSION_NR 3

static DebugLog debug_log_;

using namespace vuece;

static char iMyBareJid[64+1];

/*
 * use this flag to remember the last player progress, when playing is resumed, this flag will
 * be used as the start position
 */

static int current_play_progress;

namespace {

const char* DescribeStatus(buzz::Status::Show show, const std::string& desc) {
	switch (show) {
	case buzz::Status::SHOW_XA:
		return desc.c_str();
	case buzz::Status::SHOW_ONLINE:
		return "online";
	case buzz::Status::SHOW_AWAY:
		return "away";
	case buzz::Status::SHOW_DND:
		return "do not disturb";
	case buzz::Status::SHOW_CHAT:
		return "ready to chat";
	default:
		return "offline";
	}
}

} // namespace

VueceCoreClient::VueceCoreClient(vuece::VueceNativeInterface* _vNativeInstance,
		const char* userName

) :
			vNativeInstance(_vNativeInstance),
			currentCallOption(new cricket::CallOptions),
			pmuc_domain_("groupchat.google.com"), roster_(new RosterMap), portallocator_flags_(0), allow_local_ips_(true),
			initial_protocol_(cricket::PROTOCOL_HYBRID),
            secure_policy_(cricket::SEC_DISABLED)
{

	char buf[512];
	char buf2[512];

	struct tm  tstruct;

	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));

	xmpp_client_ = NULL;
	bConnectionFailed = false;

	client_state = CLIENT_STATE_OFFLINE;
	network_player_state = NetworkPlayerState_Idle;
	current_file_download_progress = 0;
	current_music_streaming_progress = 0;
	current_play_progress = 0;
	current_music_streaming_state = 0;
	audio_cache_location = "/sdcard/vuece/tmp/audio";
	preview_file_name = "preview.jpg";
	audio_file_name = "audio_tmp";

	//must be initialized to NULL
	connection_keeper = NULL;
	worker_thread_  = NULL;
	network_manager_  = NULL;
	port_allocator_  = NULL;
	session_manager_  = NULL;
	session_manager_task_  = NULL;
	media_stream_session_client_  = NULL;

	session_  = NULL;
	presence_push_  = NULL;
	presence_out_  = NULL;
	friend_invite_send_  = NULL;
	roster_query_send_  = NULL;
	roster_query_result_recv_  = NULL;
	roster_sub_resp_recev_  = NULL;

	jingle_info_push  = NULL;
	vhub_get  = NULL;
	vhub_result  = NULL;
	remoteNodeServingMap  = NULL;
	remoteActiveDeviceMap  = NULL;

	shell  = NULL;

	bAllowConcurrentStreaming = true;

	time_t now = time(0);   // get time now
	tstruct = *localtime(&now);

	//log file format: YEAR-MONTH-DAY_HOURMINSEC.log
	strftime(buf2, sizeof(buf2), "%Y-%m-%d_%H%M%S_MS2.log", &tstruct);

	strcpy(buf, "C:\\vuece-pc-logs\\");
	strcat(buf, buf2);

	LOG(LS_VERBOSE) << "VueceCoreClient - constructor, initializing ...";
//	LOG(INFO) << "VueceCoreClient - constructor, initializing ...";
//	LOG(WARNING) << "VueceCoreClient - constructor, initializing ...";
//	LOG(LERROR) << "VueceCoreClient - constructor, initializing ...";


#ifdef VUECE_APP_ROLE_HUB
	dbMgr = new VueceMediaDBManager();
	if( !dbMgr->Open() )
	{
		LOG(LS_ERROR) << "VueceCoreClient constructor, db cannot be opened.";
	}

	LOG(LS_VERBOSE) << "VueceCoreClient - constructor, hub mode, start DB self check ...";

	dbMgr->SelfCheck();

	LOG(LS_VERBOSE) << "VueceCoreClient - constructor, hub mode, DB self check done, force checksum calc only once";

	VueceMediaDBManager::RetrieveDBFileChecksum(true);

	remoteNodeServingMap = new RemoteNodeServingMap();
	remoteActiveDeviceMap = new RemoteActiveDeviceMap();

#endif

	bIsLoginFailed = true;

	memset(iMyBareJid, 0, sizeof(iMyBareJid));

	strcpy(iMyBareJid, userName);
	
	LOG(INFO) << "VueceCoreClient::Constructor done, user name: " << iMyBareJid;
}


VueceKernelShell* VueceCoreClient::GetShell(void)
{
	return shell;
}

cricket::VueceMediaStreamSessionClient* VueceCoreClient::GetStreamSessionClien(void)
{
	return media_stream_session_client_;
}


VueceCoreClient::~VueceCoreClient()
{
	LOG(INFO) << ("VueceCoreClient:De-constructor called.");

//	if(shell != NULL)
//	{
//		shell->Stop();
//
//		LOG(INFO)
//			<< "VueceNativeInterface::SignIn:Deleting shell...";
//		delete shell;
//		shell = NULL;
//
//		LOG(INFO)
//			<< "VueceNativeInterface::SignIn:VueceKernelShell deleted.";
//	}


#ifdef VUECE_APP_ROLE_HUB
	dbMgr->Close();
	delete dbMgr;

	if(remoteNodeServingMap != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting remoteNodeServingMap...";
		delete remoteNodeServingMap;
		remoteNodeServingMap = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - remoteNodeServingMap deleted";
	}

	if(remoteActiveDeviceMap != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting remoteActiveDeviceMap...";
		delete remoteActiveDeviceMap;
		remoteActiveDeviceMap = NULL;
		LOG(INFO) << "VueceCoreClient:De-constructor - remoteActiveDeviceMap deleted";
	}


#endif

	if(roster_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting roster_...";
		delete roster_;
		roster_ = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - roster_ deleted";
	}

	if(connection_keeper != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting connection_keeper...";
		delete connection_keeper;
		connection_keeper = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - connection_keeper deleted";
	}


	if(media_stream_session_client_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting media_stream_session_client_...";
		delete media_stream_session_client_;
		media_stream_session_client_ = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - media_stream_session_client_ deleted";
	}

	if(session_manager_task_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - aborting session_manager_task_...";

		session_manager_task_->Abort(true);
//		delete session_manager_task_;
//		session_manager_task_ = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - session_manager_task_ deleted";
	}

	if(session_manager_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting session_manager_...";
		delete session_manager_;
		session_manager_ = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - session_manager_ deleted";
	}

//	if(port_allocator_ != NULL)
//	{
//		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting port_allocator_...";
//		delete port_allocator_;
//		port_allocator_ = NULL;
//
//		LOG(INFO) << "VueceCoreClient:De-constructor - port_allocator_ deleted";
//	}


	if(worker_thread_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting worker_thread_ ...";
		delete worker_thread_;
		worker_thread_ = NULL;

		LOG(INFO) << "VueceCoreClient:De-constructor - worker_thread_ deleted ";
	}

	if(presence_push_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting presence_push_...";

		presence_push_->Abort(true);

		//delete presence_push_;
		//presence_push_ = NULL;
		//LOG(INFO) << "VueceCoreClient:De-constructor - presence_push_ deleted";
	}

	LOG(INFO) << "VueceCoreClient:De-constructor - D1";

	if(presence_out_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting presence_out_ skipped";
//		presence_out_->Abort(true);

//		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting presence_out_...";
//		delete presence_out_;
//		presence_out_ = NULL;
//		LOG(INFO) << "VueceCoreClient:De-constructor - presence_out_ deleted";
	}

	if(friend_invite_send_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting friend_invite_send_...";
		friend_invite_send_->Abort(true);

//		LOG(INFO) << "VueceCoreClient:De-constructor - Deleting friend_invite_send_...";
//		delete friend_invite_send_;
//		friend_invite_send_ = NULL;
//		LOG(INFO) << "fVueceCoreClient:De-constructor - riend_invite_send_ deleted";
	}

	LOG(INFO) << "VueceCoreClient:De-constructor - D2";

	if(roster_query_send_ != NULL)
	{
		LOG(INFO) << "Aborting roster_query_send_...";
		roster_query_send_->Abort(true);

//		LOG(INFO) << "Deleting roster_query_send_...";
//		delete roster_query_send_;
//		roster_query_send_ = NULL;
//		LOG(INFO) << "roster_query_send_ deleted";
	}

	if(roster_query_result_recv_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting roster_query_result_recv_ 1";
		roster_query_result_recv_->Abort(true);
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting roster_query_result_recv_ Done";

//		LOG(INFO) << "Deleting roster_query_result_recv_...";
//		delete roster_query_result_recv_;
//		roster_query_result_recv_ = NULL;
//		LOG(INFO) << "roster_query_result_recv_ deleted";
	}

	if(roster_sub_resp_recev_ != NULL)
	{
		LOG(INFO) << "VueceCoreClient:De-constructor - Aborting roster_sub_resp_recev_...";
		roster_sub_resp_recev_->Abort(true);

//		LOG(INFO) << "Deleting roster_sub_resp_recev_...";
//		delete roster_sub_resp_recev_;
//		roster_sub_resp_recev_ = NULL;
//		LOG(INFO) << "roster_sub_resp_recev_ deleted";
	}

	LOG(INFO) << ("VueceCoreClient:De-constructor - VueceCoreClient:Destructor DONE.");
}

const std::string VueceCoreClient::strerror(buzz::XmppEngine::Error err) {
	switch (err) {
	case buzz::XmppEngine::ERROR_NONE:
		return "";
	case buzz::XmppEngine::ERROR_XML:
		return "Malformed XML or encoding error";
	case buzz::XmppEngine::ERROR_STREAM:
		return "XMPP stream error";
	case buzz::XmppEngine::ERROR_VERSION:
		return "XMPP version error";
	case buzz::XmppEngine::ERROR_UNAUTHORIZED:
		return "User is not authorized (Check your username and password)";
	case buzz::XmppEngine::ERROR_TLS:
		return "TLS could not be negotiated";
	case buzz::XmppEngine::ERROR_AUTH:
		return "Authentication could not be negotiated";
	case buzz::XmppEngine::ERROR_BIND:
		return "Resource or session binding could not be negotiated";
	case buzz::XmppEngine::ERROR_CONNECTION_CLOSED:
		return "Connection closed by output handler.";
	case buzz::XmppEngine::ERROR_DOCUMENT_CLOSED:
		return "Closed by </stream:stream>";
	case buzz::XmppEngine::ERROR_SOCKET:
		return "Socket error";
	default:
		return "Unknown error";
	}
}

bool VueceCoreClient::IsLoginFailed() {
	return bIsLoginFailed;
}

vuece::ClientState VueceCoreClient::GetClientState(void)
{
	return client_state;
}

int VueceCoreClient::LogOut()
{

	VueceLogger::Info("VueceCoreClient::LogOut");

	if ( !StateTransition(CoreClientFsmEvent_LogOut) )
	{
		return RESULT_FUNC_NOT_ALLOWED;
	}

	OnVueceCommandReceived(VUECE_CMD_SIGN_OUT, NULL);

	return RESULT_OK;

}

static void LogFsmEvent(CoreClientFsmEvent e)
{
	switch (e)
	{
	case CoreClientFsmEvent_Start:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_Start");
		break;
	}
	case CoreClientFsmEvent_LoggedIn:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_LoggedIn");
		break;
	}
	case CoreClientFsmEvent_LogOut:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_LogOut");
		break;
	}
	case CoreClientFsmEvent_LoggedOut:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_LoggedOut");
		break;
	}
	case CoreClientFsmEvent_AuthFailed:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_AuthFailed");
		break;
	}
	case CoreClientFsmEvent_ConnectionFailed:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_ConnectionFailed");
		break;
	}
	case CoreClientFsmEvent_SysErr:
	{
		VueceLogger::Debug("VueceCoreClient::LogFsmEvent - FsmEvent_SysErr");
		break;
	}
	default:
	{
		VueceLogger::Fatal("VueceCoreClient::LogFsmEvent - Unexpected event: %d, abort now!",
				(int) e);
	}
	}
}

/**
 * The ONLY allowed check point of events and state transition
 */
bool VueceCoreClient::StateTransition(CoreClientFsmEvent e)
{
	bool allowed = false;

	VueceLogger::Info("VueceCoreClient::StateTransition - Event code - %d", (int)e);

	LogFsmEvent(e);

	VueceNativeInterface::LogClientState(client_state);

	switch(e)
	{
		case CoreClientFsmEvent_Start:
		{
			LOG(INFO) << "VueceCoreClient::StateTransition - Event is FsmEvent_Start";

			//Login(i.e. Start) is only allowed when client is in OFFLINE state
			if(client_state == CLIENT_STATE_OFFLINE)
			{
				allowed = true;
				client_state = CLIENT_STATE_CONNECTING;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently offline, state is switched to -----> CLIENT_STATE_CONNECTING";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_LOGGING_IN, CLIENT_STATE_CONNECTING);
			}
			else
			{
				VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			}
			break;
		}
		case CoreClientFsmEvent_LoggedIn:
		{
			//such event is accepted only when current state is CONNECTING
			if(client_state == CLIENT_STATE_CONNECTING)
			{
				allowed = true;
				client_state = CLIENT_STATE_ONLINE;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently CONNECTING, state is switched to -----> CLIENT_STATE_CONNECTING";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_LOGIN_OK, CLIENT_STATE_ONLINE);
			}
			else
			{
				VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			}

			break;
		}
		case CoreClientFsmEvent_ConnectionFailed:
		{


			if(client_state == CLIENT_STATE_ONLINE || client_state == CLIENT_STATE_CONNECTING)
			{
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently online, state is switched to -----> CLIENT_STATE_DISCONNECTING";
				client_state = CLIENT_STATE_DISCONNECTING;

				//NOTE - We fire an event notification here, then start log off process immediately, the event notification will be
				//received by UI and UI will also try to log off, so when UI triggers log off, we need to change if we are already
				//logging off, if yes then we just ignore log off triggered by UI
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_NETWORK_ERR, CLIENT_STATE_DISCONNECTING);

				allowed = true;
			}
			else
			{
				VueceLogger::Info("VueceCoreClient::StateTransition - No state transition because client is not online, event will be ignored.");
			}

			break;
		}
		case CoreClientFsmEvent_LoggedOut:
		{
			//before current state MUST be DISCONNECTIONG
			if(client_state == CLIENT_STATE_DISCONNECTING)
			{
				allowed = true;
				client_state = CLIENT_STATE_OFFLINE;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently disconnecting, state is switched to -----> CLIENT_STATE_OFFLINE";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_LOGOUT_OK, CLIENT_STATE_OFFLINE);
			}
			else
			{
				VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			}
			break;
		}

		case CoreClientFsmEvent_AuthFailed:
		{
			if(client_state == CLIENT_STATE_CONNECTING)
			{
				allowed = true;
				client_state = CLIENT_STATE_OFFLINE;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently connecting, state is switched to -----> CLIENT_STATE_OFFLINE";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_AUTH_ERR, CLIENT_STATE_OFFLINE);
			}
			else
			{
				VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			}


			break;
		}


		case CoreClientFsmEvent_LogOut:
		{
			if(client_state == CLIENT_STATE_ONLINE)
			{
				allowed = true;
				client_state = CLIENT_STATE_DISCONNECTING;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently ONLINE, state is switched to -----> CLIENT_STATE_DISCONNECTING";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_LOGGING_OUT, CLIENT_STATE_DISCONNECTING);
			}
			//this is possbile when network connection failed, caused ConnectionFailed event
			else if(client_state == CLIENT_STATE_DISCONNECTING)
			{
				allowed = true;
				client_state = CLIENT_STATE_DISCONNECTING;
				LOG(INFO) << "VueceCoreClient::StateTransition - Transition is allowed because client is currently disconnecting, state is switched to -----> CLIENT_STATE_DISCONNECTING";
				vNativeInstance->OnClientStateChanged(CLIENT_EVENT_LOGGING_OUT, CLIENT_STATE_DISCONNECTING);
			}
			else
			{
				VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			}

			break;
		}

		case CoreClientFsmEvent_SysErr:
		{
			vNativeInstance->OnClientStateChanged(CLIENT_EVENT_SYSTEM_ERR, CLIENT_STATE_OFFLINE);
			VueceLogger::Fatal("VueceCoreClient::StateTransition - Unexpected state - need further investigation.");
			break;
		}

	}

	return allowed;
}


int VueceCoreClient::Start(const char* name, const char* pwd,  const int auth_type)
{
	bool debug = true;
	bool bIsLoginFailed = false;
	std::string protocol = "hybrid";
	std::string secure = "disable";

	talk_base::PhysicalSocketServer ss;

	VueceLogger::Info("VueceCoreClient::Start - START");

	VueceNativeInterface::LogClientState(client_state);

	VueceLogger::Info("VueceCoreClient::Start - Performing some pre-validation work");

	if(name == 0 || strlen(name) == 0)
	{
		LOG(LS_ERROR) << "Name field is invalid, log in cannot proceed";
		return RESULT_INVALID_PARAM;
	}

	if(pwd == 0 || strlen(pwd) == 0)
	{
		LOG(LS_ERROR) << "Pwd field is invalid, log in cannot proceed";
		return RESULT_INVALID_PARAM;
	}

	//TODO Check network connectivity here

	if ( !StateTransition(CoreClientFsmEvent_Start) )
	{
		VueceLogger::Error("VueceCoreClient::Start - Not allowed because StateTransition() returned false.");
		return RESULT_FUNC_NOT_ALLOWED;
	}

	//////////////////////////////////////////////////////////////
	// Environment Initialization
	cricket::SignalingProtocol initial_protocol = cricket::PROTOCOL_HYBRID;

	if (protocol == "jingle") {
		initial_protocol = cricket::PROTOCOL_JINGLE;
	} else if (protocol == "gingle") {
		initial_protocol = cricket::PROTOCOL_GINGLE;
	} else if (protocol == "hybrid") {
		initial_protocol = cricket::PROTOCOL_HYBRID;
	} else {
		LOG(WARNING)
			<< "Invalid protocol.  Must be jingle, gingle, or hybrid.";
		return RESULT_INVALID_PARAM;
	}

	cricket::SecureMediaPolicy secure_policy = cricket::SEC_DISABLED;
	if (secure == "disable") {
		secure_policy = cricket::SEC_DISABLED;
	} else if (secure == "enable") {
		secure_policy = cricket::SEC_ENABLED;
	} else if (secure == "require") {
		secure_policy = cricket::SEC_REQUIRED;
	} else {
		LOG(WARNING)
			<< "Invalid encryption.  Must be enable, disable, or require.";
		return RESULT_INVALID_PARAM;
	}

	buzz::Jid jid;

	talk_base::InsecureCryptStringImpl pass;

	LOG(INFO) << "VueceCoreClient::Start:2";

	std::string username(name);
	std::string passwd(pwd);

	LOG(INFO) << "VueceCoreClient::Start:username: " << username << ", pwd: ********";

	pass.password() = passwd;

	std::string host = "gmail.com";

	jid = buzz::Jid(username);

	buzz::XmppClientSettings xcs;

	xcs.set_user(jid.node());
	xcs.set_host(jid.domain());

	//This will appear in full jid, e.g, alice@gmail.com/vuece.pc8D221E06, alice@gmail.com/vuece.contFBB6EB83

#if defined(WIN32)
	xcs.set_resource("vuece.pc");
#elif defined(ANDROID)
	xcs.set_resource("vuece.controller");
#else
	VueceLogger::Fatal("VueceCoreClient::Start - Unsupported OS platform, abort!");
#endif

	// for gtalk
	xcs.set_use_tls(buzz::TLS_ENABLED);
	xcs.set_allow_plain(false);

	xcs.set_server(talk_base::SocketAddress("talk.google.com", 5222));
	xcs.set_pass(talk_base::CryptString(pass));

	//Note once this is set, oath2 will be activated
	if(auth_type == VUECE_AUTH_TYPE_OAUTH)
	{
		LOG(INFO) << "VueceCoreClient::Start - auth type is OATH2.";
		xcs.set_auth_token(buzz::AUTH_MECHANISM_OAUTH2, passwd);
	}
	else
	{
		VueceLogger::Debug( "VueceCoreClient::Start - auth type is other, code value: %d", auth_type);
	}

	LOG(INFO) << "VueceCoreClient::Start:Initializing SSL 1";

	talk_base::InitializeSSL();

	LOG(INFO) << "VueceCoreClient::Start:Initializing SSL 2";

	// create a new thread and add it to the pool

#if defined(WIN32)
	//PC code
	talk_base::Win32Thread w32_thread;
	talk_base::ThreadManager::SetCurrent(&w32_thread);
	talk_base::Thread* main_thread = talk_base::Thread::Current();
#elif defined(ANDROID)
	talk_base::Thread main_thread(&ss);
	talk_base::ThreadManager::SetCurrent(&main_thread);
#else
	VueceLogger::Fatal("VueceCoreClient::Start - Unsupported OS platform, abort!");
#endif


	LOG(INFO) << "VueceCoreClient::Start:Creating VueceCoreClient A ...";

	XmppPump pump;

	this->xmpp_client_ = pump.client();

	xmpp_client_->SignalStateChange.connect(this, &VueceCoreClient::OnXmppClientStateChange);

	LOG(INFO) << "VueceCoreClient::Start:Creating VueceCoreClient B ...";

	if (debug) {
		pump.client()->SignalLogInput.connect(&debug_log_, &DebugLog::Input);
		pump.client()->SignalLogOutput.connect(&debug_log_, &DebugLog::Output);
	}

	this->SetAllowLocalIps(true);
	this->SetInitialProtocol(initial_protocol);
	this->SetSecurePolicy(secure_policy);

	// gtalk
	LOG(INFO) << "VueceCoreClient::Start:Calling pump.DoLogin ...";

	XmppSocket* xmppSocket = new XmppSocket(true);

	LOG(INFO) << "VueceCoreClient::Start:Register VueceCoreClient::OnXmmpSocketClosedEvent";

	xmppSocket->SignalCloseEvent.connect(this, &VueceCoreClient::OnXmmpSocketClosedEvent);

	//------------------------------------------------
	talk_base::Thread* cur_thread = talk_base::Thread::Current();
	shell = new VueceKernelShell(cur_thread, this);
	shell->Start();
	//------------------------------------------------

	LOG(INFO) << "calling pump.DoLogin ...";
	pump.DoLogin(xcs, xmppSocket, new XmppAuth());

	// NOTE: this blocks current thread util VueceCoreClient::Quit() is called.
	LOG(INFO) << "VueceCoreClient::Start:call run() on main thread ...";

#if defined(WIN32)
	//PC code
	main_thread->Run();

#elif defined(ANDROID)
	main_thread.Run();

#else
	VueceLogger::Fatal("VueceCoreClient::Start - Unsupported OS platform, abort!");
#endif

	LOG(INFO) << "VueceCoreClient::Start:Pump.DoLogin returned. Start disconnecting.";

	VueceNativeInterface::LogClientState(client_state);

	xmppSocket->SignalCloseEvent.disconnect(this);

	LOG(INFO) << "VueceCoreClient::Start - Calling XmppPump::DoDisconnect";

	// this will trigger a xmpp socket close event on VueceCoreClient if we
	// don't disconnect SignalCloseEvent from client
	pump.DoDisconnect();

	LOG(INFO) << "VueceCoreClient::Start - Calling shell->Stop()";

	shell->Stop();

	//NOTE: DONT CHANGE THE SEQUENCE

	//WONT WORK
	//	if(xmppSocket != NULL)
	//	{
	//		LOG(INFO) << "VueceCoreClient::Start:Deleting xmppSocket...";
	//
	//		delete xmppSocket;
	//		xmppSocket = NULL;
	//
	//		LOG(INFO) << "VueceCoreClient::Start:xmppSocket deleted.";
	//	}

	//LOG(INFO) << "Deleting mediaEngine ...";
	// 	delete mediaEngine;
	//	mediaEngine = NULL;
	//LOG(INFO) << "mediaEngine deleted.";

	LOG(INFO) << "====================================================";
	LOG(INFO) << "VueceCoreClient::Start:Call client finalized!!!!!";
	LOG(INFO) << "====================================================";

	vNativeInstance->OnClientStateChanged(vuece::CLIENT_EVENT_LOGOUT_OK, vuece::CLIENT_STATE_OFFLINE);

	if(bIsLoginFailed)
	{
		return RESULT_UNAUTHORIZED;
	}

	return RESULT_OK;
}



void VueceCoreClient::OnXmppClientStateChange(buzz::XmppEngine::State state) {

	LOG(INFO) << ("VueceCoreClient::OnXmppClientStateChange");

	VueceNativeInterface::LogClientState(client_state);

	switch (state) {
	case buzz::XmppEngine::STATE_START:
		LOG(INFO)<< "VueceCoreClient::OnStateChang - STATE_START: Attempting sign in...\n";
		break;
	case buzz::XmppEngine::STATE_OPENING:
		LOG(INFO) << ("VueceCoreClient::STATE_OPENING");
		break;

	case buzz::XmppEngine::STATE_OPEN:

		LOG(INFO) << "VueceCoreClient::STATE_OPEN - User successfully logged in.";

		bIsLoginFailed = false;

    	InitPhone();

		LOG(INFO)<< "Connection succeeded. Send your presence information\n";

		InitPresence();

//		StartJingleInfoTask();

		StartConnectionKeeper();

#if defined(WIN32)
#ifdef VCARD_ENABLED
		//TODO - Check if this is a Vuece device
		//query user's own vcard info here
//		SendVCardRequest(to);
#endif
#endif

		StateTransition(CoreClientFsmEvent_LoggedIn);

		break;

	case buzz::XmppEngine::STATE_CLOSED:
	{
		buzz::XmppEngine::Error error = xmpp_client_->GetError(NULL);

		LOG(INFO) << "VueceCoreClient::STATE_CLOSED - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
		LOG(INFO) << "VueceCoreClient::STATE_CLOSED - Connection ended, error is: " << (int)error;
		LOG(INFO) << "VueceCoreClient::OnXmppClientStateChange::Client will log out with reason: " << strerror(error) ;
		LOG(INFO) << "VueceCoreClient::STATE_CLOSED - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

		if(bConnectionFailed)
		{
			LOG(INFO) << "VueceCoreClient::STATE_CLOSED - Connection closed because of a connection failure. ";

			//not need to notify upper level because its already been notified in
			//VueceCoreClient::OnXmmpSocketClosedEvent
			return;
		}

		if(error == buzz::XmppEngine::ERROR_NONE)
		{
			LOG(INFO) << "There is no error, this is a normal sign out.";

			StateTransition(CoreClientFsmEvent_LoggedOut);
		}
		else if(error == buzz::XmppEngine::ERROR_UNAUTHORIZED )
		{
			LOG(INFO) << "Login failed.";
			bIsLoginFailed = true;

			if( StateTransition(CoreClientFsmEvent_AuthFailed) )
			{
				Quit();
			}

		}
		else if(error == buzz::XmppEngine::ERROR_STREAM )
		{
			LOG(INFO) << "Stream error encountered, log out now";
			if( StateTransition(CoreClientFsmEvent_ConnectionFailed) )
			{
				Quit();
			}
		}
		else{
			LOG(LS_WARNING) << "Other error code, quit anyway";

			//TODO - Make this system error for now
			StateTransition(CoreClientFsmEvent_SysErr);
		}

		break;
	}
	default:
		break;
	}
}

void VueceCoreClient::OnXmmpSocketClosedEvent(int err) {

	//the event is originated by OpenSSLAdapter::OnCloseEvent()

	/**
	 * IMPORTANT NOTE
	 * We consider ANY CLOSE event here as a connection failure, because if user
	 * signed out normally (by clicking 'Sign out' button), this event will not be
	 * fired here.
	 */
	LOG(LS_ERROR) << "VueceCoreClient::OnXmmpSocketClosedEvent:Err code: " << err;
 
	//if this is not a login error, then it's a connection error
	LOG(LS_ERROR) << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
	LOG(LS_ERROR) << "VueceCoreClient::OnXmmpSocketClosedEvent - This is a connection failure, sign out now.";
	LOG(LS_ERROR) << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Notify App Layer now!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

	bConnectionFailed = true;

	if( StateTransition(CoreClientFsmEvent_ConnectionFailed) )
	{
		//proceed
		Quit();
	}

}

void VueceCoreClient::InitPhone() {
	LOG(INFO)<< "VueceCoreClient - InitPhone";

	std::string client_unique = xmpp_client_->jid().Str();
	talk_base::InitRandom(client_unique.c_str(), client_unique.size());

	//-----------------------------------------------------------------------------
	LOG(INFO)<< "InitPhone - create worker thread";

	worker_thread_ = new talk_base::Thread();
	worker_thread_->SetName("SessionManager Worker Thread", NULL);

	// The worker thread must be started here since initialization of
	// the ChannelManager will generate messages that need to be
	// dispatched by it.
	worker_thread_->Start();

	network_manager_ = new talk_base::NetworkManager();

	//HttpPortAllocator  new cricket::HttpPortAllocator
	port_allocator_ = new cricket::HttpPortAllocator(network_manager_, "pcp");

	if (portallocator_flags_ != 0) {
		port_allocator_->set_flags(portallocator_flags_);
	}

	//check https://developers.google.com/talk/libjingle/important_concepts#threads for more details
	session_manager_ = new cricket::SessionManager(port_allocator_, worker_thread_);
	session_manager_->set_session_timeout(VUECE_SESSION_MGR_TIMEOUT);

	session_manager_->SignalRequestSignaling.connect(this, &VueceCoreClient::OnRequestSignaling);
	session_manager_->SignalSessionCreate.connect(this, &VueceCoreClient::OnSessionCreate);
	session_manager_->OnSignalingReady();

	session_manager_task_ = new cricket::SessionManagerTask(xmpp_client_, session_manager_);
	session_manager_task_->EnableOutgoingMessages();
	session_manager_task_->Start();
	//-----------------------------------------------------------------------------


	//-----------------------------------------------------------------------------
	LOG(INFO) << "Creating vuece file share client.";

//	media_stream_session_client_ = new cricket::VueceFileShareSessionClient(session_manager_, xmpp_client_->jid(), "pcp");
	media_stream_session_client_ = new cricket::VueceMediaStreamSessionClient(session_manager_, xmpp_client_->jid(), "vuece");
//	media_stream_session_client_->SetDownloadFolder(VUECE_DEFAULT_DOWNLOAD_FOLDER);

	//NOTE: When VueceFileShareSessionClient creates a session, this signal is used to notify VueceFileShareClient the creation
	//event so VueceFileShareClient can set the created session as its current session, current impl only supports one
	//single session at a time, in the future we may support multi-session
	media_stream_session_client_->SignalFileShareStateChanged.connect(this, &VueceCoreClient::OnFileShareStateChanged);
	media_stream_session_client_->SignalFileShareRequestReceived.connect(this, &VueceCoreClient::OnFileShareRequestReceived);


	media_stream_session_client_->SignalFileShareProgressUpdated.connect(this, &VueceCoreClient::OnFileShareProgressUpdated);
	media_stream_session_client_->SignalMusicStreamingProgressUpdated.connect(this, &VueceCoreClient::OnMusicStreamingProgressUpdated);

	media_stream_session_client_->SignalFileSharePreviewReceived.connect(this, &VueceCoreClient::OnFileSharePreviewReceived);

	media_stream_session_client_->SignalStreamPlayerReleased.connect(GetShell(), &VueceKernelShell::OnPlayerReleased);

	//for stream player only, disable it for now
	//NOTE - OnStreamPlayerStateChanged() is triggered only when the state is VueceStreamPlayerState_Stopped
	//Notification for other states is done in msandroid.cc, this method here is used because after the audio track writer
	//intances is destroyed, jvm is detached in msandroid.cc, so we need another path to notify the app layer
	//that the audio streamer resources are finalized
	media_stream_session_client_->SignalStreamPlayerStateChanged.connect(this, &VueceCoreClient::OnStreamPlayerStateChanged);
	//-----------------------------------------------------------------------------

	media_stream_session_client_->SignalStreamPlayerNotification.connect(this, &VueceCoreClient::OnStreamPlayerNotification);

	connection_keeper = new VueceConnectionKeeper(xmpp_client_);
}

bool VueceCoreClient::IsMusicStreaming(void)
{
#ifdef ANDROID
	if(media_stream_session_client_ != NULL)
	{
		return media_stream_session_client_->IsMusicStreaming();
	}
	else
	{
		return false;
	}
#endif

	return false;
}

int VueceCoreClient::GetCurrentMusicStreamingProgress(const std::string& share_id)
{

	VueceLogger::Debug("VueceCoreClient::GetCurrentMusicStreamingProgress, returning: %d", current_music_streaming_progress);

	return current_music_streaming_progress;
}


int VueceCoreClient::GetCurrentPlayingProgress(void)
{
//	if (VueceStreamPlayer::HasStreamEngine())
//	{
//		int p = VueceStreamPlayer::GetCurrentPlayingProgress();
//
//		VueceLogger::Debug("VueceCoreClient::GetCurrentPlayingProgress, returning: %d", p);
//
//		return p;
//	}
//	else
//	{
//		VueceLogger::Debug("VueceCoreClient::GetCurrentPlayingProgress, VueceStreamPlayer is empty, return 0");
//
//		return 0;
//	}

	LOG(LS_VERBOSE) << "VueceCoreClient::GetCurrentPlayingProgress: " << current_play_progress;

	return current_play_progress;

}



void VueceCoreClient::OnCurrentRemoteHubUnAvailable(void)
{
	LOG(LS_VERBOSE) << "VueceCoreClient::OnCurrentRemoteHubUnAvailable - Reset play and streaming progress.";
	current_play_progress = 0;
	current_music_streaming_progress = 0;

//	LOG(LS_VERBOSE) << "VueceCoreClient::OnCurrentRemoteHubUnAvailable - Reset net player state to IDLE";
//TODO - CONTINUE HERE!!! Do not set state here because this is async!!!!
//	VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
//	core_client->FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_Paused, vuece::NetworkPlayerState_Idle);
}



/**
 * Sends play request to network based on the specified media URI, start position and preive image requirement
 */
bool VueceCoreClient::SendPlayRequest(const char* media_uri, int start_pos, bool need_preview)
{
	bool ret = false;

	LOG(LS_VERBOSE) << "VueceCoreClient::SendPlayRequest - Start";

	std::ostringstream os;

	os <<  "{'action':'play', 'category':'music','control':'start','uri':'";
	os << media_uri << "', 'start':'";
	os << start_pos << "', 'need_preview': '";

	if(need_preview)
	{
		os << "1";
	}
	else
	{
		os << "0";
	}

	os << "' }";

	const char* current_server_node = VueceGlobalContext::GetCurrentServerNodeJid();

	LOG(LS_VERBOSE) << "VueceCoreClient::SendPlayRequest - Start position is: " << start_pos << " second";
	LOG(LS_VERBOSE) << "VueceCoreClient::SendPlayRequest - media_uriI is: " << media_uri;
	LOG(LS_VERBOSE) << "VueceCoreClient::SendPlayRequest - Current sever jid is: " << current_server_node;
	LOG(LS_VERBOSE) << "VueceCoreClient::SendPlayRequest - Last available chunk file ID is: " << VueceGlobalContext::GetLastAvailableAudioChunkFileIdx();

	ret = SendVHubMessage(current_server_node, "get", os.str());

	return ret;
}

void VueceCoreClient::FireNetworkPlayerNotification(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s)
{
	LOG(LS_VERBOSE) << "VueceCoreClient::FireNetworkPlayerNotification";
	vNativeInstance->OnNetworkPlayerStateChanged(e, s);
}


void VueceCoreClient::OnStreamPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg)
{
	LOG(LS_VERBOSE) << "VueceCoreClient::OnStreamPlayerNotification";

	if(msg->notificaiont_type == VueceStreamPlayerNotificationType_Request)
	{
		LOG(LS_VERBOSE) << "VueceCoreClient::OnStreamPlayerNotification - This is a request from Stream Player";

		switch(msg->request_type)
		{
		case VueceStreamPlayerRequest_Download_Next_BufWin:
		{
			/*
			 * SAMPLE message
			 * {'action':'play','category':'music','control':'start', 'uri':'f509eccf1c7ba0e04e0faab20906f6ba', 'start':'3095', 'need_preview': '1'}
			 */

			//If we are stopping player
			if(VueceGlobalContext::IsStopPlayStarted())
			{
				LOG(LS_VERBOSE) << "VueceCoreClient::OnStreamPlayerNotification - [TRICKYDEBUG] - bStopPlayStarted is true, req will not be sent.";
			}
			else
			{
				char current_music_uri[VUECE_MAX_SETTING_VALUE_LEN];
				VueceGlobalContext::GetCurrentMusicUri(current_music_uri);
				SendPlayRequest(current_music_uri, msg->value1, false);
			}

			break;
		}
		default:
		{
			VueceLogger::Fatal("VueceCoreClient::OnStreamPlayerNotification - Cannot handle this notification for now, abort.");
			break;
		}

		}

 		return;
	}

#ifdef ANDROID
	if(msg->notificaiont_type == VueceStreamPlayerNotificationType_StateChange)
	{
		VueceLogger::Debug("VueceCoreClient::OnStreamPlayerNotification - This is a state change notification message Stream Player, event = %d, value - %d", msg->value1, msg->value2);

		vNativeInstance->OnNetworkPlayerStateChanged((vuece::NetworkPlayerEvent )msg->value1, (vuece::NetworkPlayerState ) msg->value2);

		current_play_progress = VueceStreamPlayer::GetCurrentPlayingProgress();

		LOG(INFO) << "VueceCoreClient:OnStreamPlayerNotification:current_play_progress updated: " << current_play_progress;
	}
#endif

}

void VueceCoreClient::StartConnectionKeeper()
{
	LOG(INFO) <<  ("VueceCoreClient::StartConnectionKeeper");
	connection_keeper->Start();
}

void VueceCoreClient::OnRequestSignaling() {
	LOG(INFO) <<  ("VueceCoreClient::OnRequestSignaling");
	session_manager_->OnSignalingReady();
}

void VueceCoreClient::OnSessionCreate(cricket::Session* session, bool initiate) {
	LOG(INFO) << "VueceCoreClient:OnSessionCreate:Content type is: " << session->content_type();

	//Remove this check to see what will happen
//	if(session->content_type() != cricket::NS_JINGLE_RTP )
//	{
//		LOG(LS_VERBOSE) << "VueceCoreClient:OnSessionCreate:This is not a RTP media session, return for now.";
//	}
//	else
//	{
	LOG(INFO) << ("OnSessionCreate - Setting local allow_local_ips_ flag and initial protocol");

		session->set_allow_local_ips(allow_local_ips_);
		session->set_current_protocol(initial_protocol_);
//	}

}

void VueceCoreClient::StartJingleInfoTask()
{
	LOG(INFO) << ("StartJingleInfoTask");

	jingle_info_push = new buzz::JingleInfoTask(xmpp_client_);

	jingle_info_push->RefreshJingleInfoNow();
	jingle_info_push->SignalJingleInfo.connect(this, &VueceCoreClient::OnJingleInfo);
	jingle_info_push->Start();

}

void VueceCoreClient::OnJingleInfo(const std::string & relay_token, const std::vector<std::string> &relay_addresses, const std::vector<talk_base::SocketAddress> &stun_addresses) {
	LOG(INFO) << "OnJingleInfo: relay_token = " << relay_token;

	cricket::HttpPortAllocator* p = static_cast<cricket::HttpPortAllocator*>(port_allocator_);

	p->SetStunHosts(stun_addresses);
	p->SetRelayHosts(relay_addresses);
	p->SetRelayToken(relay_token);

	LOG(INFO) << "OnJingleInfo: port_allocator_ updated.";
}



void VueceCoreClient::InitPresence() {
	LOG(INFO) << ("InitPresence");
	//init presence push notification task
	presence_push_ = new buzz::PresencePushTask(xmpp_client_, this);
	presence_push_->SignalStatusUpdate.connect(this, &VueceCoreClient::OnRosterStatusUpdate);
	if(vNativeInstance!=NULL)
	{
		presence_push_->SignalStatusUpdate.connect(vNativeInstance, &VueceNativeInterface::OnRosterStatusUpdate);
	}else
	{
		LOG(LS_ERROR) << "vNativeInstance is NULL!";
	}

#ifdef MUC_ENABLED
	presence_push_->SignalMucJoined.connect(this, &VueceCoreClient::OnMucJoined);
	presence_push_->SignalMucLeft.connect(this, &VueceCoreClient::OnMucLeft);
	presence_push_->SignalMucStatusUpdate.connect(this, &VueceCoreClient::OnMucStatusUpdate);
#endif

	presence_push_->Start();

	//init present update outgoing task
	presence_out_ = new buzz::PresenceOutTask(xmpp_client_);
	presence_out_->Start();

	//init muc invite notification task
#ifdef MUC_ENABLED
	muc_invite_recv_ = new buzz::MucInviteRecvTask(xmpp_client_);
	muc_invite_recv_->SignalInviteReceived.connect(this, &VueceCoreClient::OnMucInviteReceived);
	muc_invite_recv_->Start();

	muc_invite_send_ = new buzz::MucInviteSendTask(xmpp_client_);
	muc_invite_send_->Start();
#endif

	//init muc outgoing task, not needed for now
//	friend_invite_send_ = new buzz::FriendInviteSendTask(xmpp_client_);
//	friend_invite_send_->Start();

	roster_query_send_ = new buzz::RosterQuerySendTask(xmpp_client_);
	roster_query_send_->Start();

	//NOTE: Need to send rost query before any other query after login
	/**
	 * SAMPLE REQUEST
	 * [[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:100): SEND >>>>>>>>>>>>>>>> : Tue Jul 28 18:40:17 2015
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):    <iq type="get" id="123456">
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):      <ros:query xmlns:ros="jabber:iq:roster"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):    </iq>

	 * SAMPLE RESPONSE
	 * [[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:104): RECV <<<<<<<<<<<<<<<< : Tue Jul 28 18:40:17 2015
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):    <iq to="alice@gmail.com/vuece.pc6ECF7AE6" id="123456" type="result">
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):      <query xmlns="jabber:iq:roster">
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):        <item jid="john@gmail.com" subscription="both" name="john"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):        <item jid="jack@gmail.com" subscription="both"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):        <item jid="tom@gmail.com" subscription="both"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):        <item jid="android.test.htc2@gmail.com" subscription="both"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):        <item jid="alice@gmail.com" subscription="none" name="alice"/>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):      </query>
[[2] 2015-07-28 18:40:17] [1384] Warning(DebugLog.cc:128):    </iq>
	 */
	LOG(INFO)<< "InitPresence. Query roster first\n";
	QueryRoster();

	roster_query_result_recv_ = new buzz::RosterQueryResultRecvTask(xmpp_client_);
	roster_query_result_recv_->SignalRosterRecieved.connect(vNativeInstance, &VueceNativeInterface::OnRosterReceived);
	roster_query_result_recv_->Start();

	roster_sub_resp_recev_ = new buzz::RosterSubResponseRecvTask(xmpp_client_);

//	roster_sub_resp_recev_->SignalRosterSubRespRecieved.connect(vNativeInstance, &VueceNativeInterface::OnRosterSubRespReceived);
	roster_sub_resp_recev_->SignalRosterSubRespRecieved.connect(this, &VueceCoreClient::OnRosterSubRespReceived);
	this->SigRosterSubscriptionMsgReceived.connect(vNativeInstance, &VueceNativeInterface::OnRosterSubRespReceived);

	roster_sub_resp_recev_->Start();


#ifdef CHAT_ENABLED

	LOG(LS_VERBOSE) << "VueceCoreClient::InitPresence - Chat functionality is enabled.";

	//init chat message push notification task
	chat_msg_push = new buzz::ChatPushTask(xmpp_client_);

	if(vNativeInstance!=NULL)
	{
		chat_msg_push->SignalChatMessageReceived.connect(vNativeInstance, &VueceNativeInterface::OnChatMessageReceived);
	}else
	{
		LOG(LS_ERROR) << "VueceCoreClient::InitPresence - vNativeInstance is NULL!";
	}

	chat_msg_push->Start();
#endif

	//init vCard push notification task
#ifdef VCARD_ENABLED
	//DDD
	vcard_iq_push = new buzz::VCardPushTask(xmpp_client_);
	if(vNativeInstance!=NULL)
	{
		vcard_iq_push->SignalVCardIQResultReceived.connect(this, &VueceCoreClient::OnVCardIQResultReceived);
	}else
	{
		LOG(LS_ERROR) << "VueceCoreClient::InitPresence - vNativeInstance is NULL!";
	}

	vcard_iq_push->Start();
#endif

	// vhub
	vhub_get = new buzz::VHubGetTask(xmpp_client_);

	if(vNativeInstance!=NULL)
	{
		vhub_get->SignalVHubGetMessageReceived.connect(vNativeInstance, &VueceNativeInterface::OnVHubGetMessageReceived);

#ifdef ANDROID
//		vhub_get->SignalRemoteSessionResourceReleaseMsgReceived.connect(media_stream_session_client_, &cricket::VueceMediaStreamSessionClient::OnRemoteSessionResourceReleased);
		vhub_get->SignalRemoteSessionResourceReleaseMsgReceived.connect(shell, &VueceKernelShell::OnRemoteSessionResourceReleased);
#endif

	}
	else
	{
		LOG(LS_ERROR) << "VueceCoreClient::InitPresence - vNativeInstance is NULL!";
	}

	vhub_get->Start();
	vhub_result = new buzz::VHubResultTask(xmpp_client_);
	if(vNativeInstance!=NULL)
	{
		vhub_result->SignalVHubResultMessageReceived.connect(vNativeInstance, &VueceNativeInterface::OnVHubResultMessageReceived);
		vhub_result->SignalCurrentStreamTargetNotValid.connect(shell, &VueceKernelShell::OnInvalidStreamingTargetMsgReceived);

	}
	else
	{
		LOG(LS_ERROR) << "VueceCoreClient::InitPresence - vNativeInstance is NULL!";
	}
	vhub_result->Start();

	RefreshStatus();
}

void VueceCoreClient::OnRosterSubRespReceived(const buzz::XmlElement* stanza)
{
	LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived";
	VueceRosterSubscriptionType type = VueceRosterSubscriptionType_NA;

	std::string messageType = stanza->Attr(buzz::QN_TYPE);
	std::string from = stanza->Attr(buzz::QN_FROM);

	LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived::Source: " << from << ", type: " << messageType;
	VueceLogger::Warn("OnRosterSubRespReceive::Source: %s, type: %s", from.c_str(), messageType.c_str());

	if (messageType == buzz::STR_SUBSCRIBED) {
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Message type: subscribed";
		type = VueceRosterSubscriptionType_Subscribed;
	} else 	if (messageType == buzz::STR_SUBSCRIBE) {
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Message type: subscribe";
		type = VueceRosterSubscriptionType_Subscribe;
	} else 	if (messageType == buzz::STR_UNSUBSCRIBE) {
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Message type: unsubscribe";
		type = VueceRosterSubscriptionType_Unsubscribe;
	} else 	if (messageType == buzz::STR_UNSUBSCRIBED) {
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Message type: unsubscribed";
		type = VueceRosterSubscriptionType_Unsubscribed;
	} else 	if (messageType == buzz::STR_UNAVAILABLE) {
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Message type: unavailable";
		type = VueceRosterSubscriptionType_Unavailable;
	}

	VueceRosterSubscriptionMsg m;
	memset(&m, 0, sizeof(m));

	strcpy(m.user_id, from.c_str());
	m.subscribe_type = type;

	//If remote roster became offline for various reason, check and update active streaming session table
	//because remote roster might have just started a session
	if(type == VueceRosterSubscriptionType_Unavailable)
	{
		LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterSubRespReceived:Roster not available, update active streaming session table";
		media_stream_session_client_->CancelSessionsByJid(from);
	}

	SigRosterSubscriptionMsgReceived(&m);
}

#ifdef VCARD_ENABLED
void VueceCoreClient::OnVCardIQResultReceived(const buzz::Jid& jid, const std::string& fn, const std::string& image_data_b64) {
	LOG(LS_VERBOSE) << "VueceCoreClient:Received a vcard query response for: " << jid.Str().c_str();
	LOG(LS_VERBOSE) << "VueceCoreClient:Full name:" << fn;

	std::string full_jid (jid.Str());

	//update roster list
	RosterMap::iterator iter = roster_->find(jid.Str());
	if (iter != roster_->end())
	{
		(*iter).second.full_name = fn;
		(*iter).second.image_data = image_data_b64;
	}

	vNativeInstance->OnRosterVCardReceived(full_jid, fn, image_data_b64);

	PrintRoster();
}
#endif


void VueceCoreClient::RefreshStatus() {
	LOG(INFO) << ("VueceCoreClient - RefreshStatus");

	char device_name[VUECE_MAX_SETTING_VALUE_LEN+1];
	char app_ver[VUECE_MAX_SETTING_VALUE_LEN+1];

	memset(device_name, 0, sizeof(device_name));
	memset(app_ver, 0, sizeof(app_ver));

	VueceGlobalContext::GetDeviceName(device_name);
	VueceGlobalContext::GetAppVersion(app_ver);

//	int media_caps = media_client_->GetCapabilities();
	my_status_.set_jid(xmpp_client_->jid());
	my_status_.set_available(true);
    my_status_.set_invisible(true);
	my_status_.set_show(buzz::Status::SHOW_OFFLINE); // show busy status
	
#if defined(WIN32)
	my_status_.set_status("On Vuece PC");
#endif

#if defined(ANDROID)
	my_status_.set_status("On Vuece Android");
#endif

	my_status_.set_priority(-127);
	my_status_.set_know_capabilities(true);
	//my_status_.set_pmuc_capability(false);
	//my_status_.set_phone_capability(true);
	my_status_.set_fileshare_capability(true);
#ifdef VIDEO_ENABLED
	//my_status_.set_video_capability(true);
	//my_status_.set_camera_capability(true);
#else
	my_status_.set_video_capability(false);
	my_status_.set_camera_capability(false);
#endif
	my_status_.set_is_google_client(true);
	my_status_.set_version(app_ver);

	//Set device name here
	my_status_.set_device_name(device_name);

	my_status_.set_is_vuece_device(true);

	//set hub capability
	if(VueceGlobalContext::GetAppRole() == VueceAppRole_Media_Hub)
	{
		my_status_.set_vhub_capability(true);
	}

	presence_out_->Send(my_status_);
}

void VueceCoreClient::OnRosterStatusUpdate(const buzz::Status& status)
{
	LOG(LS_VERBOSE) << "VueceCoreClient::OnRosterStatusUpdate";
	std::string testTarget = "jack@gmail.com";

	RosterItem item;
	item.jid = status.jid();
	item.show = status.show();
	item.status = status.status();
	item.device_name = status.device_name();
	item.version = status.version();

	std::string key = item.jid.Str();
	std::string node = item.jid.node();

	LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:jid string: " << key
			<< ", node name: " << node << ", device name: " << item.device_name
			<< ", version: " << item.version;

	//Following code is needed for vuece status update, keep it for future reference
//	LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Buddy: " << item.jid.Str() << "show:" << (int)item.show << ", status: " << DescribeStatus(item.show, item.status);
//
//	if (status.available()){
//		LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Buddy: " << item.jid.Str() << " is available";
//	}else{
//		LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Buddy: " << item.jid.Str() << " is NOT available";
//	}
//
//	if (status.phone_capability()){
//		LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Buddy: " << item.jid.Str() << " has phone capability";
//	}else{
//		LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Buddy: " << item.jid.Str() << " has NO phone capability";
//	}

	//we only care about vuece device for now.
	if (status.is_vuece_device())
	{
		LOG(INFO) << "VueceCoreClient::OnRosterStatusUpdate:Adding vuece device to to roster list:  " << key;
		(*roster_)[key] = item;

#ifdef VCARD_ENABLED
		SendVCardRequest(key);
#endif

	}
	else
	{
		LOG(INFO) <<  "VueceCoreClient::OnRosterStatusUpdate:Removing non vuece device from roster list: " << key ;

		RosterMap::iterator iter = roster_->find(key);

		if (iter != roster_->end())
			roster_->erase(iter);
	}

	//TEST CODE: REMOVE LATER
	///////////////////////////////////////////
	//Test code for voice mail
	if(status.jid().BareJid().Str() == testTarget)
	{
//		LOG_F(LS_VERBOSE) << "VueceCoreClient::OnRosterStatusUpdate:Start voice mail test with remote buddy: " << key;
		//TEST - Voice mail, currently doesn't work
		//		CallVoicemail(testTarget);
	}

}

void VueceCoreClient::AcceptFileShare(const std::string &share_id,
		const std::string & target_download_folder, const std::string &target_file_name)
{
	LOG(INFO) << "VueceCoreClient::AcceptFileShare: " << share_id
			<< ", target_download_folder: " << target_download_folder
			<< ", target_file_name: " << target_file_name;

	// Note the session is created internally by VueceFileShareSessionClient::OnSessionCreate when
	// initiator is remote client
	media_stream_session_client_->Accept(share_id, target_download_folder, target_file_name);
}

void VueceCoreClient::CancelFileShare(const std::string &share_id)
{
	LOG(INFO) << "VueceCoreClient::CancelFileShare: " << share_id;

	media_stream_session_client_->Cancel(share_id);
}

void VueceCoreClient::DeclineFileShare(const std::string &share_id) {
	LOG(INFO) << "VueceCoreClient::DeclineFileShare: " << share_id;
	media_stream_session_client_->Decline(share_id);
}

void VueceCoreClient::StopStreamPlayer(const std::string &share_id) {
	LOG(INFO) << "VueceCoreClient::StopStreamPlayer: " << share_id;
#ifdef ANDROID
	media_stream_session_client_->StopStreamPlayer(share_id);
#endif
}

void VueceCoreClient::ResumeStreamPlayer(int resume_pos) {

	LOG(INFO) << "VueceCoreClient::ResumeStreamPlayer - resume_pos: " << resume_pos
			<< ", current_start_pos: " << VueceGlobalContext::GetFirstFramePositionSec()
			<< ", last available chunk file id: " << VueceGlobalContext::GetLastAvailableAudioChunkFileIdx()
			<< ", last stream termination position in seconds: " << VueceGlobalContext::GetLastStreamTerminationPositionSec();

#ifdef ANDROID
	media_stream_session_client_->ResumeStreamPlayer(resume_pos);
#endif
}

//TODO REMOVE THIS, NOT USED.
void VueceCoreClient::MediaStreamSeek(const std::string &pos_sec){
	LOG(INFO) << "VueceCoreClient::MediaStreamSeek: " << pos_sec;
	int lPos = atoi( pos_sec.c_str() );
#ifdef ANDROID
	media_stream_session_client_->SeekStream(lPos);
#endif

}

int VueceCoreClient::PlayInternal(const std::string &jid, const std::string &song_uuid)
{
	int ret = 0;

#ifdef ANDROID

	VueceGlobalContext::SetCurrentMusicUri(song_uuid.c_str());
	VueceGlobalContext::SetCurrentServerNodeJid(jid.c_str());

	VueceLogger::Debug("VueceCoreClient::PlayInternal - Start");

	// psudo code
		/*
		 if current network player state is waiting, return -1
		 state changed to waiting
		 send notification (if playing, send 110, if not playing send 100)
		 playing? stop playing
		 streaming? stop streaming
		 send a command to pc client using SendVHubPlayRequest with starting position being 0, need preview
		 auto accept if the jabber request is the requested song
		 if timeout, terminate everything, recycle resources, state changed to idle, send notification
		 when network error, state changed to idle, send notification
		 */

	//stop player, no need to check current state because it's self-managed
	StopStreamPlayer("");

	VueceLogger::Debug("VueceCoreClient::PlayInternal - StopStreamPlayer() returned. reset all streaming params because this is a new play");

	VueceStreamPlayer::ResetCurrentStreamingParams();

	VueceStreamPlayer::LogCurrentFsmState();

	current_song_uuid = song_uuid;

	current_music_streaming_progress = 0;

	/*
	 * send play request for this new song
	 */
	if( SendPlayRequest(current_song_uuid.c_str(), 0, true) )
	{

		VueceLogger::Debug("VueceCoreClient::PlayInternal - Fire OnNetworkPlayerStateChanged");

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
		vNativeInstance->OnNetworkPlayerStateChanged(vuece::NetworkPlayerEvent_PlayReqSent, vuece::NetworkPlayerState_Waiting);

		//TODO Start a watchdog timer here to guarantee error condition is handled properly
		//StartWatchdogTimer();

		// If everything is Ok, streaming session should be started soon and network player state should be changed to PLAYING
		// StreamPlayer should fire a notification via OnStreamPlayerNotification() callback
	}
	else
	{
		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_NetworkErr, vuece::NetworkPlayerState_Idle);
	}

#endif

	VueceLogger::Debug("VueceCoreClient::PlayInternal - End");

	return ret;
}

int VueceCoreClient::PauseInternal()
{
	// pause playing, streaming is not affected
	int ret = 0;

#ifdef ANDROID

	VueceLogger::Debug("VueceCoreClient::PauseInternal---------------------");
	VueceLogger::Debug("VueceCoreClient::PauseInternal - Start");
	VueceLogger::Debug("VueceCoreClient::PauseInternal---------------------");

	current_play_progress = VueceStreamPlayer::GetCurrentPlayingProgress();

	VueceLogger::Info("VueceCoreClient::PauseInternal - Remember current play progress: %d", current_play_progress);

	//TODO Fully stop for now
	StopStreamPlayer("");

	VueceLogger::Debug("VueceCoreClient::PauseInternal---------------------");
	VueceLogger::Debug("VueceCoreClient::PauseInternal - Done");
	VueceLogger::Debug("VueceCoreClient::PauseInternal---------------------");
#endif

	return ret;
}


int VueceCoreClient::ResumeInternal()
{
	// call Seek(current position)

#ifdef ANDROID

	std::ostringstream s;
	s << current_play_progress;

	const std::string pos_s(s.str());

	VueceLogger::Debug("VueceCoreClient::ResumeInternal - Start, Previously saved play progress: %s", pos_s.c_str());
	VueceLogger::Debug("VueceCoreClient::ResumeInternal - Calling Seek() with current progress: %d", current_play_progress);

	SeekInternal(current_play_progress);

	VueceLogger::Debug("VueceCoreClient::ResumeInternal - Done");

#endif

	return current_play_progress;
}

int VueceCoreClient::SeekInternal(int new_resume_pos)
{
	// psudo code
	/*
	 is data available locally? compare new position with min/max downloaded position
	 if yes, state changed to playing, resume playing straightaway
	 if no, state changed to waiting,
	 playing? stop playing
	 streaming? stop streaming
	 send a command to pc client using SendVHubPlayRequest with starting position being the requested position, no preview
	 auto accept if the jabber request is the requested song
	 if timeout, terminate everything, recycle resources, state changed to idle, send notification
	 when network error, state changed to idle, send notification
	 */

	int ret = 0;

#ifdef ANDROID

	VueceLogger::Debug("VueceCoreClient::SeekInternal - START");

	VueceStreamPlayer::LogCurrentStreamingParams();

	current_play_progress = new_resume_pos;

	VueceLogger::Debug("VueceCoreClient::SeekInternal - current_play_progress updated to: %d", current_play_progress);

	if(VueceStreamPlayer::CanLocalSeekProcceed(new_resume_pos))
	{
		VueceLogger::Debug("VueceCoreClient::SeekInternal - CanLocalSeekProcceed() returned YES");
		VueceLogger::Debug("VueceCoreClient::SeekInternal - Current first frame position: %d", VueceGlobalContext::GetFirstFramePositionSec());

		ResumeStreamPlayer(new_resume_pos);

		//TODO - Rework
//		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Playing);
//		FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_Started, vuece::NetworkPlayerState_Playing);

	}
	else
	{
		VueceLogger::Debug("VueceCoreClient::SeekInternal - CanLocalSeekProcceed() returned NO, clean up streaming parameters");

		VueceStreamPlayer::ResetCurrentStreamingParams();

		VueceLogger::Debug("VueceCoreClient::SeekInternal - Reset resume pos and first frame pos");

		VueceGlobalContext::SetNewResumePos(new_resume_pos);
		VueceGlobalContext::SetFirstFramePositionSec(new_resume_pos);

		VueceLogger::Debug("VueceCoreClient::SeekInternal - new_resume_pos and iFirstFramePosSec updated: %lu second", new_resume_pos);

		VueceStreamPlayer::LogCurrentStreamingParams();

		VueceLogger::Debug("VueceCoreClient::SeekInternal - Send play request now.");

		if( SendPlayRequest(current_song_uuid.c_str(), new_resume_pos, false) )
		{
			//TODO - Rework
			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Waiting);
			FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_PlayReqSent, vuece::NetworkPlayerState_Waiting);

			//TODO Start a watchdog timer here to guarantee error condition is handled properly
			//StartWatchdogTimer();

			// If everything is Ok, streaming session should be started soon and network player state should be changed to PLAYING
			// StreamPlayer should fire a notification via OnStreamPlayerNotification() callback
		}
		else
		{
			VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
			FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_NetworkErr, vuece::NetworkPlayerState_Idle);
		}

	}

#endif

	return ret;
}

void VueceCoreClient::OnVueceCommandReceived(int cmdIdx, const char* cmdString)
{
	shell->OnVueceCommandReceived(cmdIdx, cmdString);
}

int VueceCoreClient::SendFile(
		const std::string& share_id,
		const std::string& user,
		const std::string& pathname,
		const std::string& width,
		const std::string& height,
		const std::string& preview_file_path,
		const std::string& start_pos,
		const std::string& need_preview)
{
	LOG(INFO) << "VueceCoreClient::SendFile - Start";

	cricket::FileShareManifest *manifest = new cricket::FileShareManifest();
	size_t size = 0;
	talk_base::Pathname local_name;
	local_name.SetPathname(pathname);
	buzz::Jid send_jid(user);
	bool preview_needed = false;
	bool bFAbsent = false;
	bool bIsFile = false;

	int wi = atoi(width.c_str());
	int hi = atoi(height.c_str());

	LOG(INFO) << "VueceCoreClient::SendFile - File location: " << local_name.folder();
	LOG(INFO) << "VueceCoreClient::SendFile - File width: " << wi;
	LOG(INFO) << "VueceCoreClient::SendFile - File height: " << hi;
	LOG(INFO) << "VueceCoreClient::SendFile - File to be shared(absolute file path): " << pathname;
	LOG(INFO) << "VueceCoreClient::SendFile - Preview file location: " << preview_file_path;

	LOG(LS_VERBOSE) << "VueceCoreClient::SendFile - Check if this is a valid file";

	bFAbsent =  talk_base::Filesystem::IsAbsent(local_name);

	if(bFAbsent)
	{
		LOG(WARNING) << "VueceCoreClient::SendFile - This file does not exist, stream session cannot be initialized.";
		return VueceStreamSessionInitErrType_FileAbsent;
	}

//	bIsFile = talk_base::Filesystem::IsFile(pathname);
//
//	if(!bIsFile)
//	{
//		LOG(WARNING) << "VueceCoreClient::SendFile - This is not a file , stream session cannot be initialized.";
//		return VueceStreamSessionInitErrType_NotAFile;
//	}

	talk_base::Filesystem::GetFileSize(local_name, &size);
	LOG(INFO) << "VueceCoreClient::SendFile - File size: " << size;

	if(size <= 0)
	{
		LOG(WARNING) << "VueceCoreClient::SendFile - This is an empty file , stream session cannot be initialized.";
		return VueceStreamSessionInitErrType_EmptyFile;
	}

	// Detect file type at first, if this is an image, and preview feature
	// is enabled, then we use AddImage instead of AddFile
	if(VueceGlobalContext::IsImgPreviewEnabled())
	{
		LOG(INFO) << "VueceCoreClient::SendFile - Preview is enabled.";

		if(wi > 0 && hi > 0)
		{
			LOG(INFO) << "VueceCoreClient::SendFile - Size info found.";
			manifest->AddImage(local_name.filename(), size, wi, hi);

			preview_needed = true;
		}
		else
		{
			LOG(INFO) << "VueceCoreClient::SendFile - File will be treated as normal item because no size info found.";
			manifest->AddFile(local_name.filename(), size);
		}
	}
	else
	{
		LOG(INFO) << "VueceCoreClient::SendFile - Preview is disabled.";
		manifest->AddFile(local_name.filename(), size);
	}

	media_stream_session_client_->CreateMediaStreamSessionAsInitiator(
			share_id,
			send_jid,
			local_name.folder(),
			preview_needed,
			pathname,
			preview_file_path,
			start_pos,
			manifest
			);

	return VueceStreamSessionInitErrType_OK;

}

#ifdef VUECE_APP_ROLE_HUB

bool VueceCoreClient::BrowseMediaItem(const std::string &itemID, std::ostringstream& resp)
{
	LOG(INFO) << "VueceCoreClient::BrowseMediaItem - itemID: " << itemID;

	std::string targetUri (itemID);

	if(targetUri.length() == 0){
		 targetUri = talk_base::MD5(itemID);
		 LOG(INFO) << "VueceCoreClient::BrowseMediaItem - browse root with uri: " << targetUri;
	}

	//TODO - Handle DB error here, generate an error response with a reason

	VueceMediaItemList* list = dbMgr->BrowseMediaItem(targetUri);

	if(list != NULL)
	{
		VueceWinUtilities::GenerateJsonMsg(list, targetUri, resp);
		LOG(INFO) << "BrowseMediaItem - Final json message: " << resp.str();

		delete list;
	}

	return false;
}

vuece::VueceMediaItem*  VueceCoreClient::LocateMediaItemInDB(const std::string& user, const std::string& targetUri)
{
	LOG(INFO) << "VueceCoreClient::LocateMediaItemInDB: Uri: " << targetUri;

	VueceMediaItemList* resultList = dbMgr->QueryMediaItemWithUri(targetUri);
	if(resultList->size() < 1){
		LOG(INFO) << "VueceCoreClient::LocateMediaItemInDB:Could not find this item:  " << targetUri;
		SignalVueceEvent(VueceEvent_FileAccess_Denied, user.c_str(), targetUri.c_str());
		return NULL;
	}

	LOG(INFO) << "VueceCoreClient::LocateMediaItemInDB:Target successfully located";

	//We should only find one item, return the first anyway if we find more than one
	std::list<vuece::VueceMediaItem*>::iterator iter = resultList->begin();
	VueceMediaItem* targetItem = *iter;

	return targetItem;
}

bool VueceCoreClient::ApplyLimitationOnRequestingClient(
		 const buzz::Jid& targetJid,
		const std::string& content_id)
{
	bool ret = false;
	bool owner_bypass = false;

	LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient:: jid = " << targetJid.Str() << ", content_id = "
		<< content_id << ", ower's hub id: " << iMyBareJid;

	if(!bAllowConcurrentStreaming)
	{
		int active_sess_nr = media_stream_session_client_->GetSessionNr();
		LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Concurrent streaming is not allowed, check active session number: " << active_sess_nr;

		if(active_sess_nr >= 1)
		{
			LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Active session number >= 1, deny current request. " << active_sess_nr;
			ret = false;

			return ret;
		}
	}

	//TODO - This needs to be refined.
	if(owner_bypass)
	{
		LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Owner will be allowed to bypass";
	}
	else
	{
		LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Owner will NOT be allowed to bypass";
	}

	if(owner_bypass && strcmp(iMyBareJid, targetJid.BareJid().Str().c_str()) == 0)
	{
		LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - The request is from the owner's device, there is no limitation for now.";
		ret = true;
	}
	else
	{
		//check the size of session map
		int active_sess_nr = media_stream_session_client_->GetSessionNr();
		int test_value = VueceGlobalContext::GetMaxConcurrentStreaming();

		LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Current active session nr = " << active_sess_nr
				<< ", pre-configured limit = " << test_value;

		if(active_sess_nr >= test_value)
		{
			LOG(LS_WARNING) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Request will be denied";

			ret = false;
		}
		else
		{
			LOG(LS_VERBOSE) << "VueceCoreClient::ApplyLimitationOnRequestingClient - Request will be allowed";

			ret = true;
		}
	}
	return ret;
}

bool VueceCoreClient::IsRemoteClientAlreadyInServe(
		const std::string& jid)
{
	bool ret = false;

	LOG(LS_VERBOSE) << "VueceCoreClient::IsRemoteClientAlreadyInServe:: jid = " << jid;

	RemoteNodeServingMap::iterator iter = remoteNodeServingMap->find(jid);

	if (iter != remoteNodeServingMap->end()) {
		std::string current_cid = (*iter).second;

		LOG(LS_WARNING) << "VueceCoreClient:IsRemoteClientAlreadyInServe - Jid found in current serving map";
		ret =  true;
	}

	return ret;

}

bool VueceCoreClient::AddActiveStreamingDevice(
		const std::string& jid,
		const std::string& content_id)
{
	bool ret = false;

	LOG(LS_VERBOSE) << "VueceCoreClient::AddActiveStreamingDevice:: jid = " << jid << ", content_id = " << content_id;

	RemoteNodeServingMap::iterator iter = remoteNodeServingMap->find(jid);

	if (iter == remoteNodeServingMap->end()) {

		VueceStreamingDevice d;
		memset(&d, 0, sizeof(d));

		d.activity = VueceRemoteDeviceActivityType_StreamingStarted;

		strcpy(d.user_id, jid.c_str());
//		strcpy(d.file_url, content_id.c_str());

		//Need to decode to UTF8 otherwise windows UI cannot display Chinese properly
		std::string tmp = VueceWinUtilities::ws2s(VueceWinUtilities::utf8_decode(content_id));
		strcpy(d.file_url, tmp.c_str());

		//Retrieve user's full name and device name from roster list
		RosterMap::iterator iter = roster_->find(jid);
		if (iter != roster_->end())
		{
//			(*iter).second.image_data = imgData;
			strcpy(d.user_name, (*iter).second.full_name .c_str());
			strcpy(d.device_name, (*iter).second.device_name .c_str());

			LOG(LS_VERBOSE) << "VueceCoreClient::AddActiveStreamingDevice:: Full name = " << d.user_name;
		}



		LOG(LS_VERBOSE) << "VueceCoreClient:AddActiveStreamingDevice:Could not find this jid, not serving it now";

		//Add this streaming node
		(*remoteNodeServingMap)[jid] = content_id;

		LOG(LS_VERBOSE) << "VueceCoreClient:AddActiveStreamingDevice:Fire notification to update UI";

		//fire notification to UI layer
		vNativeInstance->OnRemoteDeviceActivity(&d);

		ret =  true;
	}
	else
	{
		LOG(LS_ERROR) << "VueceCoreClient:AddActiveStreamingDevice - This device already exist, sth is not right";
		ret =  false;
	}

	return ret;
}

bool VueceCoreClient::ValidateStreamingDevice(
		VueceStreamingDevice* dev)
{
	bool ret = false;

//	std::string full_jid(dev->user_id);
//
//	VueceLogger::Debug("%s - jid: %s", __FUNCTION__, dev->user_id);
//
//	RemoteNodeServingMap::iterator iter = remoteActiveDeviceMap->find(full_jid);
//
//	if (iter == remoteActiveDeviceMap->end()) {
//		LOG(LS_VERBOSE) << "VueceCoreClient:AddActiveStreamingDevice:Could not find this jid, not serving it now, request can continue";
//
//		//Add this streaming node
//		(*remoteActiveDeviceMap)[full_jid] = content_id;
//
//		ret =  true;
//	}
//	else
//	{
//		std::string current_cid = (*iter).second;
//
//		LOG(LS_WARNING) << "VueceCoreClient:AddActiveStreamingDevice - Jid found in current serving map, request will be ignored.";
//		LOG(LS_VERBOSE) << "VueceCoreClient:AddActiveStreamingDevice - Hub is currently streaming some content for this node, path = " << current_cid;
//		ret =  false;
//	}

	return ret;
}


bool VueceCoreClient::RemoveStreamingNode(
		const std::string& remote_jid)
{
	bool ret = false;

	LOG(LS_VERBOSE) << "VueceCoreClient::RemoveStreamingNode - remote_jid = " << remote_jid;

	RemoteNodeServingMap::iterator iter = remoteNodeServingMap->find(remote_jid);

	if (iter != remoteNodeServingMap->end())
	{
		VueceStreamingDevice d;
		memset(&d, 0, sizeof(d));

		LOG(LS_VERBOSE) << "VueceCoreClient::RemoveStreamingNode - node located, remove it now.";

		remoteNodeServingMap->erase(iter);

		d.activity = VueceRemoteDeviceActivityType_StreamingTerminated;
		strcpy(d.user_id, remote_jid.c_str());

		LOG(LS_VERBOSE) << "VueceCoreClient:RemoveStreamingNode:Fire notification to update UI";

		//fire notification to UI layer
		vNativeInstance->OnRemoteDeviceActivity(&d);

		ret = true;
	}
	else
	{
		LOG(LS_WARNING) << "VueceCoreClient::RemoveStreamingNode - node cannot be located";
	}

	return ret;
}


/**
 * Starts media streaming to remote client
 */
int VueceCoreClient::InitiateMusicStreamDB(
		const std::string& share_id,
		const std::string& target_jid,
		const std::string& absItemPathUtf8,
		const std::string& width,
		const std::string& height,
		const std::string& preview_file_path,
		const std::string& start_pos,
		const std::string& need_preview,
		vuece::VueceMediaItem* mediaItem)
{
	size_t size = 0;
	buzz::Jid jid_target(target_jid);
	int wi = atoi(width.c_str());
	int hi = atoi(height.c_str());
	int previewNeeded = atoi(need_preview.c_str());
	bool bPreviewNeeded = false;
	bool bFAbsent = false;
	bool bIsFile = false;
	talk_base::Pathname pathUtf8 (absItemPathUtf8);

	LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:Start -  remote full jid: " << target_jid << ", Abs content path: " << absItemPathUtf8;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:Remote bare jid: " << jid_target.BareJid().Str().c_str();

	GetStreamSessionClien()->ListAllSessionIDs();

	if(IsRemoteClientAlreadyInServe(target_jid))
	{
		LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:IsRemoteClientAlreadyInServe returned true, ignore and continue.";
	}
	else
	{
		AddActiveStreamingDevice(target_jid, absItemPathUtf8);
	}

	if(!AddActiveStreamingDevice(target_jid, absItemPathUtf8))
	{
//		LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:AddActiveStreamingDevice returned false, cannot continue.";
		LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:AddActiveStreamingDevice returned false, ignore and continue.";
//		return VueceStreamSessionInitErrType_NodeBusy;
	}

	if(!ApplyLimitationOnRequestingClient(jid_target, absItemPathUtf8))
	{
		LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:ApplyLimitationOnRequestingClient returned false, deny current request.";
		return VueceStreamSessionInitErrType_NodeBusy;
	}

	std::string resizedPreviewFilePath ("");

	//Query the target item from database at first
	// if not found - return an access denied error
	// if found - get the actual path, validate the path, the play the actual file
	cricket::FileShareManifest *manifest = new cricket::FileShareManifest();
	std::string final_preview_path;
	talk_base::Pathname raw_local_path ("");

	LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB:Target item was found: " << absItemPathUtf8;

	raw_local_path.AppendPathname(VueceWinUtilities::ws2s(VueceWinUtilities::utf8_decode(absItemPathUtf8)));

	LOG(INFO) << "VueceCoreClient::InitiateMusicStreamDB - Absolute target file path: " << raw_local_path.pathname();

	bFAbsent =  talk_base::Filesystem::IsAbsent(pathUtf8);

	if(bFAbsent)
	{
		LOG(WARNING) << "VueceCoreClient::InitiateMusicStreamDB - This file pathUtf8 does not exist, stream session cannot be initialized.";
		RemoveStreamingNode(target_jid);
		return VueceStreamSessionInitErrType_FileAbsent;
	}

	talk_base::Filesystem::GetFileSize(pathUtf8, &size);

	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File size: " << size;

	if(size <= 0)
	{
		LOG(WARNING) << "VueceCoreClient::InitiateMusicStreamDB - This is an empty file , stream session cannot be initialized.";
		RemoveStreamingNode(target_jid);
		return VueceStreamSessionInitErrType_EmptyFile;
	}

//	LOG(INFO) << "VueceCoreClient::InitiateMusicStream: - ID of file to be shared(base64): " << targetUri;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview file location: " << preview_file_path;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Start position: " << start_pos;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview needed? " << previewNeeded;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File size: " << size;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File width: " << wi;
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File height: " << hi;

	LOG(INFO) << "------ Target file location info START ---------------";
	LOG(INFO) << "Absolute target file path: " << raw_local_path.pathname();
	LOG(INFO) << "filename: " << raw_local_path.filename();
	LOG(INFO) << "folder path: " << raw_local_path.folder();
	LOG(INFO) << "folder name: " << raw_local_path.folder_name();
	LOG(INFO) << "parent folder: " << raw_local_path.parent_folder();
	LOG(INFO) << "url: " << raw_local_path.url();
	LOG(INFO) << "folder delimiter: " << raw_local_path.folder_delimiter();
	LOG(INFO) << "------ Target file location info END ---------------";

	std::string raw_pathname=talk_base::Base64::Encode(VueceWinUtilities::ws2s(VueceWinUtilities::utf8_decode(absItemPathUtf8)));
	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File location in B64 format(raw_pathname): " << raw_pathname;

//	previewNeeded = 0;

	if(previewNeeded == 1)
	{
		bPreviewNeeded = true;
	}

	//We need to do more work here, if the file type is audio or video,
	//we use max file size, otherwise we use actual file size

	//Note - We only allow hub to send file in streaming mode
	if(VueceGlobalContext::GetAppRole() != VueceAppRole_Media_Hub)
	{
		LOG(LS_ERROR) << "VueceCoreClient::InitiateMusicStream - FATAL ERROR!!! Streaming file in non-hub peer is not allowed, return now";
		ASSERT(false);
		return VueceStreamSessionInitErrType_SysErr;
	}

	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - In media streaming mode, use maximum file size";

	size = (std::numeric_limits<size_t>::max)();

	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - This is a hub, use max file size for media data: " << size;

	//In order to minimize libjingle dependency, we do preview work in callclient
	if(bPreviewNeeded)
	{
		std::ostringstream raw_local_path_preview;

		talk_base::Pathname raw_temp_file_path;
		talk_base::Filesystem::GetTemporaryFolder(raw_temp_file_path, true, NULL);

		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Temp folder is: " << raw_temp_file_path.pathname();

		if (!talk_base::CreateUniqueFile(raw_temp_file_path, false))
		{
			LOG(LS_ERROR) << "VueceCoreClient::InitiateMusicStream - :Cannot acquire unique file name, return now.";
			return VueceStreamSessionInitErrType_SysErr;
		}

		raw_local_path_preview << raw_temp_file_path.pathname();

		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview is need, generate tmp preview image now based on original file: " << raw_temp_file_path.pathname();
		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview is need, preview image path without format: " << raw_local_path_preview.str();

		//Note image format(i.e., file extension) will be assigned to raw_local_path_preview in this call
		if (VueceWinUtilities::ExtractAlbumArtFromFile(raw_local_path.pathname(), raw_local_path_preview))
		{

			final_preview_path = VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws(raw_local_path_preview.str()));

			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview is successfully created: " << final_preview_path;
		}
		else
		{
			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview cannot extract from source file, searching image file in current directory";

			talk_base::Pathname pathFirstImageFile;
			talk_base::Pathname pathFirstImageFileUtf8;

			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview cannot be extracted from the source file, try to locate the first image file in current folder.";

			//pass absolute folder path as param
			VueceWinUtilities::FindTheFirstImageFileInFolder(raw_local_path.folder(), &pathFirstImageFile);

			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - FindTheFirstImageFileInFolder returned: " << pathFirstImageFile.pathname();

			//we need to make a copy otherwise the file will be deleted as a tmp file
			//after the preview transfer is finished
			std::string utf8_str =VueceWinUtilities::utf8_encode(VueceWinUtilities::s2ws( pathFirstImageFile.pathname() ));

			pathFirstImageFileUtf8.SetPathname(utf8_str);

			if(talk_base::Filesystem::IsFile(pathFirstImageFileUtf8))
			{
				talk_base::Pathname pathFirstImageFileTmpCopy;

				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Tmp preview file path: " << raw_local_path_preview.str();

				//append file extension otherwise our resizing API won't recognize the image format
				raw_local_path_preview << pathFirstImageFileUtf8.extension();

				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Tmp preview file path after extension appended: " << raw_local_path_preview.str();

				//now make a copy
				pathFirstImageFileTmpCopy.SetPathname(raw_local_path_preview.str());

				if(talk_base::Filesystem::CopyFile(pathFirstImageFileUtf8, pathFirstImageFileTmpCopy))
				{
					LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview file successfully copied to tmp directory.";
				}
				else
				{
					LOG(LS_ERROR) << "VueceCoreClient::InitiateMusicStream - Preview file successfully CANNOT be copied to tmp directory.";

					return VueceStreamSessionInitErrType_SysErr;
				}

				final_preview_path = raw_local_path_preview.str();

				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - final_preview_path is set to: " << final_preview_path;
			}
			else
			{
				//preview is not available
				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Didn't find any image in this folder: " << raw_local_path.folder();
				final_preview_path = "";
				bPreviewNeeded = false;
			}
		}

		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Final preview image file path before resize: " << final_preview_path;

		resizedPreviewFilePath = final_preview_path;

		//If preview image exists, analyze the size and resize the image if necessary.
		if(final_preview_path.length() > 0)
		{
			talk_base::Pathname originalPreviewFilePath;

			originalPreviewFilePath.SetPathname(final_preview_path);

			LOG(INFO) << "Preview file is not empty, do resize at first.";

	    	VueceWinUtilities::ResizeImage(
	    			final_preview_path.c_str(),
	    			resizedPreviewFilePath,
	    			VueceGlobalContext::GetPreviewImgWidth(),
	    			VueceGlobalContext::GetPreviewImgHeight()
	    			);

	    	LOG(LS_VERBOSE) << "VueceWinUtilities::ResizeImage returned with resized preview file: " << resizedPreviewFilePath;

	    	LOG(LS_VERBOSE) << "VueceCoreClient::InitiateMusicStream - :Deleting no-resized original preview file " << final_preview_path;

	    	if(talk_base::Filesystem::DeleteFile(originalPreviewFilePath))
	    	{
	    		LOG(LS_VERBOSE) << "VueceCoreClient::InitiateMusicStream - :Original preview file deleted";
	    	}
	    	else
	    	{
	    		LOG(LS_ERROR) << "VueceCoreClient::InitiateMusicStream - :Failed to delete original preview file after resize";
	    	}
		}
	}

	LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Final preview image file path after resize: " << resizedPreviewFilePath;


	if(VueceGlobalContext::IsImgPreviewEnabled())
	{
		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview is enabled.";

		if(wi > 0 && hi > 0 && previewNeeded == 1)
		{
			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Size is specified and preview is needed by remote client.";

			if(final_preview_path.length() > 0)
			{
				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Actual preview image is available.";

				manifest->AddMusic(raw_pathname, size, wi, hi,
						mediaItem->BitRate(), mediaItem->SampleRate(), mediaItem->NChannels(), mediaItem->Duration());
			}
			else
			{
				LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Actual preview image is NOT available.";

				manifest->AddMusic(raw_pathname, size, 0, 0,
						mediaItem->BitRate(), mediaItem->SampleRate(), mediaItem->NChannels(), mediaItem->Duration());
			}
		}
		else
		{
			LOG(INFO) << "VueceCoreClient::InitiateMusicStream - File will be treated as normal item because no size info found.";
			manifest->AddMusic(raw_pathname, size, 0, 0,
					mediaItem->BitRate(), mediaItem->SampleRate(), mediaItem->NChannels(), mediaItem->Duration());
		}
	}
	else
	{
		LOG(INFO) << "VueceCoreClient::InitiateMusicStream - Preview is disabled.";
		manifest->AddMusic(raw_pathname, size, 0, 0,
				mediaItem->BitRate(), mediaItem->SampleRate(), mediaItem->NChannels(), mediaItem->Duration());
	}

	//The final function call that triggers the actual file transfer handshake
	// use non-utf8 to play music absItemPathUtf8
	media_stream_session_client_->CreateMediaStreamSessionAsInitiator(
			share_id,
			jid_target,		"",
			bPreviewNeeded,
			raw_local_path.pathname(),
			resizedPreviewFilePath,
			start_pos,
			manifest
			);

	return VueceStreamSessionInitErrType_OK;
}

#endif

cricket::CallOptions* VueceCoreClient::GetCurrentCallOption(void)
{
	return currentCallOption;
}

void VueceCoreClient::PrintRoster()
{
	LOG(INFO) <<  "VueceCoreClient::PrintRoster - Roster contains " <<  roster_->size() << " entries";
	RosterMap::iterator iter = roster_->begin();
	while (iter != roster_->end()) {
		LOG(INFO) <<  iter->second.jid.BareJid().Str() << ", full name: " << iter->second.full_name <<
				"  --  " << DescribeStatus(iter->second.show, iter->second.status) ;
		iter++;
	}
}

#ifdef VCARD_ENABLED
// ref: http://kb.cnblogs.com/a/862030/
void VueceCoreClient::SendVCardRequest(const std::string& to){
	LOG(INFO) << "SendVCardRequest - target: " << to;
	buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
	stanza->AddAttr(buzz::QN_TO, to);
	stanza->AddAttr(buzz::QN_ID, talk_base::CreateRandomString(16));
	stanza->AddAttr(buzz::QN_TYPE, "get");
	buzz::XmlElement* body = new buzz::XmlElement(buzz::QN_VCARD);
	stanza->AddElement(body);

	xmpp_client_->SendStanza(stanza);
	delete stanza;

}
#endif

void VueceCoreClient::SendVHubPlayRequest(const std::string& to,const std::string& type,const std::string& message, const std::string& uri)
{
	LOG(INFO) << "VueceCoreClient - SendVHubPlayRequest: Uri = " << uri << ", remote node: " << to << ", redirect call to SendVHubMessage()";

	//If this is a play request, we need to remember the target file URI in order for the
	//player to call back to trigger download for next buffer window
//
	VueceGlobalContext::SetCurrentMusicUri(uri.c_str());
	VueceGlobalContext::SetCurrentServerNodeJid(to.c_str());

	SendVHubMessage(to, type, message);
}

bool VueceCoreClient::SendVHubMessage(const std::string& to,const std::string& type, const std::string& message){

	bool bret = false;

	LOG(INFO) << "VueceCoreClient - SendVHubMessage:\n----------\n" << message << "\n-------------";

	vuece::ClientState state = (vuece::ClientState)vNativeInstance->GetClientState();

	if(state != vuece::CLIENT_STATE_ONLINE )
	{
		LOG(WARNING) << "VueceCoreClient - SendVHubMessage: Not online, message cannot be sent out.";
		return bret;
	}

	buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_IQ);
	stanza->AddAttr(buzz::QN_TO, to);
	stanza->AddAttr(buzz::QN_ID, talk_base::CreateRandomString(16));
	stanza->AddAttr(buzz::QN_TYPE, type);
	buzz::XmlElement* body = new buzz::XmlElement(buzz::QN_VHUB);

	body->AddText(message);

	stanza->AddElement(body);

	LOG(INFO) << "VueceCoreClient - SendVHubMessage: Sending stanza";

	buzz::XmppReturnStatus ret =  xmpp_client_->SendStanza(stanza);

	LOG(INFO) << "VueceCoreClient - SendVHubMessage: Sending stanza returned with status code: " << (int)ret;

	if(ret != buzz::XMPP_RETURN_OK)
	{
		LOG(LS_ERROR) << "VueceCoreClient - SendVHubMessage: sending stanza is not OK!";
		bret = false;
	}
	else
	{
		bret = true;
	}

	delete stanza;

	return bret;

}



// ref: http://xmpp.org/rfcs/rfc3921.html
void VueceCoreClient::SendSubscriptionMessage(const std::string& to, int type){
	LOG(INFO) << ("SendSubscriptionMessage");
	buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_PRESENCE);
	stanza->AddAttr(buzz::QN_TO, to);
	stanza->AddAttr(buzz::QN_ID, talk_base::CreateRandomString(16));
	if (type==1)
		stanza->AddAttr(buzz::QN_TYPE, buzz::STR_SUBSCRIBE);
	else if (type==2)
		stanza->AddAttr(buzz::QN_TYPE, buzz::STR_UNSUBSCRIBE);
	else if (type==3){
		  // if accept a sub req, Need to first add to roster, then send subscribed to the other party.
		buzz::XmlElement* iq = new buzz::XmlElement(buzz::QN_IQ);
		  iq->AddAttr(buzz::QN_TYPE, buzz::STR_SET);
		  buzz::XmlElement* query = new buzz::XmlElement(buzz::QN_ROSTER_QUERY);
		  buzz::XmlElement* item = new buzz::XmlElement(buzz::QN_ROSTER_ITEM);
		  item->AddAttr(buzz::QN_JID, to);
		  item->AddAttr(buzz::QN_NAME, to);
		  query->AddElement(item);
		  iq->AddElement(query);
		  xmpp_client_->SendStanza(iq);

		stanza->AddAttr(buzz::QN_TYPE, buzz::STR_SUBSCRIBED);
	}else if (type==4)
		stanza->AddAttr(buzz::QN_TYPE, buzz::STR_UNSUBSCRIBED);

	xmpp_client_->SendStanza(stanza);
	delete stanza;

}

void VueceCoreClient::SendSignature(const std::string& signature)
{
	LOG(INFO) << ("SendSignature");
	buzz::XmlElement * result = new buzz::XmlElement(buzz::QN_PRESENCE);
	result->AddElement(new buzz::XmlElement(buzz::QN_STATUS));
    result->AddText(signature, 1);

	xmpp_client_->SendStanza(result);
	delete result;
}

void VueceCoreClient::SendPresence(const std::string& status)
{
	//OLD CODE
	buzz::XmlElement * result = new buzz::XmlElement(buzz::QN_PRESENCE);
//	buzz::Status s;
	if (status == "away") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_AWAY);
	}
	else if (status == "xa") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_XA);
	}
	else if (status == "dnd") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_DND);
	}
	else if (status == "chat") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_CHAT);
	}
	else if (status == "online") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_ONLINE);
	}
	else if (status == "offline") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_OFFLINE);
	}
	else if (status == "none") {
		result->AddElement(new buzz::XmlElement(buzz::QN_SHOW));
		result->AddText(status, 1);
//      s.set_show(buzz::Status::SHOW_NONE);
	}
	else {
		result->AddElement(new buzz::XmlElement(buzz::QN_STATUS));
		result->AddText(status, 1);
//      s.set_status(status);
	}

	xmpp_client_->SendStanza(result);
	delete result;
}

/*
 * ref: http://xmpp.org/rfcs/rfc3921.html#presence
 * 
 * 2.2.2.1.  Show

The OPTIONAL <show/> element contains non-human-readable XML character data that specifies the particular availability status of an entity or specific resource. A presence stanza MUST NOT contain more than one <show/> element. The <show/> element MUST NOT possess any attributes. If provided, the XML character data value MUST be one of the following (additional availability types could be defined through a properly-namespaced child element of the presence stanza):

away -- The entity or resource is temporarily away.
chat -- The entity or resource is actively interested in chatting.
dnd -- The entity or resource is busy (dnd = "Do Not Disturb").
xa -- The entity or resource is away for an extended period (xa = "eXtended Away").
If no <show/> element is provided, the entity is assumed to be online and available.


 TOC 
2.2.2.2.  Status

The OPTIONAL <status/> element contains XML character data specifying a natural-language description of availability status. It is normally used in conjunction with the show element to provide a detailed description of an availability state (e.g., "In a meeting"). The <status/> element MUST NOT possess any attributes, with the exception of the 'xml:lang' attribute. Multiple instances of the <status/> element MAY be included but only if each instance possesses an 'xml:lang' attribute with a distinct language value.
 * 
 */

void VueceCoreClient::SendPresence(const std::string& status,  const std::string& signature)
{

	LOG(INFO) << ("SendPresence");

	//Following code is migrated from Vuece-QT project
	buzz::XmlElement * result = new buzz::XmlElement(buzz::QN_PRESENCE);

	if (!status.empty()){
		buzz::XmlElement* showElem = new buzz::XmlElement(buzz::QN_SHOW);
		showElem->SetBodyText(status);
		result->AddElement(showElem);
	}

	if (!signature.empty()){
		buzz::XmlElement* sigElem = new buzz::XmlElement(buzz::QN_STATUS);
		sigElem->SetBodyText(signature);
		result->AddElement(sigElem);
	}

	xmpp_client_->SendStanza(result);
	delete result;
}


void VueceCoreClient::QueryRoster()
{
	LOG(INFO) << ("VueceCoreClient:QueryRoster");
//	VueceLogger::Warn("VueceCoreClient:QueryRoster");
	roster_query_send_->Send();
}


void VueceCoreClient::InviteFriend(const std::string& name) {
	LOG(INFO) << ("VueceCoreClient:InviteFriend");
	buzz::Jid jid(name);
	VueceLogger::Warn("VueceCoreClient:InviteFriend: %s", jid.BareJid().Str().c_str());
	if (!jid.IsValid() || jid.node() == "") {
		LOG(INFO) << ("InviteFriend - Invalid JID. JIDs should be in the form user@domain\n");
		return;
	}

	if(friend_invite_send_)
	{
		// Note: for some reason the Buzz backend does not forward our presence
		// subscription requests to the end user when that user is another call
		// client as opposed to a Smurf user. Thus, in that scenario, you must
		// run the friend command as the other user too to create the linkage
		// (and you won't be notified to do so).
		friend_invite_send_->Send(jid);
		LOG(INFO) <<  "InviteFriend - Requesting to befriend  " << name;
	}
	else
	{
		LOG(WARNING) <<  "InviteFriend - friend_invite_send_ is not initialized, request will not be sent";
	}

}

void VueceCoreClient::OnFileShareProgressUpdated(const std::string& share_id, int percent)
{
	vNativeInstance->OnFileShareProgressUpdated(share_id, percent);

	//TODO - Maybe we need to use mutex to protect this?
	current_file_download_progress = percent;
}

void VueceCoreClient::OnMusicStreamingProgressUpdated(const std::string& share_id, int progress)
{
	vNativeInstance->OnMusicStreamingProgressUpdated(share_id, progress);

	//TODO - Maybe we need to use mutex to protect this?
	current_music_streaming_progress = progress;

//	VueceLogger::Debug("VueceCoreClient::OnMusicStreamingProgressUpdated: %d", current_music_streaming_progress);
}

void VueceCoreClient::OnFileSharePreviewReceived(const std::string& share_id, const std::string& file_path, int w, int h)
{
	vNativeInstance->OnFileSharePreviewReceived(share_id, file_path, w, h);

	LOG(INFO) << "OnFileSharePreviewReceived:accept request because it has preview";
	AcceptFileShare(share_id, audio_cache_location, preview_file_name);
}

void VueceCoreClient::OnFileShareStateChanged(const std::string& remote_jid,  const std::string& sid, int state)
{
	//Note - This callback is NOT used by java(player) client so it will be disabled in
	//android implementation, but it's still needed by Hub client.
	//Hub client use this callback when a 'FS_RESOURCE_RELEASED' signal is triggered to
	//send a 'resouce-released' message to mobile client.
	vNativeInstance->OnFileShareStateChanged(remote_jid, sid, state);
}

//TODO - Note this method is called only for STOPPED event, will be renamed to OnStreamPlayerStopped()
void VueceCoreClient::OnStreamPlayerStateChanged(const std::string& share_id, int state)
{
	LOG(INFO) << "OnStreamPlayerStateChanged: " << state;

	vNativeInstance->OnStreamPlayerStateChanged(share_id, state);

	current_music_streaming_state=state;

}

void VueceCoreClient::OnFileShareRequestReceived(const std::string& share_id, const buzz::Jid& target, int type, const std::string& fileName, int size, bool need_preview)
{
	if (type==cricket::FileShareManifest::T_MUSIC) { // 3
		if (!need_preview) {
			LOG(INFO) << "OnFileShareRequestReceived: accept request because it has no preview";
			AcceptFileShare(share_id, audio_cache_location, audio_file_name);
		}else{
			LOG(INFO) << "OnFileShareRequestReceived: don't accept request until preview is received";
		}
		current_audio_share_id=share_id;
	}
	vNativeInstance->OnFileShareRequestReceived(share_id, target, type, fileName, size, need_preview);
}

void VueceCoreClient::OnFoundVoicemailJid(const buzz::Jid& to, const buzz::Jid& voicemail) {
	LOG(INFO) << "OnFoundVoicemailJid:Empty impl for now";
	//  LOG(INFO) << ("Calling %s's voicemail.\n", to.Str().c_str());
	//  PlaceCall(voicemail, cricket::CallOptions());
}

void VueceCoreClient::OnVoicemailJidError(const buzz::Jid& to) {
	LOG(INFO) <<  "Unable to voicemail  " << to.Str();
}

void VueceCoreClient::Quit() {

	LOG(INFO) << ("VueceCoreClient - Quit, cancel all existing sessions.");

	//Note - When we are disconnecting, several call back methods might be triggered in callclient
	//e.g, OnXmppSocketClosed(), SendVhubMessage(), we need to handle there callbacks properly as some class instances are
	//already destroyed.
	if(media_stream_session_client_ != NULL)
	{
		media_stream_session_client_->CancelAllSessions();
	}

	//stop connection monitor at first
	if(connection_keeper != NULL){
		connection_keeper->Stop();
	}

#ifdef ANDROID
	LOG(INFO) << ("VueceCoreClient - Quit, checking streaming player.");


	//TODO - In case of quit caused by network failure, PauseInternal() is called
	//directly here, so there will be no notification fired (which is fired in
	//VueceKernelShell::PauseMusic which is called by user) unless we do it
	//explicitly here 
	vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();

	if(ns != vuece::NetworkPlayerState_Idle)
	{
		LOG(INFO) << ("VueceCoreClient - Player is not in IDLE, stop now.");

		PauseInternal();

		LOG(INFO) << ("VueceCoreClient - Quit, PauseInternal returned, firing notification");

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		FireNetworkPlayerNotification(vuece::NetworkPlayerEvent_Paused, vuece::NetworkPlayerState_Idle);

		LOG(INFO) << ("VueceCoreClient - Quit, stopping streaming player - Done");
	}
	else
	{
		LOG(INFO) << ("VueceCoreClient - Quit, streaming player is in IDLE, no need to stop");
	}

#endif

	LOG(INFO) << ("VueceCoreClient - Quit, final step - quit current thread now.");

	talk_base::Thread::Current()->Quit();
}

void VueceCoreClient::OnDevicesChange() {
	LOG(INFO) << ("Devices changed.");
	RefreshStatus();
}

RosterMap* VueceCoreClient::GetRosterMap()
{
	return roster_;
}

buzz::Jid VueceCoreClient::GetCurrentTargetJid()
{
	return currentTargetJid;
}

//---------------------------------------------------------------------
//chat and voice/video call related methods, only enabled in non-hub mode
//----------------------------------------------------------------------

#ifdef CHAT_ENABLED
void VueceCoreClient::VueceCmdSendChat()
{
	LOG(INFO) << "VueceCmdSendChat";
	std::string chatSample = "Hi this is a test!";
	//std::string to = "john@gmail.com";
	std::string to = "jack@gmail.com";
	SendChat(to, chatSample);
}

void VueceCoreClient::SendChat(const std::string& to, const std::string msg) {
	LOG(INFO) << ("SendChat");
	buzz::XmlElement* stanza = new buzz::XmlElement(buzz::QN_MESSAGE);
	stanza->AddAttr(buzz::QN_TO, to);
	stanza->AddAttr(buzz::QN_ID, talk_base::CreateRandomString(16));
	stanza->AddAttr(buzz::QN_TYPE, "chat");
	buzz::XmlElement* body = new buzz::XmlElement(buzz::QN_BODY);
	body->SetBodyText(msg);
	stanza->AddElement(body);

	xmpp_client_->SendStanza(stanza);
	delete stanza;
}
#endif



#ifdef MUC_ENABLED
void VueceCoreClient::JoinMuc(const std::string& room) {
 LOG(INFO) << ("JoinMuc");
	buzz::Jid room_jid;
	if (room.length() > 0) {
		room_jid = buzz::Jid(room);
	} else {
		// generate a GUID of the form XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX,
		// for an eventual JID of private-chat-<GUID>@groupchat.google.com
		char guid[37], guid_room[256];
		for (size_t i = 0; i < ARRAY_SIZE(guid) - 1;) {
			if (i == 8 || i == 13 || i == 18 || i == 23) {
				guid[i++] = '-';
			} else {
				sprintf(guid + i, "%04x", rand());
				i += 4;
			}
		}

		talk_base::sprintfn(guid_room, ARRAY_SIZE(guid_room), "private-chat-%s@%s", guid, pmuc_domain_.c_str());
		room_jid = buzz::Jid(guid_room);
	}

	if (!room_jid.IsValid()) {
		LOG(INFO) <<  "Unable to make valid muc endpoint for " <<  room;
		return;
	}

	MucMap::iterator elem = mucs_.find(room_jid);
	if (elem != mucs_.end()) {
		LOG(INFO) << ("This MUC already exists.");
		return;
	}

	buzz::Muc* muc = new buzz::Muc(room_jid, xmpp_client_->jid().node());
	mucs_[room_jid] = muc;
	presence_out_->SendDirected(muc->local_jid(), my_status_);
}

void VueceCoreClient::OnMucInviteReceived(const buzz::Jid& inviter, const buzz::Jid& room, const std::vector<
		buzz::AvailableMediaEntry>& avail) {

	LOG(INFO) << "VueceCoreClient::OnMucInviteReceived - Invited to join " << room.Str() << " by " << inviter.Str();
	LOG(INFO) << "VueceCoreClient::OnMucInviteReceived - Available media:\n"
	if (avail.size() > 0) {
		for (std::vector<buzz::AvailableMediaEntry>::const_iterator i = avail.begin(); i != avail.end(); ++i) {
			LOG(INFO) << "  " << buzz::AvailableMediaEntry::TypeAsString(i->type)
			<< ",   " << buzz::AvailableMediaEntry::StatusAsString(i->status)) << "\n";
		}
	} else {
		LOG(INFO) << "None\n"
	}
	// We automatically join the room.
	JoinMuc(room.Str());
}

void VueceCoreClient::OnMucJoined(const buzz::Jid& endpoint) {
	MucMap::iterator elem = mucs_.find(endpoint);
	ASSERT(elem != mucs_.end() && elem->second->state() == buzz::Muc::MUC_JOINING);

	buzz::Muc* muc = elem->second;
	muc->set_state(buzz::Muc::MUC_JOINED);

	LOG(INFO) << "VueceCoreClient::OnMucJoined - Joined: " << muc->jid().Str();
}

void VueceCoreClient::OnMucStatusUpdate(const buzz::Jid& jid,
    const buzz::MucStatus& status) {
  // Look up this muc.
  MucMap::iterator elem = mucs_.find(jid);
  ASSERT(elem != mucs_.end() &&
         elem->second->state() == buzz::Muc::MUC_JOINED);

  buzz::Muc* muc = elem->second;

  if (status.jid().IsBare() || status.jid() == muc->local_jid()) {
    // We are only interested in status about other users.
    return;
  }

  if (!status.available()) {
    // User is leaving the room.
    buzz::Muc::MemberMap::iterator elem =
      muc->members().find(status.jid().resource());

    ASSERT(elem != muc->members().end());

    // If user had src-ids, they have the left the room without explicitly
    // hanging-up; must tear down the stream if in a call to this room.
    if (call_ && session_->remote_name() == muc->jid().Str()) {
      RemoveStream(elem->second.audio_src_id(), elem->second.video_src_id());
    }

    // Remove them from the room.
    muc->members().erase(elem);
  } else {
    // Either user has joined or something changed about them.
    // Note: The [] operator here will create a new entry if it does not
    // exist, which is what we want.
    buzz::MucStatus& member_status(
        muc->members()[status.jid().resource()]);
    if (call_ && session_->remote_name() == muc->jid().Str()) {
      // We are in a call to this muc. Must potentially update our streams.
      // The following code will correctly update our streams regardless of
      // whether the SSRCs have been removed, added, or changed and regardless
      // of whether that has been done to both or just one. This relies on the
      // fact that AddStream/RemoveStream do nothing for SSRC arguments that are
      // zero.
      uint32 remove_audio_src_id = 0;
      uint32 remove_video_src_id = 0;
      uint32 add_audio_src_id = 0;
      uint32 add_video_src_id = 0;
      if (member_status.audio_src_id() != status.audio_src_id()) {
        remove_audio_src_id = member_status.audio_src_id();
        add_audio_src_id = status.audio_src_id();
      }
      if (member_status.video_src_id() != status.video_src_id()) {
        remove_video_src_id = member_status.video_src_id();
        add_video_src_id = status.video_src_id();
      }
      // Remove the old SSRCs, if any.
      RemoveStream(remove_audio_src_id, remove_video_src_id);
      // Add the new SSRCs, if any.
      AddStream(add_audio_src_id, add_video_src_id);
    }
    // Update the status. This will use the compiler-generated copy
    // constructor, which is perfectly adequate for this class.
    member_status = status;
  }
}

void VueceCoreClient::LeaveMuc(const std::string& room) {

	buzz::Jid room_jid;
	if (room.length() > 0) {
		room_jid = buzz::Jid(room);
	} else if (mucs_.size() > 0) {
		// leave the first MUC if no JID specified
		room_jid = mucs_.begin()->first;
	}

	if (!room_jid.IsValid()) {
		return;
	}

	MucMap::iterator elem = mucs_.find(room_jid);
	if (elem == mucs_.end()) {
		//    LOG(INFO) << ("No such MUC.");
		return;
	}

	buzz::Muc* muc = elem->second;
	muc->set_state(buzz::Muc::MUC_LEAVING);

	buzz::Status status;
	status.set_jid(my_status_.jid());
	status.set_available(false);
	status.set_priority(0);
	presence_out_->SendDirected(muc->local_jid(), status);

}

void VueceCoreClient::OnMucLeft(const buzz::Jid& endpoint, int error) {

	// We could be kicked from a room from any state.  We would hope this
	// happens While in the MUC_LEAVING state
	MucMap::iterator elem = mucs_.find(endpoint);
	if (elem == mucs_.end())
		return;

	buzz::Muc* muc = elem->second;
	if (muc->state() == buzz::Muc::MUC_JOINING) {
		//    LOG(INFO) << ("Failed to join \"%s\", code=%d",
		//                     muc->jid().Str().c_str(), error);
	} else if (muc->state() == buzz::Muc::MUC_JOINED) {
		//    LOG(INFO) << ("Kicked from \"%s\"",
		//                     muc->jid().Str().c_str());
	}

	delete muc;
	mucs_.erase(elem);
}

void VueceCoreClient::InviteToMuc(const std::string& user, const std::string& room) {
	// First find the room.
	const buzz::Muc* found_muc;
	if (room.length() == 0) {
		if (mucs_.size() == 0) {
			  LOG(INFO) << ("Not in a room yet; can't invite.\n");
			return;
		}
		// Invite to the first muc
		found_muc = mucs_.begin()->second;
	} else {
		MucMap::iterator elem = mucs_.find(buzz::Jid(room));
		if (elem == mucs_.end()) {
		     LOG(INFO) <<  "Not in room  " << room ;
			return;
		}
		found_muc = elem->second;
	}
	// Now find the user. We invite all of their resources.
	bool found_user = false;
	buzz::Jid user_jid(user);
	for (RosterMap::iterator iter = roster_->begin(); iter != roster_->end(); ++iter) {
		if (iter->second.jid.BareEquals(user_jid)) {
			muc_invite_send_->Send(iter->second.jid, *found_muc);
			found_user = true;
		}
	}
	if (!found_user) {
	    LOG(INFO) <<  "No such friend as  " << user ;
		return;
	}
}
#endif



