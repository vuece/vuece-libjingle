/*
 * VueceHttpServerThread.h
 *
 *  Created on: Jun 20, 2013
 *      Author: Jingjing Sun
 */

#include "talk/base/logging.h"

#include "talk/base/httpserver.h"

#include "VueceHttpServerThread.h"


HttpTransactionMsg::HttpTransactionMsg(talk_base::HttpServerTransaction* transaction_)
:transaction (transaction_)
{
}

StreamInterfaceMsg::StreamInterfaceMsg(talk_base::StreamInterface* stream_)
:stream (stream_)
{
}

VueceHttpServerThread::VueceHttpServerThread() :
	worker_thread_(new talk_base::Thread())
{
	http_server_ = new talk_base::HttpServer;

	bStarted = false;
}


void VueceHttpServerThread::Start() {
	LOG(INFO) << "VueceHttpServerThread::Start";
	worker_thread_->Start();
	bStarted = true;
}

void VueceHttpServerThread::Stop() {
	LOG(LS_VERBOSE) << "VueceHttpServerThread::Stop, calling worker_thread_->Stop()";
	worker_thread_->Stop();
	LOG(LS_VERBOSE) << "VueceHttpServerThread::Stop, calling worker_thread_->Stop() - Done";
}

talk_base::HttpServer* VueceHttpServerThread::GetServer()
{
	return http_server_;
}

talk_base::Thread* VueceHttpServerThread::GetWorkThread()
{
	return worker_thread_;
}


VueceHttpServerThread::~VueceHttpServerThread() {

	LOG(LS_VERBOSE) << "VueceHttpServerThread::De-constructor called.";

//	Stop();
//	CloseAll();

//	LOG(LS_VERBOSE) << "VueceHttpServerThread::De-constructor called - Calling worker_thread_->Clear()";
//	worker_thread_->Clear(this);
//	delete worker_thread_;

//	LOG(LS_VERBOSE) << "VueceHttpServerThread::De-constructor called - deleting http_server_";
//	delete http_server_;
//	LOG(LS_VERBOSE) << "VueceHttpServerThread::De-constructor called - Done";
}

void VueceHttpServerThread::Respond(talk_base::HttpServerTransaction* transaction)
{
	LOG(LS_VERBOSE) << "VueceHttpServerThread::Respond";

	HttpTransactionMsg* txnMsg = new HttpTransactionMsg(transaction);
	worker_thread_->Post(this, VUECE_HTTP_SERVERCMD_RESPOND, txnMsg);
}

void VueceHttpServerThread::HandleConnection(talk_base::StreamInterface* stream )
{
	LOG(LS_VERBOSE) << "VueceHttpServerThread::HandleConnection";

	StreamInterfaceMsg * msg = new StreamInterfaceMsg(stream);
	worker_thread_->Post(this, VUECE_HTTP_SERVERCMD_HANDLE_CONNECTION, msg);
}

void VueceHttpServerThread::CloseAll()
{
	LOG(LS_VERBOSE) << "-------------------------------------------------";
	LOG(LS_VERBOSE) << "VueceHttpServerThread::CloseAll";
	LOG(LS_VERBOSE) << "-------------------------------------------------";

	worker_thread_->Post(this, VUECE_HTTP_SERVERCMD_CLOSE_ALL);
}

void VueceHttpServerThread::OnMessage(talk_base::Message *message) {

	ASSERT(worker_thread_->IsCurrent());

	switch (message->message_id) {
		case VUECE_HTTP_SERVERCMD_RESPOND:
		{
			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - VUECE_HTTP_SERVERCMD_RESPOND";

			HttpTransactionMsg* txnMsg = static_cast<HttpTransactionMsg*>(message->pdata);

			talk_base::HttpServerTransaction* transaction =  txnMsg->transaction;

			LOG(LS_VERBOSE) << "VueceHttpServerThread:VUECE_HTTP_SERVERCMD_RESPOND";

			http_server_->Respond(transaction);

			break;
		}
		case VUECE_HTTP_SERVERCMD_HANDLE_CONNECTION:
		{
			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - VUECE_HTTP_SERVERCMD_HANDLE_CONNECTION";

			StreamInterfaceMsg* txnMsg = static_cast<StreamInterfaceMsg*>(message->pdata);

			talk_base::StreamInterface* conn =  txnMsg->stream;

			LOG(LS_VERBOSE) << "VueceHttpServerThread:VUECE_HTTP_SERVERCMD_RESPOND";

			http_server_->HandleConnection(conn);

			break;
		}

		case VUECE_HTTP_SERVERCMD_CLOSE_ALL:
		{
			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - VUECE_HTTP_SERVERCMD_CLOSE_ALL, calling http_server_->CloseAll";

//			http_server_->CloseAll(true);
//
//			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - http_server_->CloseAll returned, delete server instance now.";
//
//			delete http_server_;
//			http_server_ = NULL;

//			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - Clear worker thread.";

//			worker_thread_->Clear(this);

//			worker_thread_ = NULL;

			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - http_server_->CloseAll() returned, notify stream session instance this event.";

			SignalMediaStreamingStopped(0);

//			LOG(LS_VERBOSE) << "VueceHttpServerThread::OnMessage - VUECE_HTTP_SERVERCMD_CLOSE_ALL, calling http_server_->CloseAll, call Stop() now.";
//			Stop();

			break;
		}
	}
}
