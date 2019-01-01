/*
 * VueceMediaStreamSession.cc
 *
 *  Created on: Feb 24, 2013
 *      Author: Jingjing Sun
 */

#include "talk/session/fileshare/VueceMediaStreamSession.h"

#include "talk/base/httpcommon-inl.h"
#include "talk/base/base64.h"
#include "talk/base/fileutils.h"
#include "talk/base/streamutils.h"
#include "talk/base/event.h"
#include "talk/base/helpers.h"
#include "talk/base/httpclient.h"
#include "talk/base/httpserver.h"
#include "talk/base/httpcommon.h"
#include "talk/base/pathutils.h"
#include "talk/base/socketstream.h"
#include "talk/base/stringdigest.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"
#include "talk/base/tarstream.h"
#include "talk/base/thread.h"
#include "talk/session/tunnel/pseudotcpchannel.h"
#include "talk/session/tunnel/tunnelsessionclient.h"
#include "talk/base/scoped_ptr.h"

#include "talk/xmpp/constants.h"

#include "talk/session/fileshare/VueceFileShareSessionClient.h"
#include "talk/session/fileshare/VueceMediaStream.h"

#include "VueceGlobalSetting.h"
#include "VueceConstants.h"
#include "VueceLogger.h"

#ifndef VUECE_APP_ROLE_HUB
#include "VueceStreamPlayer.h"
#include "VueceNetworkPlayerFsm.h"
#endif


#define VUECE_PREVIEW_FILE_NAME "preview.jpg"

using namespace buzz;

//refer to http://xmpp.org/extensions/inbox/jingle-httpft.html for protocol explanation

///////////////////////////////////////////////////////////////////////////////
// <description xmlns="http://www.google.com/session/share">
//   <manifest>
//     <file size='341'>
//       <name>foo.txt</name>
//     </file>
//     <file size='51321'>
//       <name>foo.jpg</name>
//       <image width='480' height='320'/>
//     </file>
//     <folder>
//       <name>stuff</name>
//     </folder>
//   </manifest>
//   <protocol>
//     <http>
//       <url name='source-path'>/temporary/23A53F01/</url>
//       <url name='preview-path'>/temporary/90266EA1/</url>
//     </http>
//     <raw/>
//   </protocol>
// </description>
// <p:transport xmns:p="p2p"/>
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Constants and private functions
///////////////////////////////////////////////////////////////////////////////

namespace {

// Wait 10 seconds to see if any new proxies get established
const uint32 kProxyWait = 10000;

const int MSG_RETRY = 1;
const uint32 kFileTransferEnableRetryMs = 1000 * 60 * 4; // 4 minutes

const std::string MIME_OCTET_STREAM("application/octet-stream");

enum {
	MSG_PROXY_WAIT,
};

} // anon namespace

namespace cricket {

///////////////////////////////////////////////////////////////////////////////
// VueceMediaStreamSession
///////////////////////////////////////////////////////////////////////////////

VueceMediaStreamSession::VueceMediaStreamSession(cricket::Session* session, const std::string &user_agent, bool bPreviewNeeded_) :
	session_(session), state_(FS_NONE), is_closed_(false), is_sender_(false), manifest_(NULL),
	pool_(this),
	http_client_(NULL),
	http_server_(NULL),
	transfer_connection_id_(talk_base::HTTP_INVALID_CONNECTION_ID),
	counter_(NULL),
	item_transferring_(0),
	bytes_transferred_(0),
	local_cancel_(false),
	local_listener_(NULL),
	remote_listener_(NULL),
	next_channel_id_(1),
	start_pos(0),
	user_agent_(user_agent)
{
	LOG(INFO) << "VueceMediaStreamSession:Constructor called.";

	//remember session because when this session is destroy, we will not be able to retrive session
	//by calling the same method
	session_id = session_->id();

	LOG(INFO) << "VueceMediaStreamSession:Constructor - Remember session id for later use: " << session_id;

	session_->SignalState.connect(this, &VueceMediaStreamSession::OnSessionState);
	session_->SignalInfoMessage.connect(this, &VueceMediaStreamSession::OnSessionInfoMessage);
	session_->SignalChannelGone.connect(this, &VueceMediaStreamSession::OnSessionChannelGone);

	pCurrentPreviewFileStream = NULL;

	bInPreviewMode = false;
	bCancelled = false;

	current_file_type = FileShareManifest::T_NONE;

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Constructor called, current_file_type is set to 4 (NONE) by default.";

	currentPreviewFile = NULL;

	LOG(INFO) << "VueceMediaStreamSession:Constructor called, pVueceMediaStream  init to null";

	pVueceMediaStream = NULL;

	//default values for audio decoding and playing, they are only used by player
	//these values will be updated by calling Accept()
	sample_rate = 44100;
	bit_rate = 128;
	nchannels = 2;

	bPreviewNeeded = bPreviewNeeded_;

	preview_path_ = "";
}

void VueceMediaStreamSession::OnMediaStreamingStopped(int code)
{
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnMediaStreamingStopped";
	ReleaseAllResourses();
}


void VueceMediaStreamSession::ReleaseAllResourses()
{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:ReleaseAllResourses";

		pVueceMediaStream = NULL;

		SignalState(this, FS_RESOURCE_RELEASED);

		LOG(LS_VERBOSE) << "******************************* ASYNC TERMINATION END **************************************";
}

VueceMediaStreamSession::~VueceMediaStreamSession() {

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Destructor - call ReleaseAllResourses() now";

//	ReleaseAllResourses();

	ASSERT(FS_NONE != state_);

	// If we haven't closed, do cleanup now.
	if (!IsClosed()) {
		if (!IsComplete()) {
			state_ = FS_FAILURE;
		}
		DoClose(true);
	}

	if (session_) {

		LOG(INFO) << "VueceMediaStreamSession:Destructor, disconnecting session signals";

		// Make sure we don't get future state changes on this session.
		session_->SignalState.disconnect(this);
		session_->SignalInfoMessage.disconnect(this);
		session_->SignalChannelGone.disconnect(this);
		session_ = NULL;
	}

	for (TransactionList::const_iterator trans_it = transactions_.begin(); trans_it != transactions_.end(); ++trans_it) {
		(*trans_it)->response()->set_error(talk_base::HC_NOT_FOUND);
				http_server_->Respond(*trans_it);
	}

	if(http_client_ != NULL)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:Destructor, deleting http_client_";
		delete http_client_;
	}

	if(http_server_ != NULL)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:Destructor, deleting http_server_";
		delete http_server_;
	}

	if(local_listener_ != NULL)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:Destructor, deleting local_listener_";
		delete local_listener_;
	}

	if(remote_listener_ != NULL)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:Destructor, deleting remote_listener_";
		delete remote_listener_;
	}

	pVueceMediaStream = NULL;

	LOG(INFO) << "VueceMediaStreamSession:Destructorfinished";

}

bool VueceMediaStreamSession::IsComplete() const {

	LOG(INFO) << "VueceMediaStreamSession::IsComplete - Current state_ = " << (int)state_;

	return (state_ >= FS_COMPLETE);
}

bool VueceMediaStreamSession::IsClosed() const {
	return is_closed_;
}

FileShareState VueceMediaStreamSession::state() const {
	return state_;
}

bool VueceMediaStreamSession::is_sender() const {
	ASSERT(FS_NONE != state_);
	return is_sender_;
}

const buzz::Jid&
VueceMediaStreamSession::jid() const {
	ASSERT(FS_NONE != state_);
	return jid_;
}

const FileShareManifest*
VueceMediaStreamSession::manifest() const {
	ASSERT(FS_NONE != state_);
	return manifest_;
}

//const std::string&
//VueceMediaStreamSession::local_folder() const {
//	ASSERT(!local_folder_.empty());
//	return local_folder_;
//}

void VueceMediaStreamSession::Share(const buzz::Jid& jid, FileShareManifest* manifest) {
	LOG(INFO) << "VueceMediaStreamSession:Share";
	ASSERT(FS_NONE == state_);
	ASSERT(NULL != session_);

	LOG(INFO) << "VueceMediaStreamSession:Share - using single thread solution";

	http_server_ = new talk_base::HttpServer;
	http_server_->SignalHttpRequest.connect(this, &VueceMediaStreamSession::OnHttpRequest);
	http_server_->SignalHttpRequestComplete.connect(this, &VueceMediaStreamSession::OnHttpRequestComplete);
	http_server_->SignalConnectionClosed.connect(this, &VueceMediaStreamSession::OnHttpConnectionClosed);

	SessionDescription* offer = CreateOffer(manifest);

	LOG(INFO) << "Going to initiate session with jid: " << jid.Str();
	session_->Initiate(jid.Str(), offer);

	delete manifest;
}

SessionDescription* VueceMediaStreamSession::CreateAnswer( const FileShareManifest* manifest )
{
	// Dummy impl for now, we only request one file

	LOG(INFO) << "VueceMediaStreamSession::CreateAnswer";

	SessionDescription* answer = new SessionDescription();

	if(manifest->empty())
	{
		LOG(WARNING) << "VueceMediaStreamSession::CreateAnswer:Manifest is empty!";
		return answer;
	}

	FileContentDescription* fd = new FileContentDescription();

	FileShareManifest::Item item = manifest->item(0);

	if(item.type == FileShareManifest::T_FILE ||  item.type == FileShareManifest::T_IMAGE)
	{
		LOG(INFO) << "VueceMediaStreamSession::CreateAnswer - Adding file";
		fd->manifest.AddFile(item.name, item.size);
	}
	else if(item.type == FileShareManifest::T_MUSIC)
	{
		LOG(INFO) << "VueceMediaStreamSession::CreateAnswer - Adding music";

		fd->manifest.AddMusic(item.name, item.size, item.width, item.height,
				item.bit_rate, item.sample_rate, item.nchannels, item.duration);
	}
	else
	{
		LOG(LERROR) << "VueceMediaStreamSession::CreateAnswer - Unsupported file type: " << item.type;
		ASSERT(false);
	}

	fd->supports_http = true;

	const SessionDescription* remoteOffer = session_->remote_description();

	const ContentInfo* ci = remoteOffer->FirstContentByType(NS_GOOGLE_SHARE);

	LOG(INFO) << "VueceMediaStreamSession::CreateAnswer:content name: " << ci->name << ", type: " << ci->type;

	const ContentDescription* contentDesc = ci->description;

	const FileContentDescription* fcd =  static_cast<const FileContentDescription*> (contentDesc);

 	fd->source_path = fcd->source_path;

 	LOG(INFO) << "VueceMediaStreamSession::CreateAnswer:Requested soure path: " << fd->source_path;

 	answer->AddContent(CN_OTHER, NS_GOOGLE_SHARE, fd);
 	return answer;
}


