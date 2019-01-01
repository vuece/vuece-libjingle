/*
 * VueceMediaStreamSessionClient.cc
 *
 *  Created on: Feb 24, 2013
 *      Author: jingjing
 */

//WARING!!!! - DO NOT change the following header sequnece.
#include "talk/session/fileshare/VueceMediaStreamSession.h"
#include "talk/session/fileshare/VueceMediaStreamSessionClient.h"
#include "talk/session/fileshare/VueceMediaStream.h"
#include "talk/session/fileshare/VueceShareCommon.h"
#include "talk/p2p/base/constants.h"
#include "talk/base/logging.h"
#include "talk/base/stream.h"
#include "VueceLogger.h"
#include "VueceConstants.h"

#include "VueceGlobalSetting.h"
#ifdef ANDROID
#include "VueceStreamPlayer.h"
#include "VueceNetworkPlayerFsm.h"
#endif

using namespace buzz;

namespace cricket {

VueceMediaStreamSessionClient::VueceMediaStreamSessionClient(SessionManager *sm, const buzz::Jid& jid, const std::string &user_agent) :
	session_manager_(sm), jid_(jid), user_agent_(user_agent) {
	Construct();

	is_music_streaming = false;

	timeout_wait_session_released = 30;

	LOG(INFO) << "VueceMediaStreamSessionClient:Constructor called.";

	session_release_received = false;

	sessionId2VueceShareIdMap = new SessionId2VueceShareIdMap();

	session_map_  = new VueceShareId2SessionMap();

#ifdef VUECE_APP_ROLE_HUB
	return;
#else
	VueceThreadUtil::InitMutex(&mutex_wait_session_release);
	VueceStreamPlayer::Init();
	VueceNetworkPlayerFsm::Init();

	VueceNetworkPlayerFsm::RegisterUpperLayerNotificationListener(this);
#endif

}

void VueceMediaStreamSessionClient::Construct() {
	// Register ourselves
	session_manager_->AddClient(NS_GOOGLE_SHARE, this);
}

VueceMediaStreamSessionClient::~VueceMediaStreamSessionClient() {

	LOG(INFO)
		<< "VueceMediaStreamSessionClient:Destructor called.";

	session_manager_->RemoveClient(NS_GOOGLE_SHARE);

#ifdef ANDROID
	VueceStreamPlayer::UnInit();
	LOG(INFO) << "VueceMediaStreamSessionClient:Destructor called. A";
	VueceNetworkPlayerFsm::UnRegisterUpperLayerNotificationListener(this);
	LOG(INFO) << "VueceMediaStreamSessionClient:Destructor called. B";
	VueceNetworkPlayerFsm::UnInit();
	LOG(INFO) << "VueceMediaStreamSessionClient:Destructor called. C";
#endif

	if(sessionId2VueceShareIdMap != NULL)
	{
		LOG(INFO) << "VueceMediaStreamSessionClient:De-constructor - Deleting sessionId2VueceShareIdMap...";

		delete sessionId2VueceShareIdMap;
		sessionId2VueceShareIdMap = NULL;

		LOG(INFO) << "VueceMediaStreamSessionClient:De-constructor - sessionId2VueceShareIdMap deleted";
	}

	LOG(INFO) << "VueceMediaStreamSessionClient:Destructor done";
}

void VueceMediaStreamSessionClient::OnSessionCreate(cricket::Session* session, bool received_initiate) {

	LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:Content type is: " << session->content_type();
    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:is initiator: " << session->initiator();
    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:local_name: " << session->local_name();
    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:remote_name: " << session->remote_name();
    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:Session ID: " << session->id();

	if(session->content_type() != NS_GOOGLE_SHARE )
	{
		LOG(WARNING) << "VueceMediaStreamSessionClient:OnSessionCreate:Content is not for file share, return now.";
		return;
	}

	VERIFY(sessions_.insert(session).second);

	session->set_allow_local_ips(true);
    session->set_current_protocol(cricket::PROTOCOL_HYBRID);

	if (received_initiate) {

		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:received_initiate is true, create a new file share session now";
		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:user_agent: " << user_agent_;
		//we might not need this 'download_folder' any more?
//		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:download_folder: " << download_folder;

#ifdef ANDROID
		//check state at first
		vuece::NetworkPlayerState ns = VueceNetworkPlayerFsm::GetNetworkPlayerState();

		VueceStreamPlayer::LogCurrentFsmState();

		VuecePlayerFsmState s = VueceStreamPlayer::FsmState();

		if(s == VuecePlayerFsmState_Stopping )
		{
			LOG(INFO) << "VueceMediaStreamSessionClient:TRICKYDEBUG";
			LOG(INFO) << "VueceMediaStreamSessionClient:TRICKYDEBUG - terminate now";

//			session->Reject("Received in invalid state(Stopping).");
			session->TerminateWithReason("Received in invalid state(Stopping).");

			LOG(INFO) << "VueceMediaStreamSessionClient:TRICKYDEBUG";
			LOG(INFO) << "VueceMediaStreamSessionClient:TRICKYDEBUG";

			return;
		}

		//Note - TERMINATE won't work here because current state is not OFFER

		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:Create vuece media stream session now.";
		VueceStreamPlayer::SetStreamAllowed(true);
#endif
		//this is receiver
		VueceMediaStreamSession* share = new VueceMediaStreamSession(session, user_agent_, false);
		
		(*session_map_)[session->id()] = share;
		(*sessionId2VueceShareIdMap)[session->id()] = session->id();
		
//		share->SetLocalFolder(download_folder);
		share->SetShareId(session->id());
		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:share_id: " << share->GetShareId();
		
		share->SignalState.connect(this, &VueceMediaStreamSessionClient::OnSessionState);
		share->SignalNextFile.connect(this, &VueceMediaStreamSessionClient::OnUpdateProgress);
		share->SignalUpdateProgress.connect(this, &VueceMediaStreamSessionClient::OnUpdateProgress);
		share->SignalResampleImage.connect(this, &VueceMediaStreamSessionClient::OnResampleImage);
		share->SignalPreviewReceived.connect(this, &VueceMediaStreamSessionClient::OnPreviewReceived);

	}
	else
	{
		current_active_sid = session->id();

		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionCreate:I'm sender, current active sid is updated to: " << current_active_sid;
	}
}

void VueceMediaStreamSessionClient::OnSessionDestroy(cricket::Session* session) {

	LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy: session id: " << session->id();

	VERIFY(1 == sessions_.erase(session));

	//find share id at first
	std::map<std::string, std::string>::iterator it0 = sessionId2VueceShareIdMap->find(session->id());

	if (it0 == sessionId2VueceShareIdMap->end()) {
		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:Could not find this session: " << session->id();
	}

	ASSERT(it0 != sessionId2VueceShareIdMap->end());

	if (it0 != sessionId2VueceShareIdMap->end()) {
		std::string share_id = (*it0).second;

		LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:Found vuece share id:" << share_id;

		sessionId2VueceShareIdMap->erase(it0);

		std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->find(share_id);

		ASSERT(it1 != session_map_->end());

		if (it1 != session_map_->end()) {

			LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:VueceMediaStreamSession located.";

		    VueceMediaStreamSession* vSession = (*it1).second;

		    //double check the session erased has the currently active session, if not then sth is wrong
			if(vSession->GetSessionId() == current_active_sid)
			{
				LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy - Session id matched current active session.";
			}
			else
			{
#ifndef VUECE_APP_ROLE_HUB
				VueceLogger::Warn("VueceMediaStreamSessionClient:OnSessionDestroy - ////////////////////////////////////////////////////////////////////////////");
				VueceLogger::Warn("VueceMediaStreamSessionClient:OnSessionDestroy - ////////////////////////////////////////////////////////////////////////////");
				VueceLogger::Warn("VueceMediaStreamSessionClient:OnSessionDestroy - Session id doesn't match current active session id (%s), abort", current_active_sid.c_str());
				VueceLogger::Warn("VueceMediaStreamSessionClient:OnSessionDestroy - ////////////////////////////////////////////////////////////////////////////");
				VueceLogger::Warn("VueceMediaStreamSessionClient:OnSessionDestroy - ////////////////////////////////////////////////////////////////////////////");
#endif
			}

		    session_map_->erase(it1);

		    SignalFileShareStateChanged(vSession->jid().Str(), share_id, (int)cricket::FS_TERMINATED);

		    current_active_sid.clear();

		    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:current_active_sid cleared.";

		    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:Session key and share id successfully erased from map.";

		    //TODO - Dont delete vSession here, otherwise VueceMediaStreamSession::OnHttpConnectionClosed() cannot be fired
		    //I need to figured out how to properly delete the VueceMediaStreamSession instance.
		    // VueceMediaStreamSession::OnHttpConnectionClosed()  happens after VueceMediaStreamSessionClient::OnSessionDestroy
		    // so I cannot rely on OnSessionDestroy to delete VueceMediaStreamSession instance here
//		   		    LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:Delete the session now - 1.";
//					delete vSession;
//					vSession = NULL;
//					LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy:Delete the session now - 2.";
		}
	}

	LOG(INFO) << "VueceMediaStreamSessionClient:OnSessionDestroy - Done, double check session map size.";

	GetSessionNr();
}

bool VueceMediaStreamSessionClient::AllowedImageDimensions(size_t width, size_t height) {
	return (width >= kMinImageSize) && (width <= kMaxImageSize) && (height >= kMinImageSize) && (height <= kMaxImageSize);
}

VueceMediaStreamSession* VueceMediaStreamSessionClient::CreateMediaStreamSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  const std::string &sender_source_folder,
		  bool 	bPreviewNeeded,
		  const std::string &actual_file_path,
		  const std::string &actual_preview_file_path,
		  const std::string &start_pos,
		  cricket::FileShareManifest *manifest)
{
	int start_pos_i = 0;
	start_pos_i = atoi(start_pos.c_str());

	LOG(INFO) << "VueceMediaStreamSessionClient:CreateMediaStreamSessionAsInitiator - I'm sender";
	LOG(INFO) << "share_id: " << share_id;
	LOG(INFO) << "targetJid: " << targetJid.Str() ;
	LOG(INFO) << "sender_source_folder: " << sender_source_folder;
	LOG(INFO) << "actual_file_path: " << actual_file_path;
	LOG(INFO) << "actual_preview_file_path: " << actual_preview_file_path;
	LOG(INFO) << "start_pos: " << start_pos_i;



	cricket::Session* session = session_manager_->CreateSession(jid_.Str(), NS_GOOGLE_SHARE);

	VueceMediaStreamSession* share = new VueceMediaStreamSession(session, user_agent_, bPreviewNeeded);
	(*session_map_)[share_id] = share;

	//Note - Why do we need a map which maps vuece share id to session id?
	//Because SEND might be triggered by upper layer (e.g, java UI layer) which needs to
	//maintain its own session, at that point the session is not created yet so there is no
	//actual session id, we don't want to create a share session then notify upper layer the
	//actual session id, this complicates coding, so the solution is upper layer creates a vuece
	//share id, passes it to this method, when session is created, we map vuece id to the newly
	//created session id
	(*sessionId2VueceShareIdMap)[session->id()] = share_id;

	share->SetShareId(share_id);
	//local folder is not needed for sender because absolute path is used
//	share->SetLocalFolder(local_folder);
	share->SetSenderSourceFolder(sender_source_folder);
	share->SetActualFilePath(actual_file_path);
	share->SetActualPreviewFilePath(actual_preview_file_path);
	share->SetStartPosition(start_pos_i);

	share->SignalState.connect(this, &VueceMediaStreamSessionClient::OnSessionState);
	share->SignalNextFile.connect(this, &VueceMediaStreamSessionClient::OnUpdateProgress);
	share->SignalUpdateProgress.connect(this, &VueceMediaStreamSessionClient::OnUpdateProgress);
	share->SignalResampleImage.connect(this, &VueceMediaStreamSessionClient::OnResampleImage);

	LOG(INFO) << "VueceMediaStreamSessionClient:CreateMediaStreamSessionAsInitiator:share_id: " << share->GetShareId();
	LOG(INFO) << "VueceMediaStreamSessionClient:CreateMediaStreamSessionAsInitiator:actual_preview_file_path: " << actual_preview_file_path;

	share->Share(targetJid, const_cast<cricket::FileShareManifest*> (manifest));

	return share;
}

