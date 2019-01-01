/*
 * VueceMediaStreamSession.h
 *
 *  Created on: Feb 24, 2013
 *      Author: Jingjing Sun
 */


#ifndef _VUECE_MEDIA_STREAM_SESSION_
#define _VUECE_MEDIA_STREAM_SESSION_
#include "talk/base/messagequeue.h"
#include "talk/base/socketpool.h"
#include "talk/base/stringutils.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/session/fileshare/VueceShareCommon.h"
#include "talk/xmpp/jid.h"

class StreamCounter;
class StreamRelay;

namespace talk_base {
  class HttpClient;
  class HttpServer;
  class HttpServerTransaction;
//  struct HttpTransaction;
  class FileStream;
  class VueceMediaStream;
  class Pathname;
}

namespace cricket {

class VueceMediaStreamSession
  : public talk_base::StreamPool,
    public talk_base::MessageHandler,
    public sigslot::has_slots<> {
public:
  struct FileShareDescription : public cricket::SessionDescription {
    FileShareManifest manifest;
    bool supports_http;
    std::string source_path;
    std::string preview_path;
    FileShareDescription() : supports_http(false) { }
  };

  VueceMediaStreamSession(cricket::Session* session, const std::string &user_agent, bool bPreviewNeeded_);
  virtual ~VueceMediaStreamSession();

  bool IsComplete() const;
  bool IsClosed() const;
  FileShareState state() const;
  sigslot::signal2<cricket::VueceMediaStreamSession*, cricket::FileShareState> SignalState;
  sigslot::signal1<cricket::VueceMediaStreamSession*> SignalNextFile;
  sigslot::signal1<cricket::VueceMediaStreamSession*> SignalUpdateProgress;
  sigslot::signal5<cricket::VueceMediaStreamSession*, const std::string&, int, int, talk_base::HttpServerTransaction*> SignalResampleImage;
  sigslot::signal4<cricket::VueceMediaStreamSession*, const std::string&, int, int> SignalPreviewReceived;

  void ResampleComplete(talk_base::StreamInterface *si, talk_base::HttpServerTransaction *trans, bool success);

  bool is_sender() const;
  const buzz::Jid& jid() const;
  const FileShareManifest* manifest() const;
  const std::string& local_folder() const;

  std::string GetStateString(FileShareState state) const;
  std::string GetBaseSessionStateString(BaseSession::State state) const;

//  void SetLocalFolder(const std::string& folder) { local_folder_ = folder; }
  void SetSenderSourceFolder(const std::string& folder) { sender_source_folder = folder; }

  void SetActualPreviewFilePath(const std::string& path){actual_preview_path = path;}
  void SetActualFilePath(const std::string& path){actual_file_path = path;}

  void Share(const buzz::Jid& jid, FileShareManifest* manifest);
  void SetShareId(const std::string& share_id) { share_id_ = share_id; }
  void SetStartPosition(int p){start_pos = p;}
  void Accept(const std::string & target_download_folder, const std::string &target_file_name);
  void Decline();
  void Cancel();
  //request preview if the target file is an image
  void RequestPreview();
  bool PreviewAvailable();

  SessionDescription* CreateAnswer( const FileShareManifest* manifest );
  SessionDescription* CreateOffer( FileShareManifest* manifest );

  bool GetItemUrl(size_t index, std::string* url);
  bool GetImagePreviewUrl(size_t index, size_t width, size_t height,
                          std::string* url);
  // Returns true if the transferring item size is known
  bool GetProgress(size_t& bytes) const;
  // Returns true if the total size is known
  bool GetTotalSize(size_t& bytes) const;
  // Returns true if currently transferring item name is known
  bool GetCurrentItemName(std::string* name);

  const FileShareManifest::Item* GetCurrentItem();

  // TODO: Eliminate this eventually?
  cricket::Session* session() { return session_; }

  // StreamPool Interface
  virtual talk_base::StreamInterface*
    RequestConnectedStream(const talk_base::SocketAddress& remote, int* err);
  virtual void ReturnConnectedStream(talk_base::StreamInterface* stream);

  // MessageHandler Interface
  virtual void OnMessage(talk_base::Message* msg);

  void GetItemNetworkPath(size_t index, bool preview, std::string* path);
  std::string GetShareId() {return share_id_;}
  const std::string& GetSessionId() const;

  void ReleaseAllResourses();

  void OnMediaStreamingStopped(int v);

  void RetrieveUsedMusicAttributes(int* bitrate, int* samplerate, int* nchannels, int* duration);

  void LogHttpErrorString(int code);

private:
  typedef std::list<StreamRelay*> ProxyList;
  typedef std::list<talk_base::HttpServerTransaction*> TransactionList;
  talk_base::StreamInterface* pCurrentPreviewFileStream;

  // Session Signals
//  void OnSessionState(cricket::Session* session, cricket::Session::State state);
  void OnSessionState(BaseSession *session, BaseSession::State state);
  void OnSessionInfoMessage(cricket::Session* session,
                            const XmlElements& els);
  void OnSessionChannelGone(cricket::Session* session,
                            const std::string& name);

  // HttpClient Signals
  void OnHttpClientComplete(talk_base::HttpClient* http, int err);
  void OnHttpClientClosed(talk_base::HttpClient* http, int err);

  // HttpServer Signals
  void OnHttpRequest(talk_base::HttpServer* server,
                     talk_base::HttpServerTransaction* transaction);
  void OnHttpRequestComplete(talk_base::HttpServer* server,
                             talk_base::HttpServerTransaction* transaction,
                             int err);
  void OnHttpConnectionClosed(talk_base::HttpServer* server,
                              int err,
                              talk_base::StreamInterface* stream);

  // TarStream Signals
  void OnNextEntry(const std::string& name, size_t size);

  // Socket Signals
  void OnProxyAccept(talk_base::AsyncSocket* socket);
  void OnProxyClosed(StreamRelay* proxy, int error);

  // StreamCounterSignals
  void OnUpdateBytes(size_t count);

  // Internal Helpers
  void GenerateTemporaryPrefix(std::string* prefix);
  bool GetItemBaseUrl(size_t index, bool preview, std::string* url);
  bool GetProxyAddress(talk_base::SocketAddress& address, bool is_remote);
  talk_base::StreamInterface* CreateChannel(const std::string& channel_name);
  void SendTransportInfo();
  void SendTransportAccept();
  void SetState(FileShareState state, bool prevent_close);
  void OnInitiate();
  void NextDownload();
//  const FileShareDescription* description() const;
  const FileContentDescription* GetFileContentDescription() const;
  void DoClose(bool terminate);
  void LogFileContentDescription(const FileContentDescription* fcd);

  cricket::Session* session_;
  FileShareState state_;
  bool is_closed_;
  bool is_sender_;
  buzz::Jid jid_;
  int start_pos; //start position in second
  std::string share_id_;
  std::string session_id;
  //Note this is the ID of the source file path, not the actual file path
  std::string source_path_;
  //Note this is the ID of the preview file path, not the actual preview file path
  std::string preview_path_;

  //actual source file folder path
//  std::string local_folder_;

  std::string sender_source_folder;

  std::string receiver_target_download_folder;
  std::string receiver_target_download_file_name;



  //actual location of preview file
  std::string actual_preview_path;

  //actual location of preview file
  std::string actual_file_path;

  // The currently active p2p streams to our peer
  talk_base::StreamCache pool_;
  // The http client state (client only)
  talk_base::HttpClient* http_client_;
  // The http server state (server only)
  talk_base::HttpServer* http_server_;
  // The connection id of the currently transferring file (server)
  int transfer_connection_id_;
  // The counter for the currently transferring file
  const StreamCounter* counter_;
  // The number of manifest items that have successfully transferred
  size_t item_transferring_;
  // The byte count of successfully transferred items
  size_t bytes_transferred_;
  // Where the currently transferring item is being (temporarily) saved (client)
  std::string transfer_path_;
  // The name of the currently transferring item
  std::string transfer_name_;
  // Where the files are saved after transfer (client)
  std::vector<std::string> stored_location_;
  // Was it a local cancel?  Or a remote cancel?
  bool local_cancel_;
  // Proxy socket for local IE http requests
  talk_base::AsyncSocket* local_listener_;
  // Proxy socket for remote IE http requests
  talk_base::AsyncSocket* remote_listener_;
  // Cached address of remote_listener_
  talk_base::SocketAddress remote_listener_address_;
  // Uniqueness for channel names
  size_t next_channel_id_;
  // Proxy relays
  ProxyList proxies_;
  std::string user_agent_;
  TransactionList transactions_;

//  const FileShareManifest* iManifest;
  const FileShareManifest* manifest_;

  talk_base::FileStream* currentPreviewFile;
  talk_base::VueceMediaStream *pVueceMediaStream;


  bool bInPreviewMode;
  int iCurrentPreviewW;
  int iCurrentPreviewH;

  //sample rate to be used by vuece media stream to play the data
  int sample_rate;
  int bit_rate;
  int nchannels;
  int duration;

  bool bPreviewNeeded;

  bool bCancelled;

  FileShareManifest::FileType current_file_type;
};

}  // namespace cricket

#endif  // _VUECE_MEDIA_STREAM_SESSION_