SessionDescription* VueceMediaStreamSession::CreateOffer( FileShareManifest* manifest )
{
	int i = 0;
	SessionDescription* offer = new SessionDescription();

	LOG(INFO) << "VueceMediaStreamSession::CreateOffer";

	if(manifest->empty())
	{
		LOG(WARNING) << "VueceMediaStreamSession::CreateOffer:Manifest is empty!";
		return offer;
	}

	LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Start parsing manifest, size = " << manifest->size();

	FileContentDescription* fd = new FileContentDescription();

	for(i = 0; i < manifest->size(); i++)
	{
		LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Parsing item: " << i;

		FileShareManifest::Item item = manifest->item(i);

		bool is_folder;
		switch(item.type)
		{
		case FileShareManifest::T_FOLDER:
		{
			LOG(INFO) << "VueceMediaStreamSession::CreateOffer:This item is a folder.";
			is_folder = true;
			break;
		}
		case FileShareManifest::T_FILE:
		{
			LOG(INFO) << "VueceMediaStreamSession::CreateOffer:This item is a file.";
			is_folder = false;
			break;
		}
		case FileShareManifest::T_IMAGE:
		{
			LOG(INFO) << "VueceMediaStreamSession::CreateOffer:This item is an image.";
			is_folder = false;
			break;
		}
		case FileShareManifest::T_MUSIC:
		{
			LOG(INFO) << "VueceMediaStreamSession::CreateOffer:This item is a music file, current_file_type is updated to T_MUSIC";

			//NOTE - current_file_type is also updated in VueceMediaStreamSession:OnHttpRequest(), however,
			// VueceMediaStreamSession:OnHttpRequest() is not always called because remote client may cancel current streaming session
			// very quickly(i.e., user keep pressing backward or forward buttong very quickly, in this case, terminal arrives at first and OnHttpRequest()
			// doesn't have a chance to be called. so we MUST update current_file_type in CreateOffer;

			current_file_type = FileShareManifest::T_MUSIC;

			is_folder = false;
			break;
		}
		default:
		{
			LOG(LS_ERROR) << "VueceMediaStreamSession::CreateOffer:This file type cannot be recognized";
			continue;
		}
		}

		std::string name = item.name;
		LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Item name: " << name << ", size: " << item.size;

		if(is_folder)
		{
			fd->manifest.AddFolder(name, item.size);
		}
		else
		{
			if (item.type == FileShareManifest::T_IMAGE)
			{
				fd->manifest.AddImage(name, item.size, item.width, item.height);
			}
			else if (item.type == FileShareManifest::T_MUSIC)
			{
				fd->manifest.AddMusic(name, item.size, item.width, item.height,
						item.bit_rate, item.sample_rate, item.nchannels, item.duration);
			}
			else
			{
				fd->manifest.AddFile(name, item.size);
			}
		}

		LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Item added.";
	}

	LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Finished parsing manifest";

	fd->supports_http = true;

	GenerateTemporaryPrefix(&fd->source_path);

	if(bPreviewNeeded)
	{
		LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Preview is needed, generate preview path";
		GenerateTemporaryPrefix(&fd->preview_path);
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Preview is not available.";
		fd->preview_path = "";
	}

	LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Source path generated: " << fd->source_path;
	LOG(INFO) << "VueceMediaStreamSession::CreateOffer:Preview path generated: " << fd->preview_path;

	offer->AddContent(CN_OTHER, NS_GOOGLE_SHARE, fd);
	return offer;
}

void VueceMediaStreamSession::Accept(const std::string & target_download_folder_, const std::string &target_file_name_)
{

	LOG(INFO) << "VueceMediaStreamSession::Accept";

	ASSERT(FS_OFFER == state_);
	ASSERT(NULL != session_);
	ASSERT(NULL != manifest_);
	ASSERT(item_transferring_ == 0);
	
	sample_rate = 0;
	bit_rate = 0;
	nchannels = 0;

	receiver_target_download_folder = target_download_folder_;
	receiver_target_download_file_name = target_file_name_;

	LOG(INFO) << "VueceMediaStreamSession::Accept - Audio decoding and playing configurations updated";
	
	LOG(INFO) << "VueceMediaStreamSession::Accept - receiver_target_download_folder: " << receiver_target_download_folder;
	LOG(INFO) << "VueceMediaStreamSession::Accept - receiver_target_download_file_name: " << receiver_target_download_file_name;

	LOG(INFO) << "VueceMediaStreamSession::Accept - Send transport-accept message at first.";

	SendTransportAccept();

	LOG(INFO) << "VueceMediaStreamSession::Accept - Creating HTTP client";

//    http_client_->SignalHttpClientClosed.connect(this, &VueceMediaStreamSession::OnHttpClientClosed);

	// The receiver now has a need for the http_server_, when previewing already
	// downloaded content.
	http_server_ = new talk_base::HttpServer;
	http_server_->SignalHttpRequest.connect(this, &VueceMediaStreamSession::OnHttpRequest);
	http_server_->SignalHttpRequestComplete.connect(this, &VueceMediaStreamSession::OnHttpRequestComplete);
	http_server_->SignalConnectionClosed.connect(this, &VueceMediaStreamSession::OnHttpConnectionClosed);

	SessionDescription* answer = CreateAnswer(manifest_);

	session_->Accept(answer);

	LOG(INFO) << "VueceMediaStreamSession:Accept - Set session state to FS_TRANSFER: iFirstFramePosSec = " << VueceGlobalContext::GetFirstFramePositionSec();

	SetState(FS_TRANSFER, false);
	
#ifndef VUECE_APP_ROLE_HUB

	VueceStreamPlayer::LogCurrentStreamingParams();

	if(VueceStreamPlayer::StillInTheSamePlaySession())
	{
		LOG(INFO) << "VueceMediaStreamSession:Accept - Player is waiting for next buffer window, don't reset last available chunk file id: "
				<< VueceGlobalContext::GetLastAvailableAudioChunkFileIdx();
	}
	else
	{
		VueceLogger::Debug("TROUBLESHOOTING 5 - Not in the same session, Global V = %d, reset", VueceGlobalContext::GetLastAvailableAudioChunkFileIdx());

		VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(0);

	}
#endif

	LOG(INFO) << "VueceMediaStreamSession:Accept - Call NextDownload() now.";

	NextDownload();
}

bool VueceMediaStreamSession::PreviewAvailable()
{
	LOG(INFO) << "VueceMediaStreamSession:PreviewAvailable: preview_path_ = [" << preview_path_ << "]";

	if(preview_path_.compare("") != 0)
	{
		LOG(INFO) << "VueceMediaStreamSession:PreviewAvailable: Yes";
		return true;
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSession:PreviewAvailable: No";
		return false;
	}
}

//IMPORTANT NOTE - RequestPreview() is called before user accepts the share request
void VueceMediaStreamSession::RequestPreview()
{
	LOG(INFO) << "VueceMediaStreamSession:RequestPreview: preview_path_: " << preview_path_;

	//TODO - Need to check if the preview path exists even if this is a image, don't send preview
	//request if preview path doesn't exist
	if(preview_path_ == "")
	{
		LOG(INFO) << "VueceMediaStreamSession:RequestPreview: Preview path is not available, return now.";
		return;
	}

	const FileShareManifest::Item& item = manifest_->item(0);

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::In streaming mode, request preview now";

	talk_base::FileStream* file = new talk_base::FileStream;
	talk_base::StreamInterface* stream = NULL;
	talk_base::Pathname temp_name;
	std::string previewf (VUECE_PREVIEW_FILE_NAME);

	temp_name.SetFilename(previewf);

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:RequestPreview - File type is: " << item.type;

	if(item.type == FileShareManifest::T_IMAGE)
	{
			if (!talk_base::CreateUniqueFile(temp_name, true))
			{
				LOG(LS_ERROR) << "NextDownload:Cannot create unique file, return now.";
				//		SetState(FS_FAILURE, false);
				return;
			}
	}
	else if(item.type == FileShareManifest::T_MUSIC)
	{
		temp_name.SetFolder(VUECE_MEDIA_AUDIO_BUFFER_LOCATION);
	}

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Opening temp file: " << temp_name.pathname();

	if (!file->Open(temp_name.pathname().c_str(), "wb", NULL)) {

		LOG(LS_ERROR) << "NextDownload:Cannot open tmp file, return now";

		delete file;
		talk_base::Filesystem::DeleteFile(temp_name);
//		SetState(FS_FAILURE, false);
		return;
	}

	bInPreviewMode = true;

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Tmp file successfully opened.";
	stream = file;

	ASSERT(NULL != stream);
	transfer_path_ = temp_name.pathname();

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::RequestPreview::transfer_path_: " << transfer_path_;

	std::string remote_path;
	char query[64];

	GetItemNetworkPath(item_transferring_, true, &remote_path);

	int imgPreviewWidth = VueceGlobalContext::GetPreviewImgWidth();
	int imgPreviewHeight = VueceGlobalContext::GetPreviewImgHeight();

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Preset imgPreviewWidth: " << imgPreviewWidth
	 << ", imgPreviewHeight: " << imgPreviewHeight;

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Original w: " << item.width  << ", h: " << item.height;

	memset(query, 0, sizeof(query));

	if(imgPreviewWidth < item.width && imgPreviewHeight < item.height)
	{
		int scaledH = imgPreviewWidth * item.height / item.width;

		LOG(INFO) << "VueceMediaStreamSession:RequestPreview:: Size needs to be scaled.";

		iCurrentPreviewW = imgPreviewWidth;
		iCurrentPreviewH = scaledH;
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Use original size.";

		iCurrentPreviewW = item.width;
		iCurrentPreviewH = item.height;
	}

	sprintf(query, "?width=%d&height=%d", iCurrentPreviewW, iCurrentPreviewH);

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::http query: " << query;

	remote_path = remote_path.append(query);

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::remote_path(preview): " << remote_path;

	//we don't need stream counter for preview file transfer because it's a underground process
	StreamCounter* counter = new StreamCounter(stream);
	counter_ = counter;

	LOG(INFO) << "VueceMediaStreamSession:RequestPreview::Starting HTTP client with host name: " << jid_.Str();

	http_client_->reset();

	http_client_->set_server(talk_base::SocketAddress(jid_.Str(), 0, false));
	http_client_->request().verb = talk_base::HV_GET;
	http_client_->request().path = remote_path;
	http_client_->response().document.reset(counter);
	http_client_->start();

}

void VueceMediaStreamSession::Decline() {
	LOG(INFO) << "VueceMediaStreamSession:Decline";
	ASSERT(FS_OFFER == state_);
	ASSERT(NULL != session_);
	local_cancel_ = true;
	session_->Reject("Vuece streaming session invalid");
}

void VueceMediaStreamSession::Cancel() {

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Cancel";

//	ASSERT(!IsComplete());

	if ( IsComplete() )
	{
		VueceLogger::Error("VueceMediaStreamSession::Cancel - Already completed, do nothing");
		return;
	}

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Cancel 1";

//	ASSERT(NULL != session_);

	if ( session_ == NULL )
	{
		VueceLogger::Fatal("VueceMediaStreamSession::Cancel - session_ is NULL. abort!");
		return;
	}

	local_cancel_ = true;
	bCancelled = true;

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Cancel - Disconnect counter_ signal";

//	counter_->SignalUpdateByteCount.disconnect_all();

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Cancel 2, calling session_->Terminate()";

	session_->Terminate();

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:Cancel - session_->Terminate() called, now start waiting for remote session termination confirmation.";

	//TODO Sync terminate here.
}

const std::string& VueceMediaStreamSession::GetSessionId() const
{
	//	if(session_ == NULL)
	//	{
	//		VueceLogger::Fatal("VueceMediaStreamSession::GetSessionId - Session instance is null, abort now.");
	//		return "ERROR";
	//	}
	//
	//	return session_->id();

//	LOG(LS_VERBOSE) << "VueceMediaStreamSession::GetSessionId - Returning: " << session_id;

	return session_id;
}

bool VueceMediaStreamSession::GetItemUrl(size_t index, std::string* url) {
	return GetItemBaseUrl(index, false, url);
}

bool VueceMediaStreamSession::GetImagePreviewUrl(size_t index, size_t width, size_t height, std::string* url) {
	if (!GetItemBaseUrl(index, true, url))
		return false;

	if (FileShareManifest::T_IMAGE != manifest_->item(index).type) {
		ASSERT(false);
		return false;
	}

	char query[256];
	talk_base::sprintfn(query, ARRAY_SIZE(query), "?width=%u&height=%u", width, height);
	url->append(query);
	return true;
}

