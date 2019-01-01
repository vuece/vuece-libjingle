/*
 * VueceFileShareSessionClient.cc
 *
 *  Created on: Dec 15, 2011
 *      Author: jingjing
 */

//WARING!!!! - DO NOT change the following header sequnece.
#include <mediastreamer2/vuecemscommon.h>
#include "talk/session/fileshare/VueceFileShareSession.h"
#include "talk/session/fileshare/VueceFileShareSessionClient.h"
#include "talk/session/fileshare/VueceMediaStream.h"
#include "talk/session/fileshare/VueceMediaStreamSession.h"
#include "talk/session/fileshare/VueceShareCommon.h"
#include "talk/p2p/base/constants.h"
#include "talk/base/logging.h"
#include "talk/base/stream.h"
#include "VueceGlobalSetting.h"
#include "VueceConstants.h"

using namespace buzz;

namespace cricket {

VueceFileShareSessionClient::VueceFileShareSessionClient(SessionManager *sm, const buzz::Jid& jid, const std::string &user_agent) :
	session_manager_(sm), jid_(jid), user_agent_(user_agent) {
	Construct();

	LOG(INFO)
		<< "VueceFileShareSessionClient:Constructor called.";

}

void VueceFileShareSessionClient::Construct() {
	// Register ourselves
	session_manager_->AddClient(NS_GOOGLE_SHARE, this);
}

VueceFileShareSessionClient::~VueceFileShareSessionClient() {
	session_manager_->RemoveClient(NS_GOOGLE_SHARE);
}

void VueceFileShareSessionClient::SetDownloadFolder(const std::string &folder)
{
	LOG(INFO) << "VueceFileShareSessionClient:SetDownloadFolder: " << folder;
	download_folder = folder;
}


void VueceFileShareSessionClient::OnSessionCreate(cricket::Session* session, bool received_initiate) {

	LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:Content type is: " << session->content_type();
    LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:is initiator: " << session->initiator();
    LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:local_name: " << session->local_name();
    LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:remote_name: " << session->remote_name();

	if(session->content_type() != NS_GOOGLE_SHARE )
	{
		LOG(WARNING) << "VueceFileShareSessionClient:OnSessionCreate:Content is not for file share, return now.";
		return;
	}

	VERIFY(sessions_.insert(session).second);

	session->set_allow_local_ips(true);
    session->set_current_protocol(cricket::PROTOCOL_HYBRID);

	if (received_initiate) {

		LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:received_initiate is true, create a new file share session now";
		LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:user_agent: " << user_agent_;
		LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:download_folder: " << download_folder;

		//this is receiver
		VueceFileShareSession* share = new VueceFileShareSession(session, user_agent_, false);
		
		session_map_[session->id()] = share;
		sid_share_id_map_[session->id()] = session->id();
		
		share->SetLocalFolder(download_folder);
		share->SetShareId(session->id());
		LOG(INFO) << "VueceFileShareSessionClient:OnSessionCreate:share_id: " << share->GetShareId();
		
		share->SignalState.connect(this, &VueceFileShareSessionClient::OnSessionState);
		share->SignalNextFile.connect(this, &VueceFileShareSessionClient::OnUpdateProgress);
		share->SignalUpdateProgress.connect(this, &VueceFileShareSessionClient::OnUpdateProgress);
		share->SignalResampleImage.connect(this, &VueceFileShareSessionClient::OnResampleImage);
		share->SignalPreviewReceived.connect(this, &VueceFileShareSessionClient::OnPreviewReceived);
		SignalFileShareSessionCreate(share);
	}
}

void VueceFileShareSessionClient::OnSessionDestroy(cricket::Session* session) {
	LOG(INFO) << "VueceFileShareSessionClient:OnSessionDestroy: session id: " << session->id();
	VERIFY(1 == sessions_.erase(session));

	//find share id at first
	std::map<std::string, std::string>::iterator it0 = sid_share_id_map_.find(session->id());
	ASSERT(it0 != sid_share_id_map_.end());

	if (it0 != sid_share_id_map_.end()) {
		std::string sid = (*it0).second;

		LOG(INFO) << "VueceFileShareSessionClient:OnSessionDestroy:Found vuece share id:" << sid;

		sid_share_id_map_.erase(it0);

		std::map<std::string, VueceFileShareSession *>::iterator it1 = session_map_.find(sid);
		ASSERT(it1 != session_map_.end());
		if (it1 != session_map_.end()) {

			LOG(INFO) << "VueceFileShareSessionClient:OnSessionDestroy:VueceFileShareSession located.";
		    session_map_.erase(it1);

		    SignalFileShareStateChanged(sid, (int)cricket::FS_TERMINATED);

		    LOG(INFO) << "VueceFileShareSessionClient:OnSessionDestroy:Session key and share id successfully erased from map.";
		}
	}
}

bool VueceFileShareSessionClient::AllowedImageDimensions(size_t width, size_t height) {
	return (width >= kMinImageSize) && (width <= kMaxImageSize) && (height >= kMinImageSize) && (height <= kMaxImageSize);
}

VueceFileShareSession* VueceFileShareSessionClient::CreateFileShareSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  const std::string &local_folder,
		  bool 	bPreviewNeeded,
		  const std::string &actual_preview_file_path,
		  const std::string &start_pos,
		  cricket::FileShareManifest *manifest)
{
	int start_pos_i = 0;
	start_pos_i = atoi(start_pos.c_str());

	LOG(INFO) << "VueceFileShareSessionClient:CreateFileShareSessionAsInitiator - I'm sender";
	LOG(INFO) << "share_id: " << share_id;
	LOG(INFO) << "targetJid: " << targetJid.Str() ;
	LOG(INFO) << "local_folder: " << local_folder;
	LOG(INFO) << "actual_preview_file_path: " << actual_preview_file_path;
	LOG(INFO) << "start_pos: " << start_pos_i;

	cricket::Session* session = session_manager_->CreateSession(jid_.Str(), NS_GOOGLE_SHARE);
	
	VueceFileShareSession* share = new VueceFileShareSession(session, user_agent_, bPreviewNeeded);
	session_map_[share_id] = share;

	//Note - Why do we need a map which maps vuece share id to session id?
	//Because SEND might be triggered by upper layer (e.g, java UI layer) which needs to
	//maintain its own session, at that point the session is created yet so there is not
	//actual session id, we don't want to create a share session then notify upper layer the
	//actual session id, this complicates coding, so the solution is upper layer creates a vuece
	//share id, passes it to this method, when session is created, we map vuece id to the newly
	//created session id
	sid_share_id_map_[session->id()] = share_id;
	
	share->SetShareId(share_id);
	share->SetLocalFolder(local_folder);
	share->SetActualPreviewFilePath(actual_preview_file_path);
	share->SetStartPosition(start_pos_i);

	share->SignalState.connect(this, &VueceFileShareSessionClient::OnSessionState);
	share->SignalNextFile.connect(this, &VueceFileShareSessionClient::OnUpdateProgress);
	share->SignalUpdateProgress.connect(this, &VueceFileShareSessionClient::OnUpdateProgress);
	share->SignalResampleImage.connect(this, &VueceFileShareSessionClient::OnResampleImage);

	LOG(INFO) << "VueceFileShareSessionClient:CreateFileShareSessionAsInitiator:share_id: " << share->GetShareId();
	LOG(INFO) << "VueceFileShareSessionClient:CreateFileShareSessionAsInitiator:actual_preview_file_path: " << actual_preview_file_path;

	share->Share(targetJid, const_cast<cricket::FileShareManifest*> (manifest));

	SignalFileShareSessionCreate(share);

	return share;
}