void VueceMediaStreamSessionClient::Accept(
		const std::string &share_id, const std::string & target_download_folder, const std::string &target_file_name)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::Accept:Share id: " << share_id;

	std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->find(share_id);

	ASSERT(it1 != session_map_->end());

	if (it1 != session_map_->end())
	{

		bool isMusicStreamSession = false;

		VueceMediaStreamSession *fss = (*it1).second;

		const cricket::FileShareManifest* aManifest = fss->manifest();
		const FileShareManifest::Item& item = aManifest->item(0);

		LOG(INFO) << "VueceMediaStreamSessionClient::Accept:VueceMediaStreamSession located, share_id = "
				<< fss->GetShareId() << ", session id = " << fss->GetSessionId()
				<< ", jid = " << fss->jid().Str();

		current_active_sid = fss->GetSessionId();

		VueceLogger::Debug("VueceMediaStreamSessionClient::Accept - Session id is: %s", current_active_sid.c_str());

		if ((item.type == FileShareManifest::T_MUSIC) )
		{
			isMusicStreamSession = true;
			VueceLogger::Debug("VueceMediaStreamSessionClient::Accept - This is a music streaming session");
		}
		else
		{
			VueceLogger::Debug("VueceMediaStreamSessionClient::Accept - This is normal file share session");
		}

		fss->Accept(target_download_folder, target_file_name);

//		VueceStreamPlayer::SetStreamAllowed(true);

		//Only do this when this is MUSIC streaming session
		if(isMusicStreamSession)
		{
			LOG(INFO) << "VueceMediaStreamSessionClient::Accept - This is a music streaming session, keep a copy of used config params for later resume operation.";
			fss->RetrieveUsedMusicAttributes(&used_bit_rate, &used_sample_rate, &used_nchannels, &used_duration);

			LOG(INFO) << "VueceMediaStreamSessionClient::Accept:Used music file attributes populated: used_bit_rate = " << used_bit_rate
					<< ", used_sample_rate = " << used_sample_rate << ", used_nchannels = " << used_nchannels << ", used_duration = " << used_duration;

		}

		used_target_download_folder = target_download_folder;
		used_target_file_name = target_file_name;
	}

}