void VueceMediaStreamSession::ResampleComplete(talk_base::StreamInterface *i, talk_base::HttpServerTransaction *trans, bool success) {
	LOG(INFO) << "VueceMediaStreamSession:ResampleComplete";
	bool found = false;
	for (TransactionList::const_iterator trans_it = transactions_.begin(); trans_it != transactions_.end(); ++trans_it) {
		if (*trans_it == trans) {
			found = true;
			break;
		}
	}

	if (!found)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSession::ResampleComplete:Could not find matching transaction.";
		return;
	}

	transactions_.remove(trans);

	if (success)
	{
		LOG(INFO) << "VueceMediaStreamSession:ResampleComplete:Return successful response with file stream.";

		pCurrentPreviewFileStream = i;

		trans->response()->set_success(MIME_OCTET_STREAM, i);
		http_server_->Respond(trans);
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSession:ResampleComplete:Return error response.";
		trans->response()->set_error(talk_base::HC_NOT_FOUND);

		http_server_->Respond(trans);
	}

}

bool VueceMediaStreamSession::GetProgress(size_t& bytes) const {
	bool known = true;
	bytes = bytes_transferred_;

	if (counter_)
	{

		const FileShareManifest::Item* item = &manifest_->item(item_transferring_);

		if(item == NULL)
		{
			LOG(LS_ERROR) << "VueceMediaStreamSession::GetProgress - Current item is null.";
			return false;
		}

		if(item->type == FileShareManifest::T_FILE)
		{
			//get normal progress in non-streaming mode
			size_t current_size = manifest_->item(item_transferring_).size;
			size_t current_pos = counter_->GetByteCount();

			if (current_size == FileShareManifest::SIZE_UNKNOWN)
			{
				known = false;
			}
			else if (current_pos > current_size)
			{
				// Don't allow the size of a 'known' item to be reported as larger than
				// it claimed to be.
				ASSERT(false);
				current_pos = current_size;
			}

			bytes += current_pos;
		}
		else
		{
			//return time value here
			if(pVueceMediaStream == NULL)
			{
				VueceLogger::Fatal("VueceMediaStreamSession::GetProgress: pVueceMediaStream should NOT be NULL!");
				return false;
			}

			//note the output is a time value (in seconds)
			pVueceMediaStream->GetTimePositionInSecond(&bytes);
		}

	}

	return known;
}

bool VueceMediaStreamSession::GetTotalSize(size_t& bytes) const {
	//TODO - If we are in hub stream mode, we should return total duration
	bool known = true;
	bytes = 0;

	//Note the total size is the sum of each file size for file transfer
	for (size_t i = 0; i < manifest_->size(); ++i) {
		if (manifest_->item(i).size == FileShareManifest::SIZE_UNKNOWN) {
			// We make files of unknown length worth a single byte.
			known = false;
			bytes += 1;
		} else {
			bytes += manifest_->item(i).size;
		}
	}

	return known;
}

bool VueceMediaStreamSession::GetCurrentItemName(std::string* name) {

//	LOG(LS_VERBOSE) << "VueceMediaStreamSession::GetCurrentItemName";

	if (FS_TRANSFER != state_) {
		VueceLogger::Error("VueceMediaStreamSession::GetCurrentItemName - Current state is not FS_TRANSFER.");
		name->clear();
		return false;
	}

	ASSERT(item_transferring_ < manifest_->size());

	if (transfer_name_.empty()) {
		const FileShareManifest::Item& item = manifest_->item(item_transferring_);
		*name = item.name;
	} else {
		*name = transfer_name_;
	}

	return !name->empty();
}

const FileShareManifest::Item* VueceMediaStreamSession::GetCurrentItem()
{
//	LOG(LS_VERBOSE) << "VueceMediaStreamSession::GetCurrentItem - item_transferring_ = " << item_transferring_;

	if (FS_TRANSFER != state_) {
		LOG(LS_VERBOSE) << "VueceMediaStreamSession::GetCurrentItem - Current state is not FS_TRANSFER.";
//		return NULL;
	}

	ASSERT(item_transferring_ < manifest_->size());

	return &manifest_->item(item_transferring_);
}


// StreamPool Implementation

talk_base::StreamInterface* VueceMediaStreamSession::RequestConnectedStream(const talk_base::SocketAddress& remote, int* err) {
	LOG(INFO) << "VueceMediaStreamSession:RequestConnectedStream";
	LOG(INFO) << "VueceMediaStreamSession:RequestConnectedStream:Remote IP: " << remote.IPAsString();

	ASSERT(remote.IPAsString() == jid_.Str());
	ASSERT(!IsClosed());
	ASSERT(NULL != session_);
	if (!session_) {
		if (err)
			*err = -1;
		return NULL;
	}

	char channel_name[64];
	talk_base::sprintfn(channel_name, ARRAY_SIZE(channel_name), "private-%u", next_channel_id_++);
	if (err)
		*err = 0;
	return CreateChannel(channel_name);
}

void VueceMediaStreamSession::ReturnConnectedStream(talk_base::StreamInterface* stream) {
	LOG(INFO) << "VueceMediaStreamSession:ReturnConnectedStream 1";
	talk_base::Thread::Current()->Dispose(stream);
	LOG(INFO) << "VueceMediaStreamSession:ReturnConnectedStream 2";
}

// MessageHandler Implementation
void VueceMediaStreamSession::OnMessage(talk_base::Message* msg) {
	LOG(INFO) << "VueceMediaStreamSession:OnMessage";
	if (MSG_PROXY_WAIT == msg->message_id) {
		LOG_F(LS_INFO)
			<< "MSG_PROXY_WAIT";
		if (proxies_.empty() && IsComplete() && !IsClosed()) {
			DoClose(true);
		}
	}
}

static void LogSessionErrorString(int errCode)
{
	switch(errCode)
	{
	case BaseSession::ERROR_NONE:
		LOG(LS_VERBOSE) << "LogSessionErrorString - No error";
		break;
	case BaseSession::ERROR_TIME:
		LOG(LS_ERROR) << "LogSessionErrorString - no response to signaling";
			break;
	case BaseSession::ERROR_RESPONSE:
		LOG(LS_ERROR) << "LogSessionErrorString - error during signaling";
			break;
	case BaseSession::ERROR_NETWORK:
		LOG(LS_ERROR) << "LogSessionErrorString - network error, could not allocate network resources";
			break;
	default:
		LOG(LS_ERROR) << "LogSessionErrorString - Unknown error code: " << errCode;
		break;
	}
}

static void LogSessionStateString(int state)
{
	switch (state)
	{
	case cricket::Session::STATE_INIT:
		LOG(LS_VERBOSE) << "Session state  - INIT";
		break;
	case cricket::Session::STATE_SENTINITIATE:
		LOG(LS_VERBOSE) << "Session state  - SENTINITIATE";
		break;
	case cricket::Session::STATE_RECEIVEDINITIATE:
		LOG(LS_VERBOSE) << "Session state  - RECEIVEDINITIATE";
		break;
	case cricket::Session::STATE_SENTACCEPT:
		LOG(LS_VERBOSE)
				<< "Session state  - SENTACCEPT";
		break;
	case cricket::Session::STATE_RECEIVEDACCEPT:
		LOG(LS_VERBOSE)
				<< "Session state  - RECEIVEDACCEPT";
		break;
	case cricket::Session::STATE_SENTMODIFY:
		LOG(LS_VERBOSE)
				<< "Session state  - SENTMODIFY";
		break;
	case cricket::Session::STATE_RECEIVEDMODIFY:
		LOG(LS_VERBOSE)
				<< "Session state  - RECEIVEDMODIFY";
		break;
	case cricket::Session::STATE_SENTREJECT:
		LOG(LS_VERBOSE)
				<< "Session state  - SENTREJECT";
		break;
	case cricket::Session::STATE_RECEIVEDREJECT:
		LOG(LS_VERBOSE)
				<< "Session state  - RECEIVEDREJECT";
		break;
	case cricket::Session::STATE_SENTREDIRECT:
		LOG(LS_VERBOSE)
				<< "Session state  - SENTREDIRECT";
		break;
	case cricket::Session::STATE_SENTTERMINATE:
		LOG(LS_VERBOSE)
				<< "Session state  - SENTTERMINATE";
		break;
	case cricket::Session::STATE_RECEIVEDTERMINATE:
		LOG(LS_VERBOSE)
				<< "Session state  - RECEIVEDTERMINATE";
		break;
	case cricket::Session::STATE_INPROGRESS:
		LOG(LS_VERBOSE)
				<< "Session state  - INPROGRESS";
		break;
	case cricket::Session::STATE_DEINIT:
		LOG(LS_VERBOSE)
				<< "Session state  - DEINIT";
		break;
	default:
		LOG(LS_ERROR) << "Session state  - Unknown state code: "
				<< state;
		break;
	}
}

// Session Signals
void VueceMediaStreamSession::OnSessionState(BaseSession* session, BaseSession::State state) {

	bool has_err = FALSE;

	VueceLogger::Debug("VueceMediaStreamSession:OnSessionState");

	VueceLogger::Info("VueceMediaStreamSession:OnSessionState - id = %s, value = %d, text = %s",
			session_->id().c_str(), (int)state, GetBaseSessionStateString(state).c_str());

	BaseSession::Error session_err = session->error();
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState - Checking if there is any error in base session, code = " << (int)session_err;

	LogSessionStateString((int)state);
	LogSessionErrorString(session_err);

	if(session_err != BaseSession::ERROR_NONE)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSession:OnSessionState - Session will be terminated because of an error, we need to handle it, cancel session locally";
		//TODO - Need to find a way to let stream session instance know there is an error when it's released.
		local_cancel_ = true;
		has_err = true;
	}

	// Once we are complete, state changes are meaningless.
	if (!IsComplete()) {
		switch (state) {
		case cricket::Session::STATE_SENTINITIATE:
		case cricket::Session::STATE_RECEIVEDINITIATE:
			OnInitiate();
			break;
		case cricket::Session::STATE_SENTACCEPT:
		case cricket::Session::STATE_RECEIVEDACCEPT:
		case cricket::Session::STATE_INPROGRESS:
			SetState(FS_TRANSFER, false);
			break;
		case cricket::Session::STATE_SENTREJECT:
		case cricket::Session::STATE_SENTTERMINATE:
			//Note matter this is a local or remote cancel, we stop player if there is a session error

			//check if this is cause by an error, if not, we can continue to play the data, otherwise we terminate the session and don't play any data
			if(has_err)
			{
				//TODO Stop player if it's already initiated/started, also destroy player instance and notify upper layer this data download error
#ifndef VUECE_APP_ROLE_HUB

				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Stop player now because session ended with error";

				/**
				 * Note - When network exception occurs, player might be in BUFFERING state, in normal case, operation is not allowed in BUFFERING state,
				 * but if in case of network error, we need to allow state machine to go back to IDLE otherwise it will blocks other player operations forever.
				 */
				VueceStreamPlayer::SetStopReason(VueceStreamPlayerStopReason_NetworkErr);
				VueceStreamPlayer::Stop(true);
#endif
			}

			if (local_cancel_)
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Update state to FS_LOCAL_CANCEL and prevent session close";
//				SetState(FS_LOCAL_CANCEL, false);
				if(has_err)
				{
					LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Has error, session will be forced to close.";

					SetState(FS_LOCAL_CANCEL, false);
				}
				else
				{
					SetState(FS_LOCAL_CANCEL, true);
				}

			}
			else
			{

				LOG(LS_VERBOSE) << "******************************* ASYNC TERMINATION START **************************************";
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Update state to FS_REMOTE_CANCEL and allow sesion close";

				SetState(FS_REMOTE_CANCEL, false);
			}

			break;
		case cricket::Session::STATE_DEINIT:
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Current state is STATE_DEINIT";

			if (local_cancel_)
			{
				//bWaitingForRemoteTerminationConfirmation
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:This is a local cancel, waiting for the actual session termination";
			}
			else
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:This is a remote cancel";
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnSessionState:Update state to FS_TERMINATED";

				SetState(FS_TERMINATED, false);
			}

			break;
		}
		case cricket::Session::STATE_RECEIVEDTERMINATE:

			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:STATE_RECEIVEDTERMINATE";

			if (is_sender()) {
				// If we are the sender, and the receiver downloaded the correct number
				// of bytes, then we assume the transfer was successful.  We've
				// introduced support for explicit completion notification
				// (QN_SHARE_COMPLETE), but it's not mandatory at this point, so we need
				// this as a fallback.
				size_t total_bytes;
				GetTotalSize(total_bytes);
				if (bytes_transferred_ >= total_bytes) {
					LOG(INFO) << "VueceMediaStreamSession:OnSessionState:STATE_RECEIVEDTERMINATE - Update state to FS_COMPLETE";
					SetState(FS_COMPLETE, false);
					break;
				}
			}
			// Fall through
			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Fall through to STATE_RECEIVEDREJECT";
		case cricket::Session::STATE_RECEIVEDREJECT:
			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:STATE_RECEIVEDREJECT";
			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Update state to FS_REMOTE_CANCEL";
			SetState(FS_REMOTE_CANCEL, false);
			break;
		case cricket::Session::STATE_INIT:
		case cricket::Session::STATE_SENTMODIFY:
		case cricket::Session::STATE_RECEIVEDMODIFY:
		case cricket::Session::STATE_SENTREDIRECT:
		default:
			// These states should not occur.
			ASSERT(false);
			break;
		}
	}
	else
	{
		LOG(INFO) << "VueceMediaStreamSession:OnSessionState:IsComplete() returned true, already completed.";
	}

	//Note - STATE_DEINIT doesn't mean the session is really released. The final release point is in OnMediaStreamingStopped()
	if (state == cricket::Session::STATE_DEINIT) {

		LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Target state is STATE_DEINIT, close session now.";

		if (!IsClosed()) {

			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Session is open, close it now.";

			DoClose(false);
		}
		else
		{
			LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Target state is STATE_DEINIT, already closed.";
		}

//		session_ = NULL;
	}

	//If error has occurred, when execution has reached here, player should be stopped, we force current state to IDLE, so
	//such error condition won't block user operations


	if(has_err)
	{

#ifdef ANDROID
		LOG(INFO) << "VueceMediaStreamSession:OnSessionState:Target state is STATE_DEINIT, already closed.";

		VueceNetworkPlayerFsm::SetNetworkPlayerState(vuece::NetworkPlayerState_Idle);
		VueceNetworkPlayerFsm::FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent_NetworkErr, vuece::NetworkPlayerState_Idle);