void VueceFileShareSessionClient::Accept(const std::string &share_id, int sample_rate)
{
	LOG(INFO) << "VueceFileShareSessionClient::Accept:Share id: " << share_id;
	std::map<std::string, VueceFileShareSession *>::iterator it1 = session_map_.find(share_id);
	ASSERT(it1 != session_map_.end());
	if (it1 != session_map_.end()) {

		LOG(INFO) << "VueceFileShareSessionClient:Accept:VueceFileShareSession located.";
		VueceFileShareSession *fss = (*it1).second;
		fss->Accept(sample_rate);

		used_sample_rate = sample_rate;
	}
}

void VueceFileShareSessionClient::Cancel(const std::string &share_id)
{
	LOG(INFO) << "VueceFileShareSessionClient::Cancel:Share id: " << share_id;
	std::map<std::string, VueceFileShareSession *>::iterator it1 = session_map_.find(share_id);
	ASSERT(it1 != session_map_.end());
	if (it1 != session_map_.end()) {

		LOG(INFO) << "VueceFileShareSessionClient:Cancel:VueceFileShareSession located.";
		VueceFileShareSession *fss = (*it1).second;
		fss->Cancel();
	}
}

void VueceFileShareSessionClient::Decline(const std::string &share_id)
{
	LOG(INFO) << "VueceFileShareSessionClient::Decline:Share id: " << share_id;
	std::map<std::string, VueceFileShareSession *>::iterator it1 = session_map_.find(share_id);
	ASSERT(it1 != session_map_.end());
	if (it1 != session_map_.end()) {

		LOG(INFO) << "VueceFileShareSessionClient:Decline:VueceFileShareSession located.";
		VueceFileShareSession *fss = (*it1).second;
		fss->Decline();
	}
}

