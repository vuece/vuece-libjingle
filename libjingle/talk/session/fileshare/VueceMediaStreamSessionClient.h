/*
 * VueceMediaStreamSessionClient.h
 *
 *  Created on: Feb 24, 2013
 *      Author: jingjing
 */

#ifndef VUECE_MEDIA_STREAM_SESSION_CLIENT_H
#define VUECE_MEDIA_STREAM_SESSION_CLIENT_H

#include "talk/base/messagequeue.h"
#include "talk/base/socketpool.h"
#include "talk/base/stringutils.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/xmpp/jid.h"
#include "talk/base/httpserver.h"
#include "talk/session/fileshare/VueceShareCommon.h"
#include "VueceConstants.h"

#ifdef ANDROID
#include "VueceThreadUtil.h"
#endif

namespace cricket {

class VueceMediaStreamSession;
class VueceFileShareSession;
class FileContentDescription;

class VueceMediaStreamSessionClient :  public SessionClient, public sigslot::has_slots<>
{

  friend class VueceMediaStreamSession;

 public:
  VueceMediaStreamSessionClient(SessionManager *sm, const buzz::Jid& jid, const std::string &user_agent);
  ~VueceMediaStreamSessionClient();
  virtual void OnSessionCreate(cricket::Session* session,
                               bool received_initiate);
  virtual void OnSessionDestroy(cricket::Session* session);
  virtual buzz::XmlElement* TranslateSessionDescription(const cricket::FileContentDescription* description);

  VueceMediaStreamSession *CreateMediaStreamSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  const std::string &sender_source_folder,
		  bool bPreviewNeeded,
		  const std::string &actual_file_path,
		  const std::string &actual_preview_file_path,
		  const std::string& start_pos,
		  cricket::FileShareManifest *manifest);

  //NOT USED
//  sigslot::signal1<VueceMediaStreamSession*> SignalFileShareSessionCreate;
//  sigslot::signal1<VueceMediaStreamSession*> SignalFileShareSessionDestroy;

	//jid, session id, state
	sigslot::signal3<const std::string&, const std::string&, int> SignalFileShareStateChanged;

	sigslot::signal2<const std::string&, int> SignalStreamPlayerStateChanged;
	sigslot::signal6<const std::string&, const buzz::Jid&, int, const std::string&, int, bool> SignalFileShareRequestReceived;
	sigslot::signal2<const std::string&, int> SignalFileShareProgressUpdated;
	sigslot::signal2<const std::string&, int> SignalMusicStreamingProgressUpdated;
	sigslot::signal4<const std::string&, const std::string&, int, int> SignalFileSharePreviewReceived;

	sigslot::signal1<VueceStreamPlayerNotificaiontMsg*> SignalStreamPlayerNotification;

	sigslot::signal0<> SignalStreamPlayerReleased;

	void SetDownloadFolder(const std::string &folder);

	void OnSessionState(cricket::VueceMediaStreamSession *sess, cricket::FileShareState state);
	void OnUpdateProgress(cricket::VueceMediaStreamSession *sess);
	void OnResampleImage( cricket::VueceMediaStreamSession* fss, const std::string& path, int width, int height, talk_base::HttpServerTransaction *tran);
	void OnPreviewReceived( cricket::VueceMediaStreamSession* fss, const std::string& path, int w, int h);
	void Accept(const std::string &share_id, const std::string & target_download_folder, const std::string &target_file_name);
	void Cancel(const std::string &share_id);
	void CancelSessionsByJid(const std::string &jid);

	void DestroySession(const std::string &sid);

	void Decline(const std::string &share_id);
	void CancelAllSessions();
	void ListAllSessionIDs();
	int  GetSessionNr();

#ifdef ANDROID
  	void CancelAllSessionsSync(void);
	void StopStreamPlayer(const std::string &share_id);
	void OnStreamPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg);
	void ResumeStreamPlayer(int resume_pos);
	void SeekStream(int posInSec);
	bool IsMusicStreaming(void);
	void OnRemoteSessionResourceReleased(const std::string& sid);
	void HandleStreamPlayerEvent(VueceStreamPlayerEvent state);
	void PassNetworkPlayerNotification(VueceStreamPlayerNotificaiontMsg* msg);
#endif

 private:

  void Construct();

  virtual bool ParseContent(SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const ContentDescription** content,
                            ParseError* error);
  virtual bool WriteContent(SignalingProtocol protocol,
                            const ContentDescription* content,
                            buzz::XmlElement** elem,
                            WriteError* error);

  bool AllowedImageDimensions(size_t width, size_t height);

  bool ParseFileShareContent(const buzz::XmlElement* content_elem,
                               const ContentDescription** content,
                               ParseError* error);

  bool CreatesFileContentDescription(const buzz::XmlElement* element, FileContentDescription* share_desc);

  SessionManager *session_manager_;
  buzz::Jid jid_;
  typedef std::set<cricket::Session*> SessionSet;
  SessionSet sessions_;
  std::string user_agent_;
//  std::map<std::string, VueceMediaStreamSession *> session_map_;
  VueceShareId2SessionMap* session_map_;
  //map: share id -----> session id
  SessionId2VueceShareIdMap* sessionId2VueceShareIdMap;
//  std::map<std::string, std::string> sessionId2VueceShareIdMap;
  int used_sample_rate;
  int used_bit_rate;
  int used_nchannels;
  int used_duration;

  std::string used_target_download_folder;
  std::string used_target_file_name;

  bool is_music_streaming;

  int timeout_wait_session_released;
  bool session_release_received;

  std::string current_active_sid;

#ifdef ANDROID
  JMutex	mutex_wait_session_release;
#endif
};

}// namespace cricket

#endif /* VUECEFILESHARESESSIONCLIENT_H_ */