#endif

	}
}

void VueceMediaStreamSession::OnSessionInfoMessage(cricket::Session* session, const XmlElements& els) {
	LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage";

	if(is_sender())
	{
		LOG(INFO) << "I'm sender.";
	}
	else
	{
		LOG(INFO) << "I'm receiver.";
	}

	if (IsClosed())
		return;
	ASSERT(NULL != session_);
	for (size_t i = 0; i < els.size(); ++i) {

		LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - Element ns = " << els[i]->Name().Namespace() <<
				", localpart = " << els[i]->Name().LocalPart();

		if (is_sender() && (els[i]->Name() == QN_SHARE_CHANNEL)) {
			if (els[i]->HasAttr(buzz::QN_NAME)) {

					cricket::PseudoTcpChannel* channel = new cricket::PseudoTcpChannel(talk_base::Thread::Current(), session_);
					VERIFY(channel->Connect(CN_OTHER, els[i]->Attr(buzz::QN_NAME)));
					talk_base::StreamInterface* stream = channel->GetStream();
					http_server_->HandleConnection(stream);

			}
		} else if (is_sender() && (els[i]->Name() == QN_SHARE_COMPLETE)) {
			LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - This is sender, received completion from receiver.";

			const FileShareManifest::Item* item = GetCurrentItem();

			if(item->type == FileShareManifest::T_MUSIC)
			{
//				LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - I am sender, complete current session now(MUSIC).";
				//We should receive a TERMINATE message soon, no need to send terminate now.
				LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - I am sender, received remote complete message, waiting for remote terminate.";
				SetState(FS_COMPLETE, true);
			}
			else
			{
				// Normal file transfer has completed, but receiver may still be getting
				// previews.
				if (!IsComplete()) {
					LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - This is sender, not completed yet(FILE)";
					SetState(FS_COMPLETE, true);
				}
			}
		} else {
			//other cases added by VUECE

			if (!is_sender() && (els[i]->Name() == QN_SHARE_COMPLETE)) {
				LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - This is receiver, received completion from sender.";

				if(VueceGlobalContext::GetAppRole() == VueceAppRole_Media_Hub_Client)
				{
					LOG(INFO) << "VueceMediaStreamSession:OnSessionInfoMessage - This is a hub client, update state to COMPLETE now.";

					SetState(FS_COMPLETE, true);
				}
				else
				{
					LOG(LS_ERROR) << "Sth is wrong!!This case is only accepted when receive is a hub client!";

				}

				return;
			}

			LOG(LS_WARNING)
				<< "Unknown VueceMediaStreamSession info message: " << els[i]->Name().Merged();
		}
	}
}

void VueceMediaStreamSession::OnSessionChannelGone(cricket::Session* session, const std::string& name) {

	LOG(INFO) << "VueceMediaStreamSession:OnSessionChannelGone";
	//JJ- Need to figure out the "name" is content name or channel name?
	LOG_F(LS_WARNING) << "VueceMediaStreamSession::OnSessionChannelGone:Name: " << name;
	ASSERT(session == session_);
	if (cricket::TransportChannel* channel = session->GetChannel(NS_GOOGLE_SHARE, name)) {
		session->DestroyChannel(NS_GOOGLE_SHARE, name);
	}
}

// HttpClient Signals

void VueceMediaStreamSession::OnHttpClientComplete(talk_base::HttpClient* http, int err) {
	LOG(INFO) << "\n VueceMediaStreamSession-----------------------------------------";
	LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete - HTTP client name: " << http->MyName();
	LOG(INFO) << "\n VueceMediaStreamSession-----------------------------------------\n";
	LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete - (Error code: " << err << ", Response code: " << http->response().scode << ")";

	LogHttpErrorString(err);

	if(http != http_client_)
	{
		VueceLogger::Fatal("VueceMediaStreamSession::OnHttpClientComplete - http != http_client_");
	}

	if(http == http_client_)
	{
		LOG(INFO) << "This event is from http_client_";
	}

//	LOG(INFO) << " VueceMediaStreamSession::OnHttpClientComplete - D 1";

	if(session_ == NULL)
	{
		VueceLogger::Fatal("VueceMediaStreamSession::OnHttpClientComplete - session_ is null, abort!");
	}

//	LOG(INFO) << " VueceMediaStreamSession::OnHttpClientComplete - D 2";

	transfer_name_.clear();

//	LOG(INFO) << " VueceMediaStreamSession::OnHttpClientComplete - D 3";

	counter_ = NULL; // counter_ is deleted by HttpClient

	//NOTE!!! - This reset() call will destroy stream instance
	//uuuuuuu
	http->response().document.reset();

//	LOG(INFO) << " VueceMediaStreamSession::OnHttpClientComplete - D 4";

//	bool success = (err == 0) && (http->response().scode == talk_base::HC_OK);

	bool success = (http->response().scode == talk_base::HC_OK);

	LOG(INFO) << " VueceMediaStreamSession::OnHttpClientComplete - D 5, item_transferring_ = " << item_transferring_;

	if(success)
	{
		LOG(INFO) << "Http request succeeded.";
	}
	else
	{
		//failure handling
		LOG(INFO) << "VueceMediaStreamSession::OnHttpClientComplete - Http request failed, delete tmp file now: "  << transfer_path_;

		talk_base::Pathname p (transfer_path_);

		if( talk_base::Filesystem::DeleteFile(p) )
		{
			LOG(INFO) << "Tmp file deleted.";
		}
		else
		{
			LOG(LS_WARNING) << "Cannot delete this tmp file";
		}

//		currentTmpFilePath->clear();

		if(bInPreviewMode)
		{
			LOG(INFO) << "VueceMediaStreamSession::OnHttpClientComplete - Preview request failed or canceled, no further processing is needed, return now.";

			bInPreviewMode = false;

		}
		else
		{

			LOG(INFO) << "VueceMediaStreamSession::OnHttpClientComplete - Preview request failed or canceled, in non-preview mode, terminate session now.";

			if (!IsComplete())
			{
				LOG(INFO) << "VueceMediaStreamSession::OnHttpClientComplete - Session not completed, set state to FS_FAILURE and return";

				//Note this will
				SetState(FS_FAILURE, false);
			}
		}

		return;
	}

	const FileShareManifest::Item& item = manifest_->item(item_transferring_);
	talk_base::Pathname local_name;
//	local_name.SetFilename(item.name);
	local_name.SetFilename(receiver_target_download_file_name);
	local_name.SetFolder(receiver_target_download_folder);

	talk_base::Pathname temp_name(transfer_path_);

	LOG(LS_VERBOSE) << " VueceMediaStreamSession::OnHttpClientComplete - D 6";

	if(bInPreviewMode)
	{
		LOG(LS_VERBOSE) << "\n--------------------------------------------------------------------------------------------------------------------------------------------";
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpClientComplete:In preview mode now, preview request finished";
		LOG(LS_VERBOSE) << "--------------------------------------------------------------------------------------------------------------------------------------------\n";
	}

	//compare speicfied target local file path with current downloaded file path,
	//if not the same, move it to target location

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpClientComplete:Local file path: " << local_name.pathname();
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpClientComplete:transfer_path_: " << transfer_path_;

	if (local_name.pathname() != transfer_path_)
	{
		const bool is_folder = (item.type == FileShareManifest::T_FOLDER);

		LOG(LS_VERBOSE) << "VueceMediaStreamSession::OnHttpClientComplete - "
				<< "The specified local target file path is not the same as current  transfer_path_, "
				<< "download file will be moved to target location.";

		LOG(LS_VERBOSE) << "VueceMediaStreamSession::OnHttpClientComplete - Create file in target location.";

		if (success && !talk_base::CreateUniqueFile(local_name, false)) {
			LOG(LS_ERROR) << "Couldn't rename downloaded file: " << local_name.pathname();
			success = false;
		}
		else
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession::OnHttpClientComplete - Target file created.";
		}


		if (is_folder) {
			// The folder we want is a subdirectory of the transfer_path_.
			temp_name.AppendFolder(item.name);
		}

		LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete:Temp file path: " << temp_name.pathname();

		//Note: We don't need to move the file if it's in preview mode
		if(!bInPreviewMode)
		{
		   if(item.type == FileShareManifest::T_FILE)
		   {
			   //in normal transfer mode, move the file to download folder
				LOG(INFO) << "in normal transfer mode, moving downloaded from tmp folder to specified download folder";
				LOG(INFO) << "Tmp folder is: " << temp_name.pathname();
				LOG(INFO) << "Specified target location is: " << local_name.pathname();

				if (!talk_base::Filesystem::MoveFile(temp_name.pathname(), local_name.pathname()))
				{
					success = false;
					LOG(LS_ERROR) << "Couldn't move downloaded file from '"
							<< temp_name.pathname() << "' to '" << local_name.pathname();
				}

				if (success && is_folder) {
					talk_base::Filesystem::DeleteFile(transfer_path_);
				}

		   }
		}
		else
		{
			LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete:In preview mode, file is saved at: " << temp_name.pathname();
			LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete:In preview mode, iCurrentPreviewW: " << iCurrentPreviewW
					<< ", iCurrentPreviewH: " << iCurrentPreviewH;
		}

	}