void VueceFileShareSessionClient::OnSessionState(cricket::VueceFileShareSession *session_, cricket::FileShareState state)
{

	LOG(INFO) <<  ("VueceFileShareSessionClient::OnSessionState");

	std::stringstream manifest_description;

	const cricket::FileShareManifest* aManifest = session_->manifest();

	switch (state) {
	case cricket::FS_OFFER:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_OFFER";

		LOG(INFO) << "Manifest item size: " << aManifest->size();

		//we only accept one item for now

		if(aManifest->size() != 1)
		{
			LOG(LS_WARNING) << "We only accept one file per session for now, reject request.";
			//TODO: JJ - Reject request here
			return;
		}

		LOG(INFO) << "File name: " << aManifest->item(0).name;

		size_t filesize;
		if (!session_->GetTotalSize(filesize)) {
			LOG(LS_ERROR) << " (Unknown size)";
		} else {
			LOG(INFO) << "Total file size: " << filesize;
		}

		if (session_->is_sender()) {
			LOG(INFO) << "I am sender";
//			waiting_for_file_ = false;
//			std::cout << "Offering " << manifest_description.str() << " to " << send_to_jid_.Str() << std::endl;
//		} else if (waiting_for_file_) {
		}
		else
		{
			LOG(INFO) << "I am receiver, receiving file from: " << session_->jid().Str() ;

			//NOTE we only accept one file transfer for now.
			SignalFileShareRequestReceived(
					session_->GetShareId(),
					session_->jid(),
					aManifest->item(0).name,
					(int)filesize,
					session_->PreviewAvailable()
					);

//			session_->RequestPreview();

			//TEST CODE - REMOVE LATER
			session_->Accept(0);
		}

		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_OFFER);

		return;
	}

	case cricket::FS_TRANSFER:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_TRANSFER:Transfer started!";
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_TRANSFER);
		return;
	}
	case cricket::FS_COMPLETE:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_COMPLETE";
		//XXX notify completion
//		callClient->OnFileShareCompleted(session_->jid());
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_COMPLETE);
		return;
	}

	case cricket::FS_LOCAL_CANCEL:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_LOCAL_CANCEL";
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_LOCAL_CANCEL);
		return;
	}
	case cricket::FS_REMOTE_CANCEL:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_REMOTE_CANCEL";
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_REMOTE_CANCEL);
		return;
	}
	case cricket::FS_FAILURE:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_FAILURE";
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_FAILURE);
		return;
	}
	case cricket::FS_TERMINATED:
	{
		LOG(INFO) << "VueceFileShareSessionClient::OnSessionState:FS_TERMINATED";
		//zzzzzzzzzzzz
		SignalFileShareStateChanged(session_->GetShareId(), (int)cricket::FS_TERMINATED);
		return;
	}
	default:
	{
		LOG(LS_ERROR) << "Unknow state: " << (int)state;
		return;
	}

	}
}

