
#ifndef VUECE_KERNEL_SHELL_H
#define VUECE_KERNEL_SHELL_H

#include <cstdio>

#include "talk/base/sigslot.h"
#include "talk/p2p/base/constants.h"
#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "VueceNativeInterface.h"

#include "VueceThreadUtil.h"

class VueceCoreClient;

class VueceKernelShell : public talk_base::MessageHandler, public sigslot::has_slots<> {
 public:
  VueceKernelShell(talk_base::Thread *thread, VueceCoreClient *client);
  ~VueceKernelShell();

  virtual void OnMessage(talk_base::Message *msg);

  void Start();
  void Stop();

#ifdef ANDROID
  vuece::VueceResultCode PlayMusic(const std::string &jid, const std::string& song_uuid);
  vuece::VueceResultCode PauseMusic(bool pause_st_check);
  vuece::VueceResultCode ResumeMusic();
  vuece::VueceResultCode SeekMusic(int pos);
#endif

  //event receiver with one string containing all params
  void OnVueceCommandReceived(int, const char*);

  void OnRemoteSessionResourceReleased(const std::string& sid);
  void OnInvalidStreamingTargetMsgReceived();
  void OnPlayerReleased(void);

 private:
  VueceCoreClient *core_client;
  talk_base::Thread *client_thread_;
  talk_base::Thread* console_thread_;
  int timeout_wait_session_released;
  bool session_release_received;

  bool player_release_received;

  JMutex	mutex_wait_session_release;
  JMutex	mutex_wait_player_release;

};

#endif