//	if (!success) {
//		if (!talk_base::Filesystem::DeleteFile(transfer_path_)) {
//			LOG(LS_ERROR) << "Couldn't delete downloaded file: " << transfer_path_;
//		}
//		if (!IsComplete()) {
//			SetState(FS_FAILURE, false);
//		}
//
//		if(bInPreviewMode)
//		{
//			LOG(LS_ERROR) << "Preview request failed:transfer path is: " << transfer_path_;
//
//			bInPreviewMode = false;
//		}
//
//		return;
//	}


	if(bInPreviewMode)
	{
		bInPreviewMode = false;

		//close the stream at first
//		ASSERT(currentPreviewFile != NULL);
//		currentPreviewFile->Close();

		LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete:Preview request succeeded";

		//Notify UI layer here, pass the file path to UI layer
		SignalPreviewReceived(this, temp_name.pathname(), iCurrentPreviewW, iCurrentPreviewH);

		return;
	}

	LOG(INFO) << "VueceMediaStreamSession:OnHttpClientComplete:NOT In preview mode, continue.";


	// We may have skipped over some items (if they are directories, or otherwise
	// failed.  resize ensures that we populate the skipped entries with empty
	// strings.
	stored_location_.resize(item_transferring_ + 1);
	stored_location_[item_transferring_] = local_name.pathname();

	// bytes_transferred_ represents the size of items which have completely
	// transferred, and is added to the progress of the currently transferring
	// items.
	if (item.size == FileShareManifest::SIZE_UNKNOWN) {
		bytes_transferred_ += 1;
	} else {
		bytes_transferred_ += item.size;
	}
	item_transferring_ += 1;
	NextDownload();
}

void VueceMediaStreamSession::OnHttpClientClosed(talk_base::HttpClient* http, int err) {
	LOG(INFO) << "VueceMediaStreamSession:OnHttpClientClosed";
	LOG_F(LS_INFO)
		<< "(" << err << ")";
}

// HttpServer Signals

void VueceMediaStreamSession::OnHttpRequest(talk_base::HttpServer* server, talk_base::HttpServerTransaction* transaction) {

	LOG(INFO) << "\nVueceMediaStreamSession -------------------------------------------------------";
	LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest";
	LOG(INFO) << "\nVueceMediaStreamSession -------------------------------------------------------";
	LOG_F(LS_INFO) << "(" << transaction->request()->path << ")";

	ASSERT(server == http_server_);

	std::string path, query;
	size_t query_start = transaction->request()->path.find('?');
	if (query_start != std::string::npos) {
		path = transaction->request()->path.substr(0, query_start);
		query = transaction->request()->path.substr(query_start + 1);
	} else {
		path = transaction->request()->path;
	}

	talk_base::Pathname remote_name(path);
	bool preview = (preview_path_ == remote_name.folder());
	bool original = (source_path_ == remote_name.folder());

	if(preview)
	{
		LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:This is a preview file request";
	}

	if(original)
	{
		LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:This is an original file request";
	}

	std::string requested_file(remote_name.filename());
	talk_base::transform(requested_file, requested_file.size(), requested_file, talk_base::url_decode);

	LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Requested file is: " << requested_file;

	size_t item_index;
	const FileShareManifest::Item* item = NULL;
	if (preview || original)
	{
		for (size_t i = 0; i < manifest_->size(); ++i)
		{
			LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Checking item in local manifest: " << manifest_->item(i).name;

			if (manifest_->item(i).name == requested_file)
			{
				item_index = i;
				item = &manifest_->item(item_index);

				LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Requested item located in local manifest";
				break;
			}
		}
	}

	talk_base::StreamInterface* stream = NULL;
	std::string mime_type(MIME_OCTET_STREAM);

	if (!item) {
		// Fall through
		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Target item not found in local manifest, transfer will fail";
	}
	else if (preview)
	{
		// Only image previews allowed
		unsigned int width = 0, height = 0;
		bool bAllowPreviewResponse = false;

		LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Starting handling preview request.";

		if( 	(item->type == FileShareManifest::T_IMAGE) &&
				!query.empty() &&
				(sscanf(query.c_str(), "width=%u&height=%u", &width, &height) == 2))
		{

			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview request is valid in normal mode.";

			//Preview response is allowed in normal streaming mode with image file.
			bAllowPreviewResponse = true;
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview response is allowed in normal streaming mode with image file.";
		}
		else if( (item->type == FileShareManifest::T_MUSIC) &&
				!query.empty() &&
				(sscanf(query.c_str(), "width=%u&height=%u", &width, &height) == 2))
		{
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview request is valid in music streaming mode.";

			//Preview response is allowed in music streaming mode.
			bAllowPreviewResponse = true;
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview response is allowed in music streaming mode.";
		}
		else
		{
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview response is NOT allowed";
		}

		if (bAllowPreviewResponse)
		{
			//Note the 'name' value of this item is a UUID, which can not be used to open the original
			//file, the actual file path should already be injected when the session client calls
			//the CreateMediaStreamSessionAsInitiator method, here we just make sure the path is not
			//empty

			std::string decodedAbsFPath;
			talk_base::Pathname local_path;

			width = talk_base::_max<unsigned int>(1, talk_base::_min(width, kMaxPreviewSize));
			height = talk_base::_max<unsigned int>(1, talk_base::_min(height, kMaxPreviewSize));

			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview: width = " << width;
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview: height = " << height;
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview: sender_source_folder = " << sender_source_folder;
			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview: item name = " << item->name;

			if ( (item->type == FileShareManifest::T_MUSIC))
			{
				LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest(T_MUSIC) - actual_file_path: " << actual_file_path;

				ASSERT(actual_file_path.length() > 0);

				local_path.AppendPathname(actual_file_path);
			}
			else
			{
				std::string decodedFname;

				talk_base::Base64::Decode(item->name, talk_base::Base64::DO_STRICT,  &decodedFname, NULL);
				LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest(non-MUSIC) - decodedFname: " << decodedFname;
				local_path.AppendPathname(decodedFname);
			}

			LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequest:actual file path(after decoding) is: " << local_path.pathname();

			//NOTE: JJ - Keep the following code for reference, following code tries to get
			//the path of the original file, that's not what we want, we want the file path
			//of the preview, which is now injected by SetActualPreviewFilePath() from
			//VueceFileShareSessionClient::CreateVueceFileShareSessionAsInitiator
////////////
			//std::string pathname;
//			if (is_sender_) {
//				talk_base::Pathname local_path;
//				local_path.SetFolder(local_folder_);
//				local_path.SetFilename(item->name);
//				pathname = local_path.pathname();
//
//				LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview:I'm sender";
//			} else if ((item_index < stored_location_.size()) && !stored_location_[item_index].empty()) {
//				pathname = stored_location_[item_index];
//				LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview:I'm receiver";
//			}
////////////////////

			LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Preview: actual_preview_path = " << actual_preview_path;

			if (!actual_preview_path.empty()) {
				transactions_.push_back(transaction);

				LOG(INFO) << "VueceMediaStreamSession:OnHttpRequest:Start image re-sampling. Response will be sent in ResampleComplete().";

				// IMPORTANT NOTE: JJ - This is a tricky call, once the slot in VueceFileShareSessionClient::OnResampleImage
				// is finished, ResampleComplete() will be called and respond will be sent in that method
				// so for preview request we SHOULD NOT send response at the end of this method.
				SignalResampleImage(this, actual_preview_path, width, height, transaction);

				//return from here
				return;
			}
			else
			{
				LOG(LS_ERROR) << "Preview file path is empty, sth is wrong, abort now!";

				//abort
				ASSERT( !actual_preview_path.empty() );
			}
		}
	}
	else if (item->type == FileShareManifest::T_FOLDER)
	{
		talk_base::Pathname local_path;
		local_path.AppendFolder(item->name);
		talk_base::TarStream* tar = new talk_base::TarStream;
		VERIFY(tar->AddFilter(local_path.folder_name()));
		if (tar->Open(local_path.parent_folder(), true)) {
			stream = tar;
			tar->SignalNextEntry.connect(this, &VueceMediaStreamSession::OnNextEntry);
			mime_type = "application/x-tar";
		} else {
			delete tar;
		}
	}

	if ( (item->type == FileShareManifest::T_MUSIC))
	{
		talk_base::Pathname local_path;
		bool fOpend = 0;

		std::string decodedAbsFPath;

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Requested item type is MUSIC, item->name: " << item->name;

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:actual_file_path: " << actual_file_path;

		local_path.AppendPathname(actual_file_path);

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:actual file path is: " << local_path.pathname();

		/**
		 * Notes
		 * 1. if app role is wrong, stream will be NULL, a 404 response will be sent back
		 * 2. For hub node, sample_rate, bit_rate, nchannels and duration are not used here, they will be updated
		 *    when the stream is opened, see VueceMediaStream::Open(const std::string& filename, const char* mode)
		 */
		talk_base::VueceMediaStream* file = new talk_base::VueceMediaStream(GetSessionId(), sample_rate, bit_rate, nchannels, duration);

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - create vuece media stream as hub server with start position: " << start_pos;

		if(start_pos < 0)
		{
			LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - start postion is a negative value, reset to 0";
			start_pos = 0;
		}

		fOpend = (file->Open(local_path.pathname().c_str(), "rb", start_pos));

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - file:Open() returned: " << fOpend;

		if ( fOpend )
		{
			LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:Meida hub - File opened";

			stream = file;

			if(pVueceMediaStream != NULL)
			{
				VueceLogger::Fatal("VueceMediaStreamSession:OnHttpRequest - pVueceMediaStream should be NULL!");
				return;
			}

			pVueceMediaStream = file;

		}
		else
		{
			LOG(LS_ERROR) << "VueceMediaStreamSession:OnHttpRequest:Meida hub - VueceMediaStream:Open() returned false, cancel this session now";
			delete file;
		}
	}
	else
	{
		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest - Requested item type is non-MUSIC, item->name: " << item->name;

		talk_base::Pathname local_path;
		local_path.SetFolder(sender_source_folder);

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:item->name: " << item->name;

		local_path.AppendPathname(item->name);

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:actual file path is: " << local_path.pathname();

		talk_base::FileStream* file = new talk_base::FileStream;

		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:In non-streaming mode - opening file " << local_path.pathname();

		if (file->Open(local_path.pathname().c_str(), "rb", NULL))
		{
			LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:File opened";
			stream = file;
		}
		else
		{
			delete file;
		}
	}

	if (!stream)
	{
		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:Stream not found, set error code to 404";
		transaction->response()->set_error(talk_base::HC_NOT_FOUND);
	}
	else if (original)
	{
		// We should never have more than one original request pending at a time
		ASSERT(NULL == counter_);
		StreamCounter* counter = new StreamCounter(stream);
		counter->SignalUpdateByteCount.connect(this, &VueceMediaStreamSession::OnUpdateBytes);

		//JJ - Note this is the place where filestream is injected into the response
		transaction->response()->set_success(mime_type.c_str(), counter);

		transfer_connection_id_ = transaction->connection_id();
		item_transferring_ = item_index;
		counter_ = counter;
	} else {
		// Note: in the preview case, we don't set counter_, so the transferred
		// bytes won't be shown as progress, and won't trigger a state change.
		// See important note above about SignalResampleImage
		LOG(LS_INFO) << "VueceMediaStreamSession:OnHttpRequest:This is an image preview request, response will not be sent here.";
		return;
	}

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequest:Result: " << transaction->response()->scode;

	//Note the actual streaming is started by following Respond() call.
		http_server_->Respond(transaction);
}