void VueceMediaStreamSessionClient::ListAllSessionIDs()
{
	int idx = 0;

	//Important Note - On PC app, the key in this map record is 'share id' created by upper layer
	//it's NOT the actual session id, session id is in  another map: sessionId2VueceShareIdMap

	VueceLogger::Info("VueceMediaStreamSessionClient::ListAllSessionIDs - Start");

	if(session_map_->size() == 0)
	{
		VueceLogger::Info("VueceMediaStreamSessionClient::ListAllSessionIDs - Session map is empty.");
		return;
	}

	VueceLogger::Info("---------------------- ACTIVE SESSION TABLE START --------------------------------");

	for (std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->begin();
			it1 != session_map_->end();
			++it1)
	{
		VueceMediaStreamSession *fss = (*it1).second;
		VueceLogger::Info("%s - session[%d]: SID[%s]--->JID[%s]", __FUNCTION__, idx, fss->GetSessionId().c_str(), fss->jid().Str().c_str());
		idx++;
	}

	if(session_map_->size() >= 2)
	{
		VueceLogger::Info("VueceMediaStreamSessionClient::ListAllSessionIDs - [TRICKYDEBUG] Hub is doing concurrent streaming");
	}

	if(session_map_->size() >= 3)
	{
		VueceLogger::Info("VueceMediaStreamSessionClient::ListAllSessionIDs - [TRICKYDEBUG] Hub is doing serious stuff now.");
	}

	VueceLogger::Info("---------------------- ACTIVE SESSION TABLE END --------------------------------");
	
	VueceLogger::Info("VueceMediaStreamSessionClient::ListAllSessionIDs - Done");
}

void VueceMediaStreamSessionClient::CancelAllSessions()
{
	LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessions current session map size: " << session_map_->size();

	//TODO - Check OnSessionDestroy() callback, might have conflict !!!!!

	for (std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->begin();
			it1 != session_map_->end();
			++it1)
	{
		VueceMediaStreamSession *fss = (*it1).second;

		VueceLogger::Debug("Calling cancel method on session, share id = %s, session id = %s", fss->GetShareId().c_str(), fss->GetSessionId().c_str());

		fss->Cancel();
	}

	LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessions - Done";
}

int VueceMediaStreamSessionClient::GetSessionNr()
{
	LOG(INFO) << "VueceMediaStreamSessionClient::GetSessionNr - current session map size: " << session_map_->size();
	return session_map_->size();
}

void VueceMediaStreamSessionClient::Cancel(const std::string &session_id)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::Cancel:Session id: " << session_id;

	if(GetSessionNr() == 0)
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::Cancel - No session exists in session map, nothing to cancel, return now";
		return;
	}

	std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->find(session_id);
	ASSERT(it1 != session_map_->end());
	if (it1 != session_map_->end()) {

		LOG(INFO) << "VueceMediaStreamSessionClient:Cancel:VueceMediaStreamSession located, calling Cancel()...";
		VueceMediaStreamSession *fss = (*it1).second;
		fss->Cancel();

		LOG(INFO) << "VueceMediaStreamSessionClient:Cancel:VueceMediaStreamSession::Cancel() returned, disconnecting all signals.";
//		fss->SignalState.disconnect(this);
//		fss->SignalNextFile.disconnect(this);
//		fss->SignalUpdateProgress.disconnect(this);
//		fss->SignalResampleImage.disconnect(this);
	}
	else
	{
		VueceLogger::Fatal("VueceMediaStreamSessionClient:Cancel:Could not locate this session.");
	}
}

void VueceMediaStreamSessionClient::CancelSessionsByJid(const std::string &jid)
{
	bool session_found = FALSE;

	LOG(INFO) << "VueceMediaStreamSessionClient::CancelSessionsByJid - " << jid;

	int idx = 0;

	if(GetSessionNr() == 0)
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::CancelSessionsByJid - No session exists in session map, nothing to cancel, return now";
		return;
	}

	for (std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->begin();
			it1 != session_map_->end();
			++it1)
	{
		VueceMediaStreamSession *fss = (*it1).second;
		VueceLogger::Info("%s - Iterating session map: [%d]: SID[%s]--->JID[%s]", __FUNCTION__, idx, fss->GetSessionId().c_str(), fss->jid().Str().c_str());

		if(fss->jid().Str() == jid)
		{
			LOG(INFO) << "VueceMediaStreamSessionClient:CancelSessionsByJid - Found a matching session with the target jid, cancel it now.";

			fss->Cancel();

			LOG(INFO) << "VueceMediaStreamSessionClient:CancelSessionsByJid:VueceMediaStreamSession::Cancel() returned";

			session_found = true;
		}

		idx++;
	}

	if(session_found)
	{
		LOG(INFO) << "VueceMediaStreamSessionClient:CancelSessionsByJid - Didn't find any active session for this jid: " << jid;
	}
}

void VueceMediaStreamSessionClient::DestroySession(const std::string &sid)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::DestroySession:Session id: " << sid;

//	std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->find(sid);
//	ASSERT(it1 != session_map_->end());
//	if (it1 != session_map_->end()) {
//
//		LOG(INFO) << "VueceMediaStreamSessionClient:Cancel:VueceMediaStreamSession located, calling Cancel()...";
//		VueceMediaStreamSession *fss = (*it1).second;
//
//		LOG(INFO) << "VueceMediaStreamSessionClient:Cancel:VueceMediaStreamSession::Cancel() returned, disconnecting all signals.";
//	}
//	else
//	{
//		LOG(LERROR) << "VueceMediaStreamSessionClient:Cancel:Could not locate this session.";
//	}

}