void VueceFileShareSessionClient::OnUpdateProgress(cricket::VueceFileShareSession *sess)
{
	//use verbose level to avoid massive output
	LOG(LS_VERBOSE) << "VueceFileShareSessionClient::OnUpdateProgress";
	LOG(LS_VERBOSE) << "Session jid: " << sess->jid().BareJid().Str();

	size_t totalsize, progress;
	std::string itemname;
	const cricket::FileShareManifest* aManifest = sess->manifest();

#ifdef VUECE_APP_ROLE_HUB
	//empty impl for now
#else
	//NOTE - Following code is commeted because VueceGlobalContext::GetStreamingMode()
	//is not implememted (not needed for now)

//	if(VueceGlobalContext::GetStreamingMode() == VueceStreamingMode_Normal)
//	{
//		if (sess->GetTotalSize(totalsize) && sess->GetProgress(progress) && sess->GetCurrentItemName(&itemname)) {
//			float percent = (float) progress / totalsize;
//			int percentInt = percent * 100;
//
//			LOG(LS_VERBOSE) << "VueceFileShareSessionClient::OnUpdateProgress:progress percent: " << percentInt << "%";
//
//			//notify UI interface
//			SignalFileShareProgressUpdated(sess->GetShareId(), percentInt);
//		}
//	}
//	else
//	{
//		sess->GetProgress(progress);
//		LOG(LS_VERBOSE) << "VueceFileShareSessionClient::OnUpdateProgress:progress in sec: " << progress;
//		//notify UI interface
//		SignalFileShareProgressUpdated(sess->GetShareId(), progress);
//	}
#endif
}

void VueceFileShareSessionClient::OnPreviewReceived(
		cricket::VueceFileShareSession* fss,
		const std::string& path,
		int w, int h
		)
{
	LOG(INFO) << "VueceFileShareSessionClient::OnPreviewReceived:Path: " << path << ", w: " << w << ", h: " << h;
	SignalFileSharePreviewReceived(fss->GetShareId(), path, w, h);
}

void VueceFileShareSessionClient::OnResampleImage(
		cricket::VueceFileShareSession* fss,
		const std::string& path,
		int width,
		int height,
		talk_base::HttpServerTransaction *trans
		)
{
	LOG(INFO) << "VueceFileShareSessionClient::OnResampleImage:path: " << path;

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
		LOG(INFO) << "VueceFileShareSessionClient::OnResampleImage:File opened";
		fss->ResampleComplete(s, trans, true);
	}
	else
	{
		LOG(LS_ERROR) << "VueceFileShareSessionClient::Error occurred when opening the file, return an error.";
		delete s;
		fss->ResampleComplete(NULL, trans, false);
	}

}

bool VueceFileShareSessionClient::ParseContent(SignalingProtocol protocol, const buzz::XmlElement* content_elem, const ContentDescription** content, ParseError* error) {
	LOG(INFO) << "VueceFileShareSessionClient:ParseContent";

	//log protocol type
	  if (protocol == PROTOCOL_GINGLE)
	  {
		LOG(INFO) << "VueceFileShareSessionClient:ParseContent:Protocol type: GINGLE" ;
	  }
	  else if (protocol == PROTOCOL_JINGLE)
	  {
		  LOG(INFO) << "VueceFileShareSessionClient:ParseContent:Protocol type: JINGLE" ;
	  }
	  else  if (protocol == PROTOCOL_JINGLE)
	  {
		  LOG(INFO) << "VueceFileShareSessionClient:ParseContent:Protocol type: HYBRID" ;
	  }

	  if (protocol == PROTOCOL_GINGLE) {

		  const std::string& content_type = content_elem->Name().Namespace();

		  //check content type
		  if(content_type == NS_GOOGLE_SHARE)
		  {
			  LOG(INFO) << "VueceFileShareSessionClient:ParseContent:Starting parsing content.";
			  return ParseFileShareContent(content_elem, content, error);
		  }
		  else
		  {
			  LOG(WARNING) << "VueceFileShareSessionClient:ParseContent:Protocol is not gingle, return false";
			  return BadParse("Unknown content type: " + content_type, error);
		  }
	  }
	  else{
		  //return BadParse("VueceFileShareSessionClient:Unsupported protocol: " + protocol, error);
		  return false;
	  }

	  return true;
}