//This method is only called on server side when a transfer is completed, it's called from httpbase::do_complete
void VueceMediaStreamSession::OnHttpRequestComplete(talk_base::HttpServer* server, talk_base::HttpServerTransaction* transaction, int err) {

	LOG(INFO) << "\nVueceMediaStreamSession -----------------------------------------------------------------------";
	LOG(INFO) << "VueceMediaStreamSession:OnHttpRequestComplete";
	LOG(INFO) << "VueceMediaStreamSession -----------------------------------------------------------------------\n";

	LOG(INFO) << "Req path:(" << transaction->request()->path << "), Err code: " << err << "";

	LOG(INFO) << "VueceMediaStreamSession:OnHttpRequestComplete, transfer_connection_id_ = " << transfer_connection_id_;
	LOG(INFO) << "VueceMediaStreamSession:OnHttpRequestComplete, transaction->connection_id() = " << transaction->connection_id();

	ASSERT(server == http_server_);

	std::string path, query;
	size_t query_start = transaction->request()->path.find('?');

	if (query_start != std::string::npos) {
		path = transaction->request()->path.substr(0, query_start);
		query = transaction->request()->path.substr(query_start + 1);
	} else {
		path = transaction->request()->path;
	}

	talk_base::Pathname remote_name(path);
	bool bIsPreviewReq = (preview_path_ == remote_name.folder());

	if(bIsPreviewReq)
	{
		talk_base::Pathname previewFilePath(actual_preview_path);

		LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequestComplete:Preview request is finished, delete preview file now.";
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequestComplete:Preview file path: " << actual_preview_path;

		if(pCurrentPreviewFileStream != NULL)
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequestComplete - Close preview file stream at first";
			pCurrentPreviewFileStream->Close();
		}

		if(talk_base::Filesystem::DeleteFile(previewFilePath))
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpRequestComplete:Preview file is deleted.";
		}
		else
		{
			//AAA
			LOG(LS_ERROR) << "VueceMediaStreamSession:OnHttpRequestComplete:Cannot delete preview file.";

		}
	}


	// We only care about transferred originals
	if (transfer_connection_id_ != transaction->connection_id())
		return;

	ASSERT(item_transferring_ < manifest_->size());
	ASSERT(NULL != counter_);

	transfer_connection_id_ = talk_base::HTTP_INVALID_CONNECTION_ID;
	transfer_name_.clear();
	counter_ = NULL;

	if (err == talk_base::HE_NONE) {
		const FileShareManifest::Item& item = manifest_->item(item_transferring_);
		if (item.size == FileShareManifest::SIZE_UNKNOWN) {
			bytes_transferred_ += 1;
		} else {
			bytes_transferred_ += item.size;
		}
	}
	else
	{
		LOG(LS_ERROR) << "VueceMediaStreamSession:OnHttpRequestComplete - ***********************";
		LOG(LS_ERROR) << "VueceMediaStreamSession:OnHttpRequestComplete:Completed with an error: " << err;
		LOG(LS_ERROR) << "VueceMediaStreamSession:OnHttpRequestComplete - ***********************";

		SetState(FS_COMPLETE, false);
	}


	if(VueceGlobalContext::GetAppRole() == VueceAppRole_Media_Hub)
	{
		//cricket::XmlElements els;

		//LOG(INFO) << "VueceMediaStreamSession:OnHttpRequestComplete - This is hub, notify remote client the completion of transfer";

		//els.push_back(new buzz::XmlElement(QN_SHARE_COMPLETE, true));

		/*
		 * Note this will send a message like this:
		 *
		 *  <iq to="tom@gmail.com/vuece96E9D0AD" type="set" id="25" from="alice@gmail.com/vuece2960E46A">
		 *   	<session type="info" id="3043787356" initiator="tom@gmail.com/vuece96E9D0AD" xmlns="http://www.google.com/session">
		 *   		<complete xmlns="http://www.google.com/session/share"/>
		 *   		</session>
		 *   </iq>
		 */

		//session_->SendInfoMessage(els);

		/*
		 * Note the state change will trigger a another message like this:
		 * <iq to="tom@gmail.com/vuece96E9D0AD" type="set" id="26" from="alice@gmail.com/vuece2960E46A">
		 *  	<session type="terminate" id="3043787356" initiator="tom@gmail.com/vuece96E9D0AD" xmlns="http://www.google.com/session">
		 *   		<success/>
		 *   	</session>
		 *  </iq>
		 */
		//SetState(FS_COMPLETE, false);

		//those two messages above will trigger session termination at the hub client side
	}
}

void VueceMediaStreamSession::OnHttpConnectionClosed(talk_base::HttpServer* server, int err, talk_base::StreamInterface* stream) {

	LOG(LS_VERBOSE) << "----------------------VueceMediaStreamSession:OnHttpConnectionClosed START ------------------------------";

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpConnectionClosed - Dispose current thread, related stream instance will be destroyed";
	LOG_F(LS_INFO) << "(" << err << ")";

	talk_base::Thread::Current()->Dispose(stream);

	LOG(LS_VERBOSE) << "----------------------VueceMediaStreamSession:OnHttpConnectionClosed DONE ------------------------------";

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnHttpConnectionClosed - current_file_type = " << current_file_type;

	if(current_file_type == FileShareManifest::T_MUSIC)
	{
		LOG(LS_VERBOSE) << "Transferred item is music, notify remote host now.";
		OnMediaStreamingStopped(0);
	}
}

// TarStream Signals
void VueceMediaStreamSession::OnNextEntry(const std::string& name, size_t size) {

	LOG(INFO) << "VueceMediaStreamSession:OnNextEntry";
	LOG_F(LS_VERBOSE) << "(" << name << ", " << size << ")";

	transfer_name_ = name;
	SignalNextFile(this);
}

// Socket Signals

void VueceMediaStreamSession::OnProxyAccept(talk_base::AsyncSocket* socket) {
	LOG(INFO) << "VueceMediaStreamSession:OnProxyAccept";

	//NOTE - This method is never called and tested so far, we abort application if this happens for now.
	ASSERT(false);

	bool is_remote;
	if (socket == remote_listener_) {
		is_remote = true;
		ASSERT(NULL != session_);
	} else if (socket == local_listener_) {
		is_remote = false;
	} else {
		ASSERT(false);
		return;
	}

	while (talk_base::AsyncSocket* accepted = static_cast<talk_base::AsyncSocket*> (socket->Accept(NULL))) {

		// Check if connection is from localhost.
		if (accepted->GetRemoteAddress().ip() != 0x7F000001) {
			delete accepted;
			continue;
		}

		LOG_F(LS_VERBOSE)
			<< (is_remote ? "[remote]" : "[local]");

		if (is_remote) {
			char channel_name[64];
			talk_base::sprintfn(channel_name, ARRAY_SIZE(channel_name), "proxy-%u", next_channel_id_++);
			talk_base::StreamInterface* remote = (NULL != session_) ? CreateChannel(channel_name) : NULL;
			if (!remote) {
				LOG_F(LS_WARNING)
					<< "CreateChannel(" << channel_name << ") failed";
				delete accepted;
				continue;
			}

			talk_base::StreamInterface* local = new talk_base::SocketStream(accepted);
			StreamRelay* proxy = new StreamRelay(local, remote, 64 * 1024);
			proxy->SignalClosed.connect(this, &VueceMediaStreamSession::OnProxyClosed);
			proxies_.push_back(proxy);
			proxy->Circulate();
			talk_base::Thread::Current()->Clear(this, MSG_PROXY_WAIT);
		} else {
			talk_base::StreamInterface* local = new talk_base::SocketStream(accepted);
			http_server_->HandleConnection(local);
		}
	}
}

void VueceMediaStreamSession::OnProxyClosed(StreamRelay* proxy, int error) {
	LOG(INFO) << "VueceMediaStreamSession:OnProxyClosed";
	ProxyList::iterator it = std::find(proxies_.begin(), proxies_.end(), proxy);
	if (it == proxies_.end()) {
		ASSERT(false);
		return;
	}

	LOG_F(LS_VERBOSE)
		<< "(" << error << ")";

	proxies_.erase(it);
	talk_base::Thread::Current()->Dispose(proxy);

	if (proxies_.empty() && IsComplete() && !IsClosed()) {
		talk_base::Thread::Current()->PostDelayed(kProxyWait, this, MSG_PROXY_WAIT);
	}
}

void VueceMediaStreamSession::OnUpdateBytes(size_t count) {
//	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnUpdateBytes 1";


	//Code not used
	//	if(!bCancelled)
//	{
//		SignalUpdateProgress(this);
//	}
//	else
//	{
//		LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnUpdateBytes - Already cancelled, no need to notify";
//		return;
//	}

	SignalUpdateProgress(this);

//	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnUpdateBytes 2";
}

// Internal Helpers

void VueceMediaStreamSession::GenerateTemporaryPrefix(std::string* prefix) {
	std::string data = talk_base::CreateRandomString(32);
	ASSERT(NULL != prefix);
	prefix->assign("/temporary/");
	prefix->append(talk_base::MD5(data));
	prefix->append("/");
}

void VueceMediaStreamSession::GetItemNetworkPath(size_t index, bool preview, std::string* path) {

	LOG(LS_VERBOSE) << "VueceMediaStreamSession::GetItemNetworkPath";

	ASSERT(index < manifest_->size());
	ASSERT(NULL != path);

	// preview_path_ and source_path_ are url path segments, which are composed
	// with the address of the localhost p2p proxy to provide a url which IE can
	// use.

	std::string ue_name;
	const std::string& name = manifest_->item(index).name;
	talk_base::transform(ue_name, name.length() * 3, name, talk_base::url_encode);

	talk_base::Pathname pathname;
	pathname.SetFolder(preview ? preview_path_ : source_path_);
	pathname.SetFilename(ue_name);
	*path = pathname.pathname();
}

bool VueceMediaStreamSession::GetItemBaseUrl(size_t index, bool preview, std::string* url) {
	// This function composes a URL to the referenced item.  It may be a local
	// file url (file:///...), or a remote peer url relayed through localhost
	// (http://...)
	ASSERT(NULL != url);
	if (index >= manifest_->size()) {
		ASSERT(false);
		return false;
	}

	const FileShareManifest::Item& item = manifest_->item(index);

	bool is_remote;
	if (is_sender_) {
		if (!preview) {
			talk_base::Pathname path("");
			path.SetFilename(item.name);
			*url = path.url();
			return true;
		}
		is_remote = false;
	} else {
		if ((index < stored_location_.size()) && !stored_location_[index].empty()) {
			if (!preview) {
				*url = talk_base::Pathname(stored_location_[index]).url();
				return true;
			}
			// Note: Using the local downloaded files as a source for previews is
			// desireable, because it means that previews can be regenerated if IE's
			// cached versions get flushed for some reason, and the remote side is
			// not available.  However, it has the downside that IE _must_ regenerate
			// the preview locally, which takes time, memory and CPU.  Eventually,
			// we will unify the remote and local cached copy through some sort of
			// smart http proxying.  In the meantime, always use the remote url, to
			// eliminate the annoying transition from remote to local caching.
			//is_remote = false;
			is_remote = true;
		} else {
			is_remote = true;
		}
	}

	talk_base::SocketAddress address;
	if (!GetProxyAddress(address, is_remote))
		return false;

	std::string path;
	GetItemNetworkPath(index, preview, &path);
	talk_base::Url<char> make_url(path.c_str(), address.IPAsString().c_str(), address.port());
	*url = make_url.url();
	return true;
}

bool VueceMediaStreamSession::GetProxyAddress(talk_base::SocketAddress& address, bool is_remote) {
	talk_base::AsyncSocket*& proxy_listener = is_remote ? remote_listener_ : local_listener_;

	if (!proxy_listener) {
		talk_base::AsyncSocket* listener = talk_base::Thread::Current()->socketserver() ->CreateAsyncSocket(SOCK_STREAM);
		if (!listener)
			return false;

		talk_base::SocketAddress bind_address("127.0.0.1", 0);

		if ((listener->Bind(bind_address) != 0) || (listener->Listen(5) != 0)) {
			delete listener;
			return false;
		}

		LOG(LS_INFO)
			<< "Proxy listener available @ " << listener->GetLocalAddress().ToString();

		listener->SignalReadEvent.connect(this, &VueceMediaStreamSession::OnProxyAccept);
		proxy_listener = listener;
	}

	if (proxy_listener->GetState() == talk_base::Socket::CS_CLOSED) {
		if (is_remote) {
			address = remote_listener_address_;
			return true;
		}
		return false;
	}

	address = proxy_listener->GetLocalAddress();
	return !address.IsAny();
}