void VueceMediaStreamSessionClient::Decline(const std::string &share_id)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::Decline:Share id: " << share_id;
	std::map<std::string, VueceMediaStreamSession *>::iterator it1 = session_map_->find(share_id);
	ASSERT(it1 != session_map_->end());
	if (it1 != session_map_->end()) {

		LOG(INFO) << "VueceMediaStreamSessionClient:Decline:VueceMediaStreamSession located.";
		VueceMediaStreamSession *fss = (*it1).second;
		fss->Decline();
	}
}


#ifdef ANDROID
void VueceMediaStreamSessionClient::CancelAllSessionsSync(void)
{
	int wait_timer = 0;

	LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - START";

	if( GetSessionNr() >1 )
	{
		VueceLogger::Fatal("FATAL - VueceMediaStreamSessionClient::CancelAllSessionsSync - Multiple sessions(%d) are not allowed, sth is wrong, abort.", GetSessionNr());
		return;
	}

	if( GetSessionNr() == 1 )
	{
		//check and stop existing active session
		LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Found active session, cancel any existing sessions.";

		VueceThreadUtil::MutexLock(&mutex_wait_session_release);
		if(session_release_received)
		{
			LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - session_release_received is currently true, will be reset to false";
			session_release_received = false;
		}
		VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

//		CancelAllSessions();
		Cancel(current_active_sid);

		LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Cancel() returned, start waiting release signal with timeout: "
				<< timeout_wait_session_released;

		VueceThreadUtil::MutexLock(&mutex_wait_session_release);
		if(session_release_received)
		{
			LOG(INFO) << "FATAL - VueceMediaStreamSessionClient::CancelAllSessionsSync - Already released, abort!";
			abort();
		}
		VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

		while(true)
		{
			LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Waiting remote session release message, current timer = " << wait_timer;

			//check flag UUU

			VueceThreadUtil::MutexLock(&mutex_wait_session_release);

			if(session_release_received)
			{
				LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Session release msg received.";
				//reset this flag
				session_release_received = false;
				VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);
				break;
			}

			VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

			VueceThreadUtil::SleepSec(1);
			wait_timer++;

			if(wait_timer >= timeout_wait_session_released)
			{
				LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Waiting remote session release message timed out, give up waiting.";
				break;
			}
		}

		LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - Waiting loop exited, continue STOP procedure.";
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - No active session found, no need to cancel, continue STOP procedure.";
	}

	session_release_received = false;

	//now destroy the session

	LOG(INFO) << "VueceMediaStreamSessionClient::CancelAllSessionsSync - DONE";
}

void VueceMediaStreamSessionClient::StopStreamPlayer(const std::string &share_id)
{
	//enable this flag to try sync release
	bool enable_sync_release = false;

	LOG(INFO) << "VueceMediaStreamSessionClient::StopStreamPlayer:Share id passed in: " << share_id << ", internally remembered session id: " << current_active_sid;

//	VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Session cancel skipped, directly stop player now.");

	if(enable_sync_release)
	{
		VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Sync method is used, calling CancelAllSessionsSync()");

		VueceStreamPlayer::SetStreamAllowed(false);

		CancelAllSessionsSync();
	}
	else
	{
		VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Sync method is not used, calling Cancel()");

		VueceStreamPlayer::SetStreamAllowed(false);

		if(GetSessionNr() >= 1)
		{
			ListAllSessionIDs();

			Cancel(current_active_sid);
		}
	}

	if(VueceNetworkPlayerFsm::GetNetworkPlayerState() != vuece::NetworkPlayerState_Idle)
	{
		VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Not in idle, stop player now.");

		//Note - If play is STOPPING, we need to sync this process, we wait until player state
		//is switched from STOPPING to STOPPED. The basic idea is that we should not continue
		//unless the player is stopped and available for next play
		VueceStreamPlayer::SetStopReason(VueceStreamPlayerStopReason_PausedByUser);

		VueceStreamPlayer::Stop(false);

		VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Stopped, sending released signal");

		SignalStreamPlayerReleased();
	}
	else
	{
		VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Player already in IDLE");
	}


	VueceLogger::Debug("VueceMediaStreamSessionClient::StopStreamPlayer - Done");

}

void VueceMediaStreamSessionClient::ResumeStreamPlayer(int resume_pos)
{
	VueceStreamPlayer::ResumeStreamPlayer(resume_pos, used_sample_rate, used_bit_rate, used_nchannels, used_duration);
}

//TODO REMOVE THIS, NOT USED.
void VueceMediaStreamSessionClient::SeekStream(int posInSec)
{
	VueceStreamPlayer::SeekStream(posInSec);
}

bool VueceMediaStreamSessionClient::IsMusicStreaming(void)
{
	return is_music_streaming;
}

void VueceMediaStreamSessionClient::OnRemoteSessionResourceReleased(const std::string& sid)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::OnRemoteSessionResourceReleased:Session id passed in: " << sid
			<< " current active session id: " << current_active_sid;
	//uuu
	//terminate current active session, maybe we use the sid to locate the actual session later
	VueceThreadUtil::MutexLock(&mutex_wait_session_release);

	if(session_release_received)
	{
		LOG(WARNING) << "WARNING - VueceMediaStreamSessionClient::OnRemoteSessionResourceReleased - Already released";
//		abort();
	}

	session_release_received = true;
	VueceThreadUtil::MutexUnlock(&mutex_wait_session_release);

}


void VueceMediaStreamSessionClient::OnStreamPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::OnStreamPlayerNotification - Msg type: " << (int)msg->notificaiont_type;
	switch(msg->notificaiont_type)
	{
//	case VueceStreamPlayerNotificationType_Event:
//	{
//		LOG(LS_VERBOSE) << "This is an event";
//		HandleStreamPlayerEvent((VueceStreamPlayerEvent)msg->value);
//		break;
//	}
	case VueceStreamPlayerNotificationType_StateChange:
	case VueceStreamPlayerNotificationType_Request:
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnStreamPlayerNotification - This is request or network player event";
		PassNetworkPlayerNotification(msg);
		break;
	}
	default:
		break;
	}
}

void VueceMediaStreamSessionClient::HandleStreamPlayerEvent(VueceStreamPlayerEvent state)
{
	LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::HandleStreamPlayerEvent";

	switch(state)
	{
	case VueceStreamPlayerEvent_Stopped:
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::HandleStreamPlayerEvent - VueceStreamPlayerEvent_Stopped";

		SignalStreamPlayerStateChanged("SID_NOT_USED", VueceStreamPlayerEvent_Stopped);

		break;
	}
	default:
		break;
	}
}