bool VueceFileShareSessionClient::ParseFileShareContent(const buzz::XmlElement* content_elem,
                             const ContentDescription** content,
                             ParseError* error) {

	LOG(INFO) << "VueceFileShareSessionClient:ParseFileShareContent";

	FileContentDescription* share_desc = new FileContentDescription();

	bool result = CreatesFileContentDescription(content_elem, share_desc);

	if(!result)
	{
		LOG(LS_ERROR) << "VueceFileShareSessionClient:ParseFileShareContent:Failed.";
		return false;
	}

    *content = share_desc;

    LOG(INFO) << "VueceFileShareSessionClient:ParseFileShareContent:OK.";

	return result;
}

bool VueceFileShareSessionClient::WriteContent(SignalingProtocol protocol, const ContentDescription* content, buzz::XmlElement** elem, WriteError* error) {

	LOG(INFO) << "VueceFileShareSessionClient:WriteContent";

	const FileContentDescription* fileContentDesc = static_cast<const FileContentDescription*>(content);

	*elem = TranslateSessionDescription(fileContentDesc);

	return true;;
}

buzz::XmlElement* CreateGingleFileShareContentElem()
{
	return 0;
}

bool VueceFileShareSessionClient::CreatesFileContentDescription(const buzz::XmlElement* element, FileContentDescription* share_desc) {


	LOG(INFO) << "VueceFileShareSessionClient:CreatesFileContentDescription";

	if (element->Name() != QN_SHARE_DESCRIPTION)
	{
		LOG(LS_ERROR) << "CreatesFileContentDescription:Wrong element name";
		return false;
	}


	const buzz::XmlElement* manifest = element->FirstNamed(QN_SHARE_MANIFEST);
	const buzz::XmlElement* protocol = element->FirstNamed(QN_SHARE_PROTOCOL);

	if (!manifest || !protocol)
	{
		LOG(LS_ERROR) << "CreatesFileContentDescription:Wrong manifest format or protocol!";
		return false;
	}


	for (const buzz::XmlElement* item = manifest->FirstElement(); item != NULL; item = item->NextElement()) {
		bool is_folder;
		if (item->Name() == QN_SHARE_FOLDER) {
			is_folder = true;
		} else if (item->Name() == QN_SHARE_FILE) {
			is_folder = false;
		} else {
			continue;
		}
		std::string name;
		if (const buzz::XmlElement* el_name = item->FirstNamed(QN_SHARE_NAME)) {
			name = el_name->BodyText();
		}
		if (name.empty()) {
			continue;
		}
		size_t size = FileShareManifest::SIZE_UNKNOWN;
		if (item->HasAttr(QN_SIZE)) {
			size = strtoul(item->Attr(QN_SIZE).c_str(), NULL, 10);
		}
		if (is_folder) {
			share_desc->manifest.AddFolder(name, size);
		} else {
			// Check if there is a valid image description for this file.
			if (const buzz::XmlElement* image = item->FirstNamed(QN_SHARE_IMAGE)) {
				if (image->HasAttr(QN_WIDTH) && image->HasAttr(QN_HEIGHT)) {
					size_t width = strtoul(image->Attr(QN_WIDTH).c_str(), NULL, 10);
					size_t height = strtoul(image->Attr(QN_HEIGHT).c_str(), NULL, 10);
					if (AllowedImageDimensions(width, height)) {
						share_desc->manifest.AddImage(name, size, width, height);
						continue;
					}
				}
			}
			share_desc->manifest.AddFile(name, size);
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

	LOG(INFO) << "VueceFileShareSessionClient:CreatesFileContentDescription():OK.";

	return true;

}


const cricket::SessionDescription* VueceFileShareSessionClient::CreateSessionDescription(const buzz::XmlElement* element) {
	VueceFileShareSession::FileShareDescription* share_desc = new VueceFileShareSession::FileShareDescription;

	LOG(INFO) << "VueceFileShareSessionClient:CreateSessionDescription";

	if (element->Name() != QN_SHARE_DESCRIPTION)
		return share_desc;

	const buzz::XmlElement* manifest = element->FirstNamed(QN_SHARE_MANIFEST);
	const buzz::XmlElement* protocol = element->FirstNamed(QN_SHARE_PROTOCOL);

	if (!manifest || !protocol)
		return share_desc;

	for (const buzz::XmlElement* item = manifest->FirstElement(); item != NULL; item = item->NextElement()) {
		bool is_folder;
		if (item->Name() == QN_SHARE_FOLDER) {
			is_folder = true;
		} else if (item->Name() == QN_SHARE_FILE) {
			is_folder = false;
		} else {
			continue;
		}
		std::string name;
		if (const buzz::XmlElement* el_name = item->FirstNamed(QN_SHARE_NAME)) {
			name = el_name->BodyText();
		}
		if (name.empty()) {
			continue;
		}
		size_t size = FileShareManifest::SIZE_UNKNOWN;
		if (item->HasAttr(QN_SIZE)) {
			size = strtoul(item->Attr(QN_SIZE).c_str(), NULL, 10);
		}
		if (is_folder) {
			share_desc->manifest.AddFolder(name, size);
		} else {
			// Check if there is a valid image description for this file.
			if (const buzz::XmlElement* image = item->FirstNamed(QN_SHARE_IMAGE)) {
				if (image->HasAttr(QN_WIDTH) && image->HasAttr(QN_HEIGHT)) {
					size_t width = strtoul(image->Attr(QN_WIDTH).c_str(), NULL, 10);
					size_t height = strtoul(image->Attr(QN_HEIGHT).c_str(), NULL, 10);
					if (AllowedImageDimensions(width, height)) {
						share_desc->manifest.AddImage(name, size, width, height);
						continue;
					}
				}
			}
			share_desc->manifest.AddFile(name, size);
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

	return share_desc;
}

buzz::XmlElement* VueceFileShareSessionClient::TranslateSessionDescription(const cricket::FileContentDescription* share_desc) {

	LOG(INFO) << "VueceFileShareSessionClient:TranslateSessionDescription";

	talk_base::scoped_ptr<buzz::XmlElement> el(new buzz::XmlElement(QN_SHARE_DESCRIPTION, true));

	const FileShareManifest& manifest = share_desc->manifest;
	el->AddElement(new buzz::XmlElement(QN_SHARE_MANIFEST));
	for (size_t i = 0; i < manifest.size(); ++i) {
		const FileShareManifest::Item& item = manifest.item(i);
		buzz::QName qname;
		if (item.type == FileShareManifest::T_FOLDER) {
			qname = QN_SHARE_FOLDER;
		} else if ((item.type == FileShareManifest::T_FILE) || (item.type == FileShareManifest::T_IMAGE)) {
			qname = QN_SHARE_FILE;
		} else {
			ASSERT(false);
			continue;
		}
		el->AddElement(new buzz::XmlElement(qname), 1);
		if (item.size != FileShareManifest::SIZE_UNKNOWN) {
			char buffer[256];
			talk_base::sprintfn(buffer, sizeof(buffer), "%lu", item.size);
			el->AddAttr(QN_SIZE, buffer, 2);
		}
		buzz::XmlElement* el_name = new buzz::XmlElement(QN_SHARE_NAME);
		el_name->SetBodyText(item.name);
		el->AddElement(el_name, 2);
		if ((item.type == FileShareManifest::T_IMAGE) && AllowedImageDimensions(item.width, item.height)) {
			el->AddElement(new buzz::XmlElement(QN_SHARE_IMAGE), 2);
			char buffer[256];
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
void VueceFileShareSessionClient::CancelAllSessions()
{
	LOG(WARNING) << "VueceFileShareSessionClient:CancelAllSessions - should not be called";
}

void VueceFileShareSessionClient::StopStreamPlayer(const std::string &share_id)
{
	LOG(WARNING) << "VueceFileShareSessionClient:StopStreamPlayer - should not be called";
}

void VueceFileShareSessionClient::ResumeStreamPlayer(const std::string &share_id)
{
	LOG(WARNING) << "VueceFileShareSessionClient:ResumeStreamPlayer - should not be called";
}

void VueceFileShareSessionClient::SeekStream(int posInSec)
{
	LOG(WARNING) << "VueceFileShareSessionClient:SeekStream - should not be called";
}

VueceMediaStreamSession* VueceFileShareSessionClient::CreateMediaStreamSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  bool 	bPreviewNeeded,
		  const std::string &actual_file_path,
		  const std::string &actual_preview_file_path,
		  const std::string &start_pos,
		  cricket::FileShareManifest *manifest)
{
	LOG(WARNING) << "VueceFileShareSessionClient:CreateMediaStreamSessionAsInitiator - should not be called";
	return NULL;
}

}
