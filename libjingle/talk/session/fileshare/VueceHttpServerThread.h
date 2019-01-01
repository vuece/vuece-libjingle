/*
 * VueceHttpServerThread.h
 *
 *  Created on: Jun 20, 2013
 *      Author: Jingjing Sun
 */

#ifndef VUECEHTTPSERVERTHREAD_H_
#define VUECEHTTPSERVERTHREAD_H_

#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"

enum VueceHttpServerCmd{
	VUECE_HTTP_SERVERCMD_START = 0,
	VUECE_HTTP_SERVERCMD_CLOSE_ALL,
	VUECE_HTTP_SERVERCMD_RESPOND,
	VUECE_HTTP_SERVERCMD_HANDLE_CONNECTION
};

namespace talk_base {
  class HttpServer;
  class HttpServerTransaction;
//  struct HttpTransaction;
  class StreamInterface;
}

struct HttpTransactionMsg : public talk_base::MessageData {
	talk_base::HttpServerTransaction* transaction;
	HttpTransactionMsg(talk_base::HttpServerTransaction* transaction);
};


struct StreamInterfaceMsg : public talk_base::MessageData {
	talk_base::StreamInterface* stream;
	StreamInterfaceMsg(talk_base::StreamInterface* stream);
};


class VueceHttpServerThread : public talk_base::MessageHandler, public sigslot::has_slots<> {


public:
	VueceHttpServerThread();
	~VueceHttpServerThread();

	virtual void OnMessage(talk_base::Message *msg);

	void Start();

	void Stop();

	talk_base::HttpServer* GetServer();

	void Respond(talk_base::HttpServerTransaction* transaction);
	void HandleConnection(talk_base::StreamInterface* stream );

	void CloseAll();

	talk_base::Thread* GetWorkThread();

public:
	  sigslot::signal1<int> SignalMediaStreamingStopped;

private:
	  talk_base::Thread* worker_thread_;
	  talk_base::HttpServer* http_server_;
	  bool bStarted;
};


#endif /* VUECEHTTPSERVERTHREAD_H_ */