void VueceMediaStreamSession::SendTransportAccept()
{
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:SendTransportAccept";

	cricket::XmlElements transAccept;
	buzz::XmlElement* transElem = new buzz::XmlElement(QN_GINGLE_P2P_TRANSPORT, true);
	transAccept.push_back(transElem);
	session_->SendTransportAcceptMessage(transAccept);

}

void VueceMediaStreamSession::SendTransportInfo()
{
//	LOG(LS_VERBOSE) << "VueceMediaStreamSession:SendTransportInfo";
//	cricket::XmlElements els;
//	buzz::XmlElement* xel_channel = new buzz::XmlElement(QN_SHARE_CHANNEL, true);
//	xel_channel->AddAttr(buzz::QN_NAME, channel_name);
//	els.push_back(xel_channel);
//	session_->SendInfoMessage(els);
}

talk_base::StreamInterface* VueceMediaStreamSession::CreateChannel(const std::string& channel_name) {
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:CreateChannel";
	ASSERT(NULL != session_);

	// Send a heads-up for our new channel
	/*
	 *    <iq to="tom@gmail.com/pcpF5B83FDD" type="set" id="50" from="jack@gmail.com/Talk.v104C20D4FEF">
			 <session type="info" id="4215662446" initiator="tom@gmail.com/pcpF5B83FDD" xmlns="http://www.google.com/session">
			   <channel name="private-1" xmlns="http://www.google.com/session/share"/>
			 </session>
		   </iq>
	 */

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:CreateChannel:==========> Send channel info";

	cricket::XmlElements els;
	buzz::XmlElement* xel_channel = new buzz::XmlElement(QN_SHARE_CHANNEL, true);
	xel_channel->AddAttr(buzz::QN_NAME, channel_name);
	els.push_back(xel_channel);
	session_->SendInfoMessage(els);

	/*
	 *    <iq to="tom@gmail.com/pcpF5B83FDD" type="set" id="48" from="jack@gmail.com/Talk.v104C20D4FEF">
			 <session type="transport-accept" id="4215662446" initiator="tom@gmail.com/pcpF5B83FDD" xmlns="http://www.google.com/session">
			   <transport xmlns="http://www.google.com/transport/p2p"/>
			 </session>
		   </iq>
	 */
	//	SendTransportAccept();
//	cricket::XmlElements transAccept;
//	buzz::XmlElement* transElem = new buzz::XmlElement(QN_GINGLE_P2P_TRANSPORT, true);
//	transAccept.push_back(transElem);
//	session_->SendTransportAcceptMessage(transAccept);


	cricket::PseudoTcpChannel* channel = new cricket::PseudoTcpChannel(talk_base::Thread::Current(), session_);
//	cricket::PseudoTcpChannel* channel = new cricket::PseudoTcpChannel(httpServerThread->GetWorkThread(), session_);

	//arg0: content name, e.g, http://www.google.com/session/share
	//arg1: channel name, e.g, private-1

//	VERIFY(channel->Connect(NS_GOOGLE_SHARE, channel_name));
	VERIFY(channel->Connect(CN_OTHER, channel_name));

	return channel->GetStream();
}

void VueceMediaStreamSession::SetState(FileShareState state, bool prevent_close) {

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:" << state;

	if (state == state_)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState: State is not changed, do nothing and return.";
		return;
	}


	if (IsComplete()) {

		LOG(LERROR) << "VueceMediaStreamSession:SetState:Session is not completed yet, this should not happen.";

		// Entering a completion state is permanent.
		ASSERT(false);

		return;
	}

	state_ = state;

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState: Current state is updated to " << state_;

	if(!prevent_close)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:prevent_close is false, session will be closed if completed. ";
	}
	else
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:prevent_close is true, session will not be closed if not completed. ";
	}


	if (IsComplete()) {

		LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:Session is completed.";

		if(prevent_close)
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:Session is completed, prevent_close is true, will not call DoClose()";
		}
		else
		{
			LOG(LS_VERBOSE) << "VueceMediaStreamSession:SetState:Session is completed, prevent_close is false, Calling DoClose()";
		}

		// All completion states auto-close except for FS_COMPLETE
//		bool close = (state_ > FS_COMPLETE) || !prevent_close;

		bool close = !prevent_close;

		if (close) {

			if(state_ == FS_REMOTE_CANCEL)
			{
				LOG(LS_VERBOSE) << "VueceMediaStreamSession:Received a terminate request from remote client";
			}


			LOG(LS_VERBOSE) << "VueceMediaStreamSession:Call DoClose() now";

			////////////////AAAAAAAAAAAAAAAAAAAAA
			//Start monitoring the termination procedure.
			DoClose(true);
		}
	}

	//this is connected to VueceMediaStreamSessionClient::OnSessionState()
	SignalState(this, state_);
}

void VueceMediaStreamSession::OnInitiate() {
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate";
	// Cache the variables we will need, in case session_ goes away
	is_sender_ = session_->initiator();
	jid_ = buzz::Jid(session_->remote_name());

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:initiator: " << session_->initiator();
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:remote: " << session_->remote_name();

	//get file content description
	const FileContentDescription* fDescription = GetFileContentDescription();

	LogFileContentDescription(fDescription);

	manifest_ = &fDescription->manifest;

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:iManifest initiated.";
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:A";

	source_path_ = fDescription->source_path;
	preview_path_ = fDescription->preview_path;

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:source_path_: "  << source_path_;
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:preview_path_: " << preview_path_;

	if (sender_source_folder.empty())
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession - no pre-defined source folder, using temp";

		talk_base::Pathname temp_folder;
		talk_base::Filesystem::GetTemporaryFolder(temp_folder, true, NULL);
		sender_source_folder = temp_folder.pathname();

		LOG(LS_VERBOSE) << "VueceMediaStreamSession:OnInitiate:Local folder to be used: " << sender_source_folder;
	}

	http_client_ = new talk_base::HttpClient(user_agent_, &pool_);
	http_client_->SignalHttpClientComplete.connect(this, &VueceMediaStreamSession::OnHttpClientComplete);
	http_client_->SetMyName("HTTP-Client-Vuece-Hub-Client");

	LOG(LS_INFO) << "Current state is: " << GetStateString((FileShareState)session_->state());
	SetState(FS_OFFER, false);
}

void VueceMediaStreamSession::LogHttpErrorString(int code)
{
	switch(code)
	{
	case talk_base::HE_NONE:
		LOG(INFO) << "Http code: " << code << " - None";
		break;
	case talk_base::HE_PROTOCOL:
		LOG(INFO) << "Http code: " << code << " - Received non-valid HTTP data";
		break;
	case talk_base::HE_DISCONNECTED:
		LOG(INFO) << "Http code: " << code << " - Connection closed unexpectedly";
		break;
	case talk_base::HE_OVERFLOW:
		LOG(INFO) << "Http code: " << code << " - Received too much data for internal buffers";
		break;
	case talk_base::HE_CONNECT_FAILED:
		LOG(INFO) << "Http code: " << code << " - The socket failed to connect";
		break;
	case talk_base::HE_SOCKET_ERROR:
		LOG(INFO) << "Http code: " << code << " - An error occurred on a connected socket";
		break;
	case talk_base::HE_SHUTDOWN:
		LOG(INFO) << "Http code: " << code << " - Http object is being destroyed";
		break;
	case talk_base::HE_OPERATION_CANCELLED:
		LOG(INFO) << "Http code: " << code << " - Connection aborted locally";
		break;
	case talk_base::HE_AUTH:
		LOG(INFO) << "Http code: " << code << " - Proxy Authentication Required";
		break;
	case talk_base::HE_CERTIFICATE_EXPIRED:
		LOG(INFO) << "Http code: " << code << " - Cert expired during SSL negotiation";
		break;
	case talk_base::HE_STREAM:
		LOG(INFO) << "Http code: " << code << " - Problem reading or writing to the document";
		break;
	case talk_base::HE_CACHE:
		LOG(INFO) << "Http code: " << code << " - Problem reading from cache";
		break;
	case talk_base::HE_DEFAULT:
		LOG(INFO) << "Http code: " << code << " - Un-registered error code";
		break;
	default:
		break;
	}
}


std::string VueceMediaStreamSession::GetStateString(FileShareState state) const
{

    std::string result;

	switch(state){
	case FS_NONE:{
		result =  "NONE";
		return result;
	}
	case FS_OFFER:{
		result =  "OFFER";
		return result;
	}
	case FS_TRANSFER:{
		result =  "TRANSFER";
		return result;
	}
	case FS_COMPLETE:{
		result =  "COMPLETE";
		return result;
	}
	case FS_LOCAL_CANCEL:{
		result =  "LOCAL_CANCEL";
		return result;
	}
	case FS_REMOTE_CANCEL:{
		result =  "REMOTE_CANCEL";
		return result;
	}
	case FS_FAILURE:{
		result =  "FAILURE";
		return result;
	}
	default:{
		result =  "ERR_UNKNOWN_STATE";
		LOG(LS_ERROR) << "GetStateString: Unknown state value" << state;
		return result;
	}
	}
}