void VueceMediaStreamSessionClient::PassNetworkPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg)
{
	LOG(LS_VERBOSE) << "PassNetworkPlayerNotification - pass to next signal receiver";

	//pass to next receiver
	SignalStreamPlayerNotification(msg);
}

#endif

void VueceMediaStreamSessionClient::OnSessionState(cricket::VueceMediaStreamSession *session_, cricket::FileShareState state)
{

	LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState - SID: " << session_->GetSessionId() << " ,STATE : " << (int)state;

	std::stringstream manifest_description;

	const cricket::FileShareManifest* aManifest = session_->manifest();

	switch (state) {
	case cricket::FS_OFFER:
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState:FS_OFFER";

	    SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_OFFER);

		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState::Manifest item size: " << aManifest->size();

		//we only accept one item for now
		if(aManifest->size() != 1)
		{
			VueceLogger::Fatal("VueceMediaStreamSessionClient::OnSessionState::We only accept one file per session for now, abort!");
			//TODO: JJ - Reject request here
			return;
		}

		const FileShareManifest::Item& item = aManifest->item(0);

		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState::File name: " << item.name;

		size_t filesize;
		if (!session_->GetTotalSize(filesize)) {
			LOG(LS_WARNING) << " VueceMediaStreamSessionClient::OnSessionState(Unknown size)";
		} else {
			LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionStateTotal file size: " << filesize;
		}

		if (session_->is_sender()) {
			LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - I am sender";
		}
		else
		{
			bool bPreviewAvailable = session_->PreviewAvailable();

			LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - I am receiver, receiving file from: " << session_->jid().Str() ;

			LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - File type is: " << item.type;

			 if ((item.type == FileShareManifest::T_MUSIC) )
			 {

#ifdef VUECE_APP_ROLE_HUB

#else

				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - This is music streaming offer, checking current player state";

				current_active_sid = session_->GetSessionId();

				is_music_streaming = true;

				//this happens when downloading next buffer window is triggered but immediately after that
				//user issued a seek, which makes this downloading request invalid
				bool b1 = VueceGlobalContext::IsStopPlayStarted();
				bool b2 = VueceGlobalContext::ShouldDeclineSessionForNextBufWin();
				if(b1 || b2)
				{
					if(b1)
					{
						LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState - [TRICKYDEBUG] - We are stopping player now, any incoming session will be declined.";
					}

					if(b2)
					{
						LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState - [TRICKYDEBUG] - ShouldDeclineSessionForNextBufWin returned YES, decline now";
					}


					VueceGlobalContext::SetDeclineSessionForNextBufWin(false);

					session_->Decline();

					VueceGlobalContext::SetSessionDeclinedLocally(true);
					return;
				}

				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - Notify upper layer a music streaming request has been received";

				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - Received an music streaming session init request, "
				<< "current active session id is updated to: " << current_active_sid;

				VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Buffering);
				VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_StreamSessionStarted,
				vuece::NetworkPlayerState_Buffering);

				if(VueceStreamPlayer::StillInTheSamePlaySession())
				{
					LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - Player is waiting for next buffer window, this is a share request for the same file, no need to notify upper layer";
					LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - Previously used configuration will be used to automatically accept share request";
					LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - used_target_download_folder: " << used_target_download_folder;
					LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - used_target_file_name: " << used_target_file_name;

					LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - Automatically accept request now";

					//Automatically accept request here
					session_->Accept(used_target_download_folder, used_target_file_name);

					VueceStreamPlayer::SetStreamAllowed(true);

				}
				else
				{
					// this is a new request

					LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState - This is a new request, validate current network player state";

					vuece::NetworkPlayerState current_s = VueceNetworkPlayerFsm::GetNetworkPlayerState();

					if(current_s != vuece::NetworkPlayerState_Buffering)
					{
						VueceLogger::Warn("VueceMediaStreamSessionClient::OnSessionState - TRICKYDEBUG Received a new request but network play is not BUFFERING, decline it now.");

						session_->Decline();

						VueceGlobalContext::SetSessionDeclinedLocally(true);
					}
					else
					{
						//NOTE we only accept one file transfer for now.
						SignalFileShareRequestReceived(
								session_->GetShareId(),
								session_->jid(),
								(int)item.type,
								aManifest->item(0).name,
								(int)filesize,
								bPreviewAvailable
								);

						if(bPreviewAvailable)
						{
							LOG(INFO) << "VueceMediaStreamSessionClient -------------------------------------------------------------------------------------";
							LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState - Preview is available, request it now.";
							LOG(INFO) << "VueceMediaStreamSessionClient -------------------------------------------------------------------------------------";

							session_->RequestPreview();
						}
					}
				}
#endif
			 }
			 else if (item.type == FileShareManifest::T_FILE ||  item.type == FileShareManifest::T_IMAGE)
			 {
				 LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnSessionState - This is a normal file share request, upper layer will be notified.";

					SignalFileShareRequestReceived(
							session_->GetShareId(),
							session_->jid(),
							(int)item.type,
							aManifest->item(0).name,
							(int)filesize,
							bPreviewAvailable
							);
			 }

		}

		return;
	}

	case cricket::FS_TRANSFER:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_TRANSFER:Transfer started!";

		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_TRANSFER);
		return;
	}
	case cricket::FS_COMPLETE:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_COMPLETE";
		is_music_streaming = false;
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_COMPLETE);
		return;
	}

	case cricket::FS_LOCAL_CANCEL:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_LOCAL_CANCEL";
		is_music_streaming = false;
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_LOCAL_CANCEL);
		return;
	}
	case cricket::FS_REMOTE_CANCEL:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_REMOTE_CANCEL";
		is_music_streaming = false;
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_REMOTE_CANCEL);
		return;
	}
	case cricket::FS_FAILURE:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_FAILURE";
		is_music_streaming = false;
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_FAILURE);
		return;
	}
	case cricket::FS_TERMINATED:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_TERMINATED";
		is_music_streaming = false;
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_TERMINATED);
		return;
	}
	case cricket::FS_RESOURCE_RELEASED:
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnSessionState:FS_RESOURCE_RELEASED";
		is_music_streaming = false;
//		SignalFileShareStateChanged(session_->jid().Str(), session_->GetShareId(), (int)cricket::FS_RESOURCE_RELEASED);
		SignalFileShareStateChanged(session_->jid().Str(), session_->GetSessionId(), (int)cricket::FS_RESOURCE_RELEASED);

		return;
	}
	default:
	{
		is_music_streaming = false;
		VueceLogger::Fatal("VueceMediaStreamSessionClient::OnSessionState - Unknown state: %d", (int)state );
		return;
	}

	}
}

