/*
 * VueceFileShareSessionClient.h
 *
 *  Created on: Dec 15, 2011
 *      Author: jingjing
 */

#ifndef VUECEFILESHARESESSIONCLIENT_H_
#define VUECEFILESHARESESSIONCLIENT_H_

#include "talk/base/messagequeue.h"
#include "talk/base/socketpool.h"
#include "talk/base/stringutils.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/xmpp/jid.h"
#include "talk/base/httpserver.h"
#include "talk/session/fileshare/VueceShareCommon.h"

namespace cricket {

class VueceFileShareSession;
class FileContentDescription;
class FileShareManifest;
class VueceMediaStreamSession;

class VueceFileShareSessionClient :  public SessionClient, public sigslot::has_slots<>
{

  friend class VueceFileShareSession;

 public:
  VueceFileShareSessionClient(SessionManager *sm, const buzz::Jid& jid, const std::string &user_agent);
  ~VueceFileShareSessionClient();
  virtual void OnSessionCreate(cricket::Session* session,
                               bool received_initiate);
  virtual void OnSessionDestroy(cricket::Session* session);
  virtual const cricket::SessionDescription* CreateSessionDescription(const buzz::XmlElement* element);
  virtual buzz::XmlElement* TranslateSessionDescription(const cricket::FileContentDescription* description);

  VueceFileShareSession *CreateFileShareSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  const std::string &local_folder,
		  bool bPreviewNeeded,
		  const std::string &actual_preview_file_path,
		  const std::string& start_pos,
		  FileShareManifest *manifest);

  sigslot::signal1<VueceFileShareSession*> SignalFileShareSessionCreate;
  sigslot::signal1<VueceFileShareSession*> SignalFileShareSessionDestroy;

  sigslot::signal2<const std::string&, int> SignalFileShareStateChanged;
  sigslot::signal2<const std::string&, int> SignalStreampLayerStateChanged;
  sigslot::signal5<const std::string&, const buzz::Jid&, const std::string&, int, bool> SignalFileShareRequestReceived;
  sigslot::signal2<const std::string&, int> SignalFileShareProgressUpdated;
  sigslot::signal4<const std::string&, const std::string&, int, int> SignalFileSharePreviewReceived;

  //NOT USED FOR FILE SHARE
//  sigslot::signal2<const std::string&, int> SignalStreamPlayerStateChanged;

  	  void SetDownloadFolder(const std::string &folder);
  
	void OnSessionState(cricket::VueceFileShareSession *sess, cricket::FileShareState state);
	void OnUpdateProgress(cricket::VueceFileShareSession *sess);
	void OnResampleImage( cricket::VueceFileShareSession* fss, const std::string& path, int width, int height, talk_base::HttpServerTransaction *tran);
	void OnPreviewReceived( cricket::VueceFileShareSession* fss, const std::string& path, int w, int h);
	void Accept(const std::string &share_id, int sample_rate);
	void Cancel(const std::string &share_id);
	void Decline(const std::string &share_id);

	  //NOT USED FOR FILE SHARE
	void CancelAllSessions();
	void StopStreamPlayer(const std::string &share_id);
	void ResumeStreamPlayer(const std::string &share_id);
	void SeekStream(int posInSec);
	VueceMediaStreamSession *CreateMediaStreamSessionAsInitiator(
		  const std::string &share_id,
		  const buzz::Jid& targetJid,
		  bool bPreviewNeeded,
		  const std::string &actual_file_path,
		  const std::string &actual_preview_file_path,
		  const std::string& start_pos,
		  cricket::FileShareManifest *manifest);

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
  std::string download_folder;
  // share id -> file share session
  std::map<std::string, VueceFileShareSession *> session_map_;
  // session id -> share id
  std::map<std::string, std::string> sid_share_id_map_;

  int used_sample_rate;
};

}// namespace cricket

#endif /* VUECEFILESHARESESSIONCLIENT_H_ */