void VueceMediaStreamSession::NextDownload() {

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload";

	if (FS_TRANSFER != state_)
	{
		LOG(LS_ERROR) << "VueceMediaStreamSession:NextDownload:Current state is not FS_TRANSFER, return now.";
		return;
	}

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload:item_transferring_: " << item_transferring_;

	if (item_transferring_ >= manifest_->size()) {
		// Notify the other side that transfer has completed
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload:Notify the other side that transfer has completed, sending COMPLETE message";

		cricket::XmlElements els;
		els.push_back(new buzz::XmlElement(QN_SHARE_COMPLETE, true));
		session_->SendInfoMessage(els);

		LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload:Notify the other side that transfer has completed, calling SetState(FS_COMPLETE...)";

		SetState(FS_COMPLETE, !proxies_.empty());

		return;
	}

	const FileShareManifest::Item& item = manifest_->item(item_transferring_);
	if (
			(item.type != FileShareManifest::T_FILE)
			&& (item.type != FileShareManifest::T_IMAGE)
			&& (item.type != FileShareManifest::T_FOLDER)
			&& (item.type != FileShareManifest::T_MUSIC)
	)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload:Skip this item because it is not downloadable.";
		item_transferring_ += 1;
		NextDownload();
		return;
	}

	const bool is_folder = (item.type == FileShareManifest::T_FOLDER);
	talk_base::Pathname temp_name;
	temp_name.SetFilename(item.name);

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload:item.type = " << item.type << ", current file type is set to this type";

	current_file_type = item.type;

	if(item.type == FileShareManifest::T_FILE)
	{
		LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload - File type is  T_FILE, actual file will be created and saved, now create temp file.";

		if (!talk_base::CreateUniqueFile(temp_name, !is_folder))
		{
			LOG(LS_ERROR) << "VueceMediaStreamSession:NextDownload - Failed to create tmp file.";

			SetState(FS_FAILURE, false);
			return;
		}

		LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload - Temp file created: " << temp_name.pathname();
	}

	talk_base::StreamInterface* stream = NULL;
	if (is_folder)
	{
		// Convert unique filename into unique foldername
		temp_name.AppendFolder(temp_name.filename());
		temp_name.SetFilename("");
		talk_base::TarStream* tar = new talk_base::TarStream;

		// Note: the 'target' directory will be a subdirectory of the transfer_path_
		talk_base::Pathname target;
		target.SetFolder(item.name);
		tar->AddFilter(target.pathname());

		if (!tar->Open(temp_name.pathname(), false))
		{
			delete tar;
			SetState(FS_FAILURE, false);
			return;
		}
		stream = tar;
		tar->SignalNextEntry.connect(this, &VueceMediaStreamSession::OnNextEntry);
	}
	else
	{
		   if( item.type == FileShareManifest::T_FILE)
		   {
			   //creatre a normal file stream
				talk_base::FileStream* file = new talk_base::FileStream;

				LOG(LS_VERBOSE) << "NVueceMediaStreamSession:extDownload - File type is T_FILE, opening temp file: " << temp_name.pathname();

				if (!file->Open(temp_name.pathname().c_str(), "wb", NULL))
				{

					LOG(LS_ERROR) << "VueceMediaStreamSession:NextDownload:Cannot open this file!";

					delete file;
					talk_base::Filesystem::DeleteFile(temp_name);
					SetState(FS_FAILURE, false);
					return;
				}

				LOG(INFO) << "VueceMediaStreamSession:NextDownload - File successfully opened.";

				stream = file;
		   }
		   else if( item.type == FileShareManifest::T_MUSIC)
		   {
				LOG(INFO) << "VueceMediaStreamSession:NextDownload - File type is T_MUSIC, create Vuece media stream file now.";

				/*
				 * IMPORTANT -
				 * This is where music attributes get populated by remote share request message
				 */
				sample_rate = item.sample_rate;
				bit_rate = item.bit_rate;
				nchannels = item.nchannels;
				duration = item.duration;

				ASSERT(sample_rate != 0);
				ASSERT(bit_rate != 0);
				ASSERT(nchannels != 0);
				ASSERT(duration != 0);

				LOG(INFO) << "VueceMediaStreamSession:NextDownload - Music attritues populated from item in manifest: sample_rate = " << sample_rate
						<< ", bit_rate = " << bit_rate << ", nchannels = " << nchannels
						<< ", duration = " << duration;

				talk_base::VueceMediaStream* file = new talk_base::VueceMediaStream(GetSessionId(), sample_rate, bit_rate, nchannels, duration);

				if(!file->Open(temp_name.pathname().c_str(), "hubclient"))
				{
					LOG(LS_ERROR) << "VueceMediaStreamSession:NextDownload:Cannot open this file!";

					delete file;
					talk_base::Filesystem::DeleteFile(temp_name);
					SetState(FS_FAILURE, false);
					return;
				}

				LOG(INFO) << "VueceMediaStreamSession:NextDownload:Vuece file stream successfully opened.";
				stream = file;

				if(pVueceMediaStream != NULL)
				{
					VueceLogger::Fatal("VueceMediaStreamSession:NextDownload - pVueceMediaStream should be NULL!");
					return;
				}

				pVueceMediaStream = file;
		   }
		   else
		   {
			   LOG(INFO) << "VueceMediaStreamSession:NextDownload:Only MUSIC type is supported for now, abort.";
			   ASSERT(false);
			   return;
		   }
	}

	ASSERT(NULL != stream);
	transfer_path_ = temp_name.pathname();

	LOG(LS_VERBOSE) << "VueceMediaStreamSession:NextDownload - transfer_path_ is set to (same as temp file path): " << transfer_path_;

	std::string remote_path;
	GetItemNetworkPath(item_transferring_, false, &remote_path);

	LOG(INFO) << "VueceMediaStreamSession:NextDownload:remote_path: " << remote_path;

	StreamCounter* counter = new StreamCounter(stream);
	counter->SignalUpdateByteCount.connect(this, &VueceMediaStreamSession::OnUpdateBytes);
	counter_ = counter;

	LOG(INFO) << "VueceMediaStreamSession:NextDownload:Starting HTTP client with host name: " << jid_.Str();

	//NOTE this reset() call will cancel existing preview request, which will trigger VueceMediaStreamSession:OnHttpClientComplete()
	//event, in this case we should not cancel/terminate current share session because this is a preview request failure,
	//not the actual file share request

	//IMPORTANT NOTE!!! - The following reset call is commented out because now
	//preview request is not blindly sent out, it is sent only when preview path is
	//available, and the way how the UI layer works is different with normal file share,
	//it sends preview request then waits until preview is received, then sends accept
	//to start the actual data streaming, so there is no request to cancel
	//we probably need to use a new class to handle normal file share
//	http_client_->reset();
	//Note: JJ - The following line use old API, check the doc to understand:
	/*
	 *  // Creates the address with the given host and port.  If use_dns is true,
	 // the hostname will be immediately resolved to an IP (which may block for
	 // several seconds if DNS is not available).  Alternately, set use_dns to
	 // false, and then call Resolve() to complete resolution later, or use
	 // SetResolvedIP to set the IP explictly.
	 SocketAddress(const std::string& hostname, int port = 0, bool use_dns = true);
	 */
	//  http_client_->set_server(talk_base::SocketAddress(jid_.Str(), 0, false));
	//So we use new API as below
//	http_client_->set_server(talk_base::SocketAddress(jid_.Str(), 0));
	http_client_->set_server(talk_base::SocketAddress(jid_.Str(), 0, false));
	http_client_->request().verb = talk_base::HV_GET;
	http_client_->request().path = remote_path;
	http_client_->response().document.reset(counter);
	http_client_->start();

	LOG(INFO) << "VueceMediaStreamSession:NextDownload:HTTP client started.";
}

/**
 * 				sample_rate = item.sample_rate;
				bit_rate = item.bit_rate;
				nchannels = item.nchannels;
 */

void VueceMediaStreamSession::RetrieveUsedMusicAttributes(int* bitrate, int* samplerate, int* nchannels, int* duration)
{
	LOG(LS_VERBOSE) << "VueceMediaStreamSession:RetrieveUsedMusicAttributes";

	*bitrate = this->bit_rate;
	*samplerate = this->sample_rate;
	*nchannels = this->nchannels;
	*duration = this->duration;
}

const FileContentDescription* VueceMediaStreamSession::GetFileContentDescription() const{
	LOG(INFO) << "VueceMediaStreamSession:GetFileContentDescription";

	if(NULL == session_){
		LOG(LS_ERROR) << "session_ is NULL! abort now.";
	}

	ASSERT(NULL != session_);

	const cricket::SessionDescription* desc = session_->initiator() ? session_->local_description() : session_->remote_description();

	//Note we only have one content here, so finding first content should be ok for now
	const ContentInfo* ci = desc->FirstContentByType(NS_GOOGLE_SHARE);

	LOG(INFO) << "VueceMediaStreamSession:GetFileContentDescription:content name: " << ci->name << ", type: " << ci->type;

	const ContentDescription* contentDesc = ci->description;
	const FileContentDescription* fcd =  static_cast<const FileContentDescription*> (contentDesc);

	return fcd;
}

void VueceMediaStreamSession::LogFileContentDescription(const FileContentDescription* fcd)
{
	LOG(INFO) << "VueceMediaStreamSession::LogFileContentDescription";
	LOG(INFO) << "-------------------------------------------";

	//Some test code
	LOG(INFO) << "file preview_path: " << fcd->preview_path;
	LOG(INFO) << "file source_path: " << fcd->source_path;
	LOG(INFO) << "file supports_http: " << fcd->supports_http;
	LOG(INFO) << "manifest.GetFileCount: " << fcd->manifest.GetFileCount();
	LOG(INFO) << "manifest.GetFolderCount: " << fcd->manifest.GetFolderCount();
	LOG(INFO) << "manifest.GetImageCount: " << fcd->manifest.GetImageCount();
	LOG(INFO) << "manifest.GetMusicCount: " << fcd->manifest.GetMusicCount();
	LOG(INFO) << "manifest.GetFileCount(item): " << fcd->manifest.GetItemCount(FileShareManifest::T_FILE);
	if(fcd->manifest.GetFileCount() == 1)
	{
		FileShareManifest::Item item = fcd->manifest.item(0);
		LOG(INFO) << "First item type: " << item.type;
		LOG(INFO) << "First item name: " << item.name;
		LOG(INFO) << "First item size: " << item.size;
	}

	LOG(INFO) << "-------------------------------------------";

}

void VueceMediaStreamSession::DoClose(bool terminate) {

	LOG(WARNING) << "VueceMediaStreamSession:DoClose";

	if(terminate)
	{
		LOG(WARNING) << "VueceMediaStreamSession:DoClose - terminate flag is true, session will be terminated";
	}
	else
	{
		LOG(WARNING) << "VueceMediaStreamSession:DoClose - terminate flag is false, session will not be terminated";
	}

	ASSERT(!is_closed_);
	ASSERT(IsComplete());
	ASSERT(NULL != session_);

	is_closed_ = true;

	if (http_client_) {
		LOG(WARNING) << "VueceMediaStreamSession:DoClose - This a http client, call reset()";
		http_client_->reset();
	}

	LOG(WARNING) << "VueceMediaStreamSession:DoClose 2";

	// Currently, CloseAll doesn't result in OnHttpRequestComplete callback.
	// If we change that, the following resetting won't be necessary.
	transfer_connection_id_ = talk_base::HTTP_INVALID_CONNECTION_ID;
	transfer_name_.clear();
	counter_ = NULL;

	LOG(WARNING) << "VueceMediaStreamSession:DoClose 3";

	// 'reset' and 'CloseAll' cause counter_ to clear.
	ASSERT(NULL == counter_);

	if (remote_listener_) {
		// Cache the address for the remote_listener_, so that we can continue to
		// present a consistent URL for remote previews, which is necessary for IE
		// to continue using its cached copy.
		remote_listener_address_ = remote_listener_->GetLocalAddress();
		remote_listener_->Close();
		LOG(WARNING)
			<< "Proxy listener closed @ " << remote_listener_address_.ToString();
	}

	LOG(WARNING) << "VueceMediaStreamSession:DoClose 4";


	if (terminate) {
		LOG(WARNING) << "VueceMediaStreamSession:DoClose - Terminate this session now.";
		session_->Terminate();

		//TODO - We need to sync terminator because we want to let remote client know when exactly
		//this session is terminated and all related resources are release.
	}

	LOG(WARNING) << "VueceMediaStreamSession:DoClose - Done";
}

std::string VueceMediaStreamSession::GetBaseSessionStateString(BaseSession::State  state) const
{

    std::string result;

	switch(state){

	case cricket::Session::STATE_INIT:
	{
		result =  "STATE_INIT";
		return result;
	}
	case cricket::Session::STATE_SENTINITIATE:
	{
		result =  "STATE_SENTINITIATE";
		return result;
	}
	case cricket::Session::STATE_RECEIVEDINITIATE:{
		result =  "STATE_RECEIVEDINITIATE";
		return result;
	}
	case cricket::Session::STATE_SENTACCEPT:{
		result =  "STATE_SENTACCEPT";
		return result;
	}
	case cricket::Session::STATE_RECEIVEDACCEPT:{
		result =  "STATE_RECEIVEDACCEPT";
		return result;
	}
	case cricket::Session::STATE_SENTMODIFY:{
		result =  "STATE_SENTMODIFY";
		return result;
	}
	case cricket::Session::STATE_RECEIVEDMODIFY:{
		result =  "STATE_RECEIVEDMODIFY";
		return result;
	}
	case cricket::Session::STATE_SENTREJECT:{
		result =  "STATE_SENTREJECT";
		return result;
	}
	case cricket::Session::STATE_RECEIVEDREJECT:{
		result =  "STATE_RECEIVEDREJECT";
		return result;
	}
	case cricket::Session::STATE_SENTREDIRECT:{
		result =  "STATE_SENTREDIRECT";
		return result;
	}
	case cricket::Session::STATE_SENTTERMINATE:{
		result =  "STATE_SENTTERMINATE";
		return result;
	}
	case cricket::Session::STATE_RECEIVEDTERMINATE:{
		result =  "STATE_RECEIVEDTERMINATE";
		return result;
	}
	case cricket::Session::STATE_INPROGRESS:{
		result =  "STATE_INPROGRESS";
		return result;
	}
	case cricket::Session::STATE_DEINIT:{
		result =  "STATE_DEINIT";
		return result;
	}
	default:{
		result =  "ERR_UNKNOWN_STATE";
		LOG(LS_ERROR) << "GetBaseSessionStateString: Unknow state value" << state;
		return result;
	}
	}
}


} // namespace cricket