void VueceMediaStreamSessionClient::OnUpdateProgress(cricket::VueceMediaStreamSession *sess)
{
	//use verbose level to avoid massive output
	if(sess == NULL)
	{
		VueceLogger::Fatal("VueceMediaStreamSessionClient::OnUpdateProgress - session instance passed in is invalid, abort.");
		return;
	}

//	LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnUpdateProgress";

	std::string itemname;
	const cricket::FileShareManifest* aManifest = sess->manifest();

#ifdef VUECE_APP_ROLE_HUB
	//empty impl for now
#else
	size_t totalsize;
	size_t progress;

	const FileShareManifest::Item* currentItem = sess->GetCurrentItem();

	if(currentItem->type != FileShareManifest::T_MUSIC)
	{
		if (sess->GetTotalSize(totalsize) && sess->GetProgress(progress) && sess->GetCurrentItemName(&itemname)) {
			float percent = (float) progress / totalsize;
			int percentInt = percent * 100;

//			LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnUpdateProgress(NORMAL):progress percent: " << percentInt << "%";

			//notify UI interface
			SignalFileShareProgressUpdated(sess->GetShareId(), percentInt);
		}
	}
	else
	{
		sess->GetProgress(progress);

//		LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::OnUpdateProgress(MUSIC):progress in sec: " << progress;

		//notify UI interface
		SignalMusicStreamingProgressUpdated(sess->GetShareId(), progress);
	}
#endif
}

void VueceMediaStreamSessionClient::OnPreviewReceived(
		cricket::VueceMediaStreamSession* fss,
		const std::string& path,
		int w, int h
		)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::OnPreviewReceived:Path: " << path << ", w: " << w << ", h: " << h;
	SignalFileSharePreviewReceived(fss->GetShareId(), path, w, h);
}

void VueceMediaStreamSessionClient::OnResampleImage(
		cricket::VueceMediaStreamSession* fss,
		const std::string& path,
		int width,
		int height,
		talk_base::HttpServerTransaction *trans
		)
{
	LOG(INFO) << "VueceMediaStreamSessionClient::OnResampleImage:path: " << path;

	// The other side has requested an image preview. This is an asynchronous request. We should resize
	// the image to the requested size,and send that to ResampleComplete(). For simplicity, here, we
	// send back the original sized image. Note that because we don't recognize images in our manifest
	// this will never be called in pcp

	// Even if you don't resize images, you should implement this method and connect to the
	// SignalResampleImage signal, just to return an error.

	//Vuece Note: We actually don't need to do the resampling here because preview image was already specified by
	//upper layer, we just return ResampleComplete to the share session, share session already has the actual path
	//of the preview image
	talk_base::FileStream *s = new talk_base::FileStream();

	if (s->Open(path.c_str(), "rb", NULL))
	{
		LOG(INFO) << "VueceMediaStreamSessionClient::OnResampleImage:File opened";
		fss->ResampleComplete(s, trans, true);
	}
	else
	{
		LOG(LS_ERROR) << "VueceMediaStreamSessionClient::Error occurred when opening the file, return an error.";
		delete s;
		fss->ResampleComplete(NULL, trans, false);
	}

}

bool VueceMediaStreamSessionClient::ParseContent(SignalingProtocol protocol, const buzz::XmlElement* content_elem, const ContentDescription** content, ParseError* error) {
	LOG(INFO) << "VueceMediaStreamSessionClient:ParseContent";

	//log protocol type
	  if (protocol == PROTOCOL_GINGLE)
	  {
		LOG(INFO) << "VueceMediaStreamSessionClient:ParseContent:Protocol type: GINGLE" ;
	  }
	  else if (protocol == PROTOCOL_JINGLE)
	  {
		  LOG(INFO) << "VueceMediaStreamSessionClient:ParseContent:Protocol type: JINGLE" ;
	  }
	  else  if (protocol == PROTOCOL_JINGLE)
	  {
		  LOG(INFO) << "VueceMediaStreamSessionClient:ParseContent:Protocol type: HYBRID" ;
	  }

	  if (protocol == PROTOCOL_GINGLE) {

		  const std::string& content_type = content_elem->Name().Namespace();

		  //check content type
		  if(content_type == NS_GOOGLE_SHARE)
		  {
			  LOG(INFO) << "VueceMediaStreamSessionClient:ParseContent:Starting parsing content.";
			  return ParseFileShareContent(content_elem, content, error);
		  }
		  else
		  {
			  LOG(WARNING) << "VueceMediaStreamSessionClient:ParseContent:Protocol is not gingle, return false";
			  return BadParse("Unknown content type: " + content_type, error);
		  }
	  }
	  else{
		  //return BadParse("VueceMediaStreamSessionClient:Unsupported protocol: " + protocol, error);
		  return false;
	  }

	  return true;
}

bool VueceMediaStreamSessionClient::ParseFileShareContent(const buzz::XmlElement* content_elem,
                             const ContentDescription** content,
                             ParseError* error) {

	LOG(INFO) << "VueceMediaStreamSessionClient:ParseFileShareContent";

	FileContentDescription* share_desc = new FileContentDescription();

	bool result = CreatesFileContentDescription(content_elem, share_desc);

	if(!result)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSessionClient:ParseFileShareContent:Failed.";
		return false;
	}

    *content = share_desc;

    LOG(INFO) << "VueceMediaStreamSessionClient:ParseFileShareContent:OK.";

	return result;
}

bool VueceMediaStreamSessionClient::WriteContent(SignalingProtocol protocol, const ContentDescription* content, buzz::XmlElement** elem, WriteError* error) {

	LOG(INFO) << "VueceMediaStreamSessionClient:WriteContent";

	const FileContentDescription* fileContentDesc = static_cast<const FileContentDescription*>(content);

	*elem = TranslateSessionDescription(fileContentDesc);

	return true;
}

bool VueceMediaStreamSessionClient::CreatesFileContentDescription(const buzz::XmlElement* element, FileContentDescription* share_desc) {


	LOG(INFO) << "VueceMediaStreamSessionClient:CreatesFileContentDescription";

	if (element->Name() != QN_SHARE_DESCRIPTION)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSessionClient::CreatesFileContentDescription:Wrong element name";
		return false;
	}


	const buzz::XmlElement* manifest = element->FirstNamed(QN_SHARE_MANIFEST);
	const buzz::XmlElement* protocol = element->FirstNamed(QN_SHARE_PROTOCOL);

	if (!manifest || !protocol)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - Wrong manifest format or protocol!";
		return false;
	}


	for (const buzz::XmlElement* item = manifest->FirstElement(); item != NULL; item = item->NextElement()) {
		bool is_folder = false;

		if (item->Name() == QN_SHARE_FOLDER)
		{
			is_folder = true;
		}
		else if (item->Name() == QN_SHARE_FILE
				|| item->Name() == QN_SHARE_MUSIC)
		{
			is_folder = false;
		}
		else
		{
			continue;
		}

		std::string name;

		if (const buzz::XmlElement* el_name = item->FirstNamed(QN_SHARE_NAME))
		{
			name = el_name->BodyText();
		}

		if (name.empty())
		{
			continue;
		}

		size_t size = FileShareManifest::SIZE_UNKNOWN;
		if (item->HasAttr(QN_SIZE)) {
			size = strtoul(item->Attr(QN_SIZE).c_str(), NULL, 10);
		}

		if (is_folder)
		{
			share_desc->manifest.AddFolder(name, size);
		}
		else
		{

		    int bit_rate=0, sample_rate=0, nchannels=0, duration=0;

			 if( item->Name() == QN_SHARE_MUSIC )
			 {
				 bit_rate 		= atoi( item->Attr(QN_BITRATE).c_str());
				 sample_rate = atoi( item->Attr(QN_SAMPLERATE).c_str());
				 nchannels 		= atoi( item->Attr(QN_CHANNELS).c_str());
				 duration 		= atoi( item->Attr(QN_DURATION).c_str());

				 LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - Found music attributes:";
				 LOG(LS_VERBOSE) << "bit_rate = " << bit_rate << ", sample_rate = " << sample_rate
						 << ", nchannels = " << nchannels<< ", duration = " << duration;
			 }

			// Check if there is a valid image description for this file.
			if (const buzz::XmlElement* image = item->FirstNamed(QN_SHARE_IMAGE))
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - Found an image element";

				if (image->HasAttr(QN_WIDTH) && image->HasAttr(QN_HEIGHT))
				{
					size_t width = strtoul(image->Attr(QN_WIDTH).c_str(), NULL, 10);
					size_t height = strtoul(image->Attr(QN_HEIGHT).c_str(), NULL, 10);

					if (AllowedImageDimensions(width, height))
					{
						if( item->Name() == QN_SHARE_FILE )
						{
							LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - This is a normal file with image preview";

							share_desc->manifest.AddImage(name, size, width, height);
						}
						else if( item->Name() == QN_SHARE_MUSIC )
						{
							LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - This is a music file with image preview"
									;
							share_desc->manifest.AddMusic(name, size, width, height,
									bit_rate, sample_rate, nchannels, duration);
						}

						continue;
					}
				}
			}

			if( item->Name() == QN_SHARE_FILE )
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - This is a normal file without image preview";
				share_desc->manifest.AddFile(name, size);
			}
			else if( item->Name() == QN_SHARE_MUSIC )
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSessionClient::CreatesFileContentDescription - This is a music file without image preview";
				share_desc->manifest.AddMusic(name, size, 0, 0,
						bit_rate, sample_rate, nchannels, duration);
			}

		}
	}

	if (const buzz::XmlElement* http = protocol->FirstNamed(QN_SHARE_HTTP)) {

		share_desc->supports_http = true;

		for (const buzz::XmlElement* url = http->FirstNamed(QN_SHARE_URL); url != NULL; url = url->NextNamed(QN_SHARE_URL)) {

			if (url->Attr(buzz::QN_NAME) == kHttpSourcePath) {
				share_desc->source_path = url->BodyText();
			} else if (url->Attr(buzz::QN_NAME) == kHttpPreviewPath) {
				share_desc->preview_path = url->BodyText();
			}

		}

	}

	LOG(INFO) << "VueceMediaStreamSessionClient:CreatesFileContentDescription():OK.";

	return true;

}


//
//const cricket::SessionDescription* VueceMediaStreamSessionClient::CreateSessionDescription(const buzz::XmlElement* element) {
//	VueceMediaStreamSession::FileShareDescription* share_desc = new VueceMediaStreamSession::FileShareDescription;
//
//	LOG(INFO) << "VueceMediaStreamSessionClient:CreateSessionDescription";
//
//	if (element->Name() != QN_SHARE_DESCRIPTION)
//		return share_desc;
//
//	const buzz::XmlElement* manifest = element->FirstNamed(QN_SHARE_MANIFEST);
//	const buzz::XmlElement* protocol = element->FirstNamed(QN_SHARE_PROTOCOL);
//
//	if (!manifest || !protocol)
//		return share_desc;
//
//	for (const buzz::XmlElement* item = manifest->FirstElement(); item != NULL; item = item->NextElement()) {
//		bool is_folder = false;
//		if (item->Name() == QN_SHARE_FOLDER) {
//			is_folder = true;
//		} else if (item->Name() == QN_SHARE_FILE) {
//			is_folder = false;
//		} else {
//			continue;
//		}
//
//		std::string name;
//
//		if (const buzz::XmlElement* el_name = item->FirstNamed(QN_SHARE_NAME)) {
//			name = el_name->BodyText();
//		}
//
//		if (name.empty()) {
//			continue;
//		}
//
//		size_t size = FileShareManifest::SIZE_UNKNOWN;
//		if (item->HasAttr(QN_SIZE)) {
//			size = strtoul(item->Attr(QN_SIZE).c_str(), NULL, 10);
//		}
//
//		if (is_folder) {
//			share_desc->manifest.AddFolder(name, size);
//		} else {
//			// Check if there is a valid image description for this file.
//			if (const buzz::XmlElement* image = item->FirstNamed(QN_SHARE_IMAGE)) {
//
//				if (image->HasAttr(QN_WIDTH) && image->HasAttr(QN_HEIGHT)) {
//
//					size_t width = strtoul(image->Attr(QN_WIDTH).c_str(), NULL, 10);
//					size_t height = strtoul(image->Attr(QN_HEIGHT).c_str(), NULL, 10);
//					if (AllowedImageDimensions(width, height)) {
//						share_desc->manifest.AddImage(name, size, width, height);
//						continue;
//					}
//
//				}
//			}
//
//			share_desc->manifest.AddFile(name, size);
//		}
//
//	}
//
//	if (const buzz::XmlElement* http = protocol->FirstNamed(QN_SHARE_HTTP)) {
//
//		share_desc->supports_http = true;
//
//		for (const buzz::XmlElement* url = http->FirstNamed(QN_SHARE_URL);
//				url != NULL; url = url->NextNamed(QN_SHARE_URL)) {
//
//				if (url->Attr(buzz::QN_NAME) == kHttpSourcePath) {
//					share_desc->source_path = url->BodyText();
//				} else if (url->Attr(buzz::QN_NAME) == kHttpPreviewPath) {
//					share_desc->preview_path = url->BodyText();
//				}
//		}
//	}
//
//	return share_desc;
//}

buzz::XmlElement* VueceMediaStreamSessionClient::TranslateSessionDescription(const cricket::FileContentDescription* share_desc)
{
	char buffer[256];

	LOG(INFO) << "VueceMediaStreamSessionClient:TranslateSessionDescription";

	talk_base::scoped_ptr<buzz::XmlElement> el(new buzz::XmlElement(QN_SHARE_DESCRIPTION, true));

	const FileShareManifest& manifest = share_desc->manifest;

	el->AddElement(new buzz::XmlElement(QN_SHARE_MANIFEST));
	/*
	 * EXAMPLE
	 * [353:442] [5630] SEND >>>>>>>>>>>>>>>> : Tue Apr 15 22:32:51 2014
[353:444] [5630]    <iq to="jack@gmail.com/Talk.v1049EAAC0A0" type="set" id="26">
[353:445] [5630]      <session xmlns="http://www.google.com/session" type="initiate" id="2600075813" initiator="alice@gmail.com/vuece.pc9B3F9F89">
[353:446] [5630]        <description xmlns="http://www.google.com/session/share">
[353:446] [5630]          <manifest>
[353:447] [5630]            <file size="5407274">
[353:449] [5630]              <name>
[353:452] [5630]                IMG_0125.JPG
[353:452] [5630]              </name>
[353:453] [5630]              <image width="128" height="128"/>
[353:454] [5630]            </file>
[353:454] [5630]          </manifest>
[353:455] [5630]          <protocol>
[353:455] [5630]            <http>
[353:456] [5630]              <url name="source-path">
[353:456] [5630]                /temporary/6cf87e0e88fce56858176902d749b0e0/
[353:457] [5630]              </url>
[353:457] [5630]              <url name="preview-path">
[353:458] [5630]                /temporary/7aee7d5f84a874cab389f89773927e66/
[353:459] [5630]              </url>
[353:459] [5630]            </http>
[353:460] [5630]          </protocol>
[353:460] [5630]        </description>
[353:461] [5630]      </session>
[353:462] [5630]    </iq>
	 */

	for (size_t i = 0; i < manifest.size(); ++i) {
		const FileShareManifest::Item& item = manifest.item(i);
		buzz::QName qname;

		//add file type - folder, file(image) or music
		if (item.type == FileShareManifest::T_FOLDER) {
			qname = QN_SHARE_FOLDER;
		} else if ((item.type == FileShareManifest::T_FILE) || (item.type == FileShareManifest::T_IMAGE)) {
			qname = QN_SHARE_FILE;
		} else if (item.type == FileShareManifest::T_MUSIC) {
			qname = QN_SHARE_MUSIC;
		}
		else {
			ASSERT(false);
			continue;
		}

		//add size attribute
		el->AddElement(new buzz::XmlElement(qname), 1);

		LOG(INFO) << "VueceMediaStreamSessionClient:TranslateSessionDescription, item.size  = " << item.size;
		LOG(INFO) << "VueceMediaStreamSessionClient:TranslateSessionDescription, FileShareManifest::SIZE_UNKNOWN  = " << FileShareManifest::SIZE_UNKNOWN;

		if (item.size != FileShareManifest::SIZE_UNKNOWN) {

			memset(buffer, 0, sizeof(buffer));

			LOG(INFO) << "VueceMediaStreamSessionClient:TranslateSessionDescription, item size is not SIZE_UNKNOWN";

			LOG(LS_VERBOSE) << "Adding size attribute: " << item.size;

			talk_base::sprintfn(buffer, sizeof(buffer), "%lu", item.size);
			el->AddAttr(QN_SIZE, buffer, 2);
		}
		else
		{
			LOG(INFO) << "VueceMediaStreamSessionClient:TranslateSessionDescription, item size is using the maximum value.";
		}

		//add music attributes
		if(item.type == FileShareManifest::T_MUSIC)
		{
			LOG(LS_VERBOSE) << "Adding music attribute - bit_rate " << item.bit_rate;

			talk_base::sprintfn(buffer, sizeof(buffer), "%d", item.bit_rate);

			el->AddAttr(QN_BITRATE, buffer, 2);

			LOG(LS_VERBOSE) << "Adding music attribute - sample_rate " << item.sample_rate;

			talk_base::sprintfn(buffer, sizeof(buffer), "%d", item.sample_rate);

			el->AddAttr(QN_SAMPLERATE, buffer, 2);

			LOG(LS_VERBOSE) << "Adding music attribute - nchannels " << item.nchannels;

			talk_base::sprintfn(buffer, sizeof(buffer), "%d", item.nchannels);

			el->AddAttr(QN_CHANNELS, buffer, 2);

			LOG(LS_VERBOSE) << "Adding music attribute - duration " << item.duration;

			talk_base::sprintfn(buffer, sizeof(buffer), "%d", item.duration);

			el->AddAttr(QN_DURATION, buffer, 2);
		}


		//add file name
		buzz::XmlElement* el_name = new buzz::XmlElement(QN_SHARE_NAME);
		el_name->SetBodyText(item.name);
		el->AddElement(el_name, 2);

		//add size information if file type is image/music
		if (
				((item.type == FileShareManifest::T_IMAGE) || (item.type == FileShareManifest::T_MUSIC) )
				&& AllowedImageDimensions(item.width, item.height))
		{
			char buffer[256];

//			if(item.type == FileShareManifest::T_IMAGE)
//			{
				el->AddElement(new buzz::XmlElement(QN_SHARE_IMAGE), 2);
//			}
//			else if(item.type == FileShareManifest::T_MUSIC)
//			{
//				el->AddElement(new buzz::XmlElement(QN_SHARE_MUSIC), 2);
//			}

			talk_base::sprintfn(buffer, sizeof(buffer), "%lu", item.width);
			el->AddAttr(QN_WIDTH, buffer, 3);
			talk_base::sprintfn(buffer, sizeof(buffer), "%lu", item.height);
			el->AddAttr(QN_HEIGHT, buffer, 3);
		}

	}

	el->AddElement(new buzz::XmlElement(QN_SHARE_PROTOCOL));

	if (share_desc->supports_http) {

		el->AddElement(new buzz::XmlElement(QN_SHARE_HTTP), 1);

		if (!share_desc->source_path.empty()) {
			buzz::XmlElement* url = new buzz::XmlElement(QN_SHARE_URL);
			url->SetAttr(buzz::QN_NAME, kHttpSourcePath);
			url->SetBodyText(share_desc->source_path);
			el->AddElement(url, 2);
		}

		if (!share_desc->preview_path.empty()) {
			buzz::XmlElement* url = new buzz::XmlElement(QN_SHARE_URL);
			url->SetAttr(buzz::QN_NAME, kHttpPreviewPath);
			url->SetBodyText(share_desc->preview_path);
			el->AddElement(url, 2);
		}

	}

	return el.release();
}

}
