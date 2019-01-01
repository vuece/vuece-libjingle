/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/p2p/base/session.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/helpers.h"
#include "talk/base/stringutils.h"
#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportchannelproxy.h"
#include "talk/p2p/base/p2ptransport.h"
#include "talk/p2p/base/p2ptransportchannel.h"

#include "talk/p2p/base/constants.h"

static bool log_detail;

namespace {

const uint32 MSG_TIMEOUT = 1;
const uint32 MSG_ERROR = 2;
const uint32 MSG_STATE = 3;

}  // namespace

namespace cricket {

bool BadMessage(const buzz::QName type,
                const std::string& text,
                MessageError* err) {
  err->SetType(type);
  err->SetText(text);
  return false;
}

TransportProxy::~TransportProxy() {
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {

	  if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::~TransportProxy: Fire event: SignalDestroyed" ;

    iter->second->SignalDestroyed(iter->second);
    delete iter->second;
  }
  delete transport_;
}

std::string TransportProxy::type() const {
  return transport_->type();
}

TransportChannel* TransportProxy::GetChannel(const std::string& name) {
  return GetProxy(name);
}

TransportChannel* TransportProxy::CreateChannel(
    const std::string& name, const std::string& content_type) {
	if(log_detail)LOG(INFO) << "TransportProxy::CreateChannel with name: " << name << ", session_type_: " << content_type;
  ASSERT(GetChannel(name) == NULL);
  ASSERT(!transport_->HasChannel(name));

  // We always create a proxy in case we need to change out the transport later.
  TransportChannelProxy* channel =
      new TransportChannelProxy(name, content_type);
  channels_[name] = channel;

  if (state_ == STATE_NEGOTIATED) {

	  if(log_detail)LOG(INFO) << "TransportProxy::CreateChannel:STATE_NEGOTIATED:SetProxyImpl";

    SetProxyImpl(name, channel);
  } else if (state_ == STATE_CONNECTING) {
	  if(log_detail)LOG(INFO) << "TransportProxy::CreateChannel:STATE_CONNECTING:GetOrCreateImpl";
    GetOrCreateImpl(name, content_type);
  }
  else
  {
	  LOG(LS_ERROR) << "TransportProxy::CreateChannel:Wrong state value: " << (int)state_;
  }
  return channel;
}

void TransportProxy::DestroyChannel(const std::string& name) {
  TransportChannel* channel = GetChannel(name);
  if (channel) {
    channels_.erase(name);

    if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::DestroyChannel" ;

    channel->SignalDestroyed(channel);
    delete channel;
  }
}

void TransportProxy::SpeculativelyConnectChannels() {
  ASSERT(state_ == STATE_INIT || state_ == STATE_CONNECTING);
  state_ = STATE_CONNECTING;
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    GetOrCreateImpl(iter->first, iter->second->content_type());
  }
  transport_->ConnectChannels();
}

void TransportProxy::CompleteNegotiation() {

//	if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::CompleteNegotiation";

	if (state_ != STATE_NEGOTIATED) {
		state_ = STATE_NEGOTIATED;

//		if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::State is set to: STATE_NEGOTIATED";

		for (ChannelMap::iterator iter = channels_.begin();
				iter != channels_.end(); ++iter) {
//			if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::CompleteNegotiation:SetProxyImpl";
			SetProxyImpl(iter->first, iter->second);
		}

//		if(log_detail)LOG(LS_VERBOSE) << "TransportProxy::CompleteNegotiation:ConnectChannels";

		transport_->ConnectChannels();
	} else {
		LOG(LS_WARNING)
				<< "TransportProxy::CompleteNegotiation:Transport proxy already initaited.";
	}
}

void TransportProxy::AddSentCandidates(const Candidates& candidates) {
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    sent_candidates_.push_back(*cand);
  }
}


TransportChannelProxy* TransportProxy::GetProxy(const std::string& name) {
  ChannelMap::iterator iter = channels_.find(name);
  return (iter != channels_.end()) ? iter->second : NULL;
}

TransportChannelImpl* TransportProxy::GetOrCreateImpl(
    const std::string& name, const std::string& content_type) {

	if(log_detail)LOG(INFO) << "TransportProxy::GetOrCreateImpl:name: " << name << ", content_type: " << content_type;

  TransportChannelImpl* impl = transport_->GetChannel(name);
  if (impl == NULL) {
    impl = transport_->CreateChannel(name, content_type);
  }
  return impl;
}

void TransportProxy::SetProxyImpl(
    const std::string& name, TransportChannelProxy* proxy) {
  TransportChannelImpl* impl = GetOrCreateImpl(name, proxy->content_type());
  ASSERT(impl != NULL);
  proxy->SetImplementation(impl);
}




BaseSession::BaseSession(talk_base::Thread *signaling_thread)
    : state_(STATE_INIT), error_(ERROR_NONE),
      local_description_(NULL), remote_description_(NULL),
      signaling_thread_(signaling_thread) {
}

BaseSession::~BaseSession() {
  delete remote_description_;
  delete local_description_;
}

void BaseSession::SetState(State state) {
  ASSERT(signaling_thread_->IsCurrent());
  if (state != state_) {
    state_ = state;
    SignalState(this, state_);
    signaling_thread_->Post(this, MSG_STATE);
  }
}

void BaseSession::SetError(Error error) {

	  if(log_detail)LOG(LS_VERBOSE) << "Session::SetError: " << (int)error;

  ASSERT(signaling_thread_->IsCurrent());
  if (error != error_) {
    error_ = error;
    SignalError(this, error);
    if (error_ != ERROR_NONE)
      signaling_thread_->Post(this, MSG_ERROR);
  }
}

void BaseSession::OnMessage(talk_base::Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_TIMEOUT:

		LOG(INFO) << "BaseSession::OnMessage - MSG_TIMEOUT";

    // Session timeout has occured.
    SetError(ERROR_TIME);
    break;

  case MSG_ERROR:
    TerminateWithReason(STR_TERMINATE_ERROR);
    break;

  case MSG_STATE:
    switch (state_) {
    case STATE_SENTACCEPT:
    case STATE_RECEIVEDACCEPT:
      SetState(STATE_INPROGRESS);
      break;

    case STATE_SENTREJECT:
    case STATE_RECEIVEDREJECT:
      // Assume clean termination.
      Terminate();
      break;

    default:
      // Explicitly ignoring some states here.
      break;
    }
    break;
  }
}


Session::Session(SessionManager *session_manager,
                 const std::string& local_name,
                 const std::string& initiator_name,
                 const std::string& sid, const std::string& content_type,
                 SessionClient* client) :
    BaseSession(session_manager->signaling_thread()) {
  ASSERT(session_manager->signaling_thread()->IsCurrent());
  ASSERT(client != NULL);
  session_manager_ = session_manager;
  local_name_ = local_name;
  sid_ = sid;
  initiator_name_ = initiator_name;
  content_type_ = content_type;
  // TODO: Once we support different transport types,
  // don't hard code this here.
  transport_type_ = NS_GINGLE_P2P;
  transport_parser_ = new P2PTransportParser();
  client_ = client;
  error_ = ERROR_NONE;
  state_ = STATE_INIT;
  initiator_ = false;
  current_protocol_ = PROTOCOL_HYBRID;

  log_detail = false;
}

Session::~Session() {
  ASSERT(signaling_thread_->IsCurrent());
  ASSERT(state_ != STATE_DEINIT);

 if(log_detail)LOG(LS_VERBOSE) << "Session::De-Constructor is called.";

	 state_ = STATE_DEINIT;
  SignalState(this, state_);

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    delete iter->second;
  }

  delete transport_parser_;
}

Transport* Session::GetTransport(const std::string& content_name) {

	  if(log_detail)LOG(INFO) << "Session::GetTransport:Content name: " << content_name;

  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
    return NULL;
  return transproxy->impl();
}

void Session::set_allow_local_ips(bool allow) {

	 if(log_detail)LOG(INFO) << "Session::set_allow_local_ips: " << allow;

  allow_local_ips_ = allow;
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->impl()->set_allow_local_ips(allow);
  }
}

bool Session::Initiate(const std::string &to,
                       const SessionDescription* sdesc) {
  ASSERT(signaling_thread_->IsCurrent());
  SessionError error;
  const ContentInfos& contents = sdesc->contents();

  if(log_detail)LOG(INFO) << "Session::Initiate";

  // Only from STATE_INIT
  if (state_ != STATE_INIT)
    return false;

  // Setup for signaling.
  remote_name_ = to;
  initiator_ = true;
  set_local_description(sdesc);

  //NOTE: jj - we do a hack here to get the content type of content description, if it's file share session
  // then we set current protocol to GINGLE because we only use GINGLE format for file share
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); content++) {
    if (content->type == NS_GOOGLE_SHARE) {
    	  if(log_detail)LOG(INFO) << "Session::Initiate:This is a file share session, we only use GINGLE format for file share.";
    	  current_protocol_ = PROTOCOL_GINGLE;
    }
  }

  if(log_detail)LOG(INFO) << "Session::Initiate::CreateTransportProxies";

  if (!CreateTransportProxies(GetEmptyTransportInfos(sdesc->contents()),
                              &error)) {
    LOG(LS_ERROR) << "Could not create transports: " << error.text;
    return false;
  }

  if (!SendInitiateMessage(sdesc, &error)) {
    LOG(LS_ERROR) << "Could not send initiate message: " << error.text;
    return false;
  }

  SetState(Session::STATE_SENTINITIATE);

  SpeculativelyConnectAllTransportChannels();
  return true;
}

bool Session::Accept(const SessionDescription* sdesc) {
  ASSERT(signaling_thread_->IsCurrent());

  // Only if just received initiate
  if (state_ != STATE_RECEIVEDINITIATE)
    return false;

  // Setup for signaling.
  initiator_ = false;
  set_local_description(sdesc);

  SessionError error;
  if (!SendAcceptMessage(sdesc, &error)) {
    LOG(LS_ERROR) << "Could not send accept message: " << error.text;
    return false;
  }

  SetState(Session::STATE_SENTACCEPT);
  return true;
}

bool Session::Reject(const std::string& reason) {
  ASSERT(signaling_thread_->IsCurrent());

  // Reject is sent in response to an initiate or modify, to reject the
  // request
  LOG(LS_WARNING) << "Session::Reject: state =  " << state_;

  if (state_ != STATE_RECEIVEDINITIATE && state_ != STATE_RECEIVEDMODIFY)
  {
	  LOG(LS_WARNING) << "Session::Reject: state is not STATE_RECEIVEDINITIATE and STATE_RECEIVEDMODIFY, return false";

	    return false;
  }


  // Setup for signaling.
  initiator_ = false;

  SessionError error;

  LOG(LS_WARNING) << "Session::Reject:Rejecting with reason: " << reason;

  if (!SendRejectMessage(reason, &error)) {
    LOG(LS_ERROR) << "Could not send reject message: " << error.text;
    return false;
  }

  SetState(STATE_SENTREJECT);
  return true;
}

bool Session::TerminateWithReason(const std::string& reason) {

  LOG(LS_VERBOSE) << "Session::TerminateWithReason - " << reason	 << ", current state is: " << state_;

  ASSERT(signaling_thread_->IsCurrent());

  if(state_ == STATE_INPROGRESS)
  {
	  LOG(LS_VERBOSE) << "Session::TerminateWithReason - Session is INPROGRESS but terminated locally.";
  }

  // Either side can terminate, at any time.
  switch (state_) {

    case STATE_SENTTERMINATE:
    {
    	LOG(LS_VERBOSE) << "Session::TerminateWithReason - Current state is SENTTERMINATE, so no further action needed.";

        return false;
    }
    case STATE_RECEIVEDTERMINATE:
    {
    	LOG(LS_VERBOSE) << "Session::TerminateWithReason - Current state is RECEIVEDTERMINATE, so no further action needed.";

        return false;
    }


    case STATE_SENTREJECT:
    case STATE_RECEIVEDREJECT:
      // We don't need to send terminate if we sent or received a reject...
      // it's implicit.
      break;

    default:
      SessionError error;

      LOG(LS_VERBOSE) << "Session::TerminateWithReason - Calling SendTerminateMessage()";

      if (!SendTerminateMessage(reason, &error)) {
        LOG(LS_ERROR) << "Session::TerminateWithReason - Could not send terminate message: " << error.text;
        return false;
      }

      break;
  }

  LOG(LS_VERBOSE) << "Session::TerminateWithReason - Set current state to STATE_SENTTERMINATE";

  /*
   * Note(jj) - This set method will trigger a notification signal to session manager,
   * it will cause current session to be destroyed, see Session::OnMessage and
   * SessionManager::DestroySession
   */
  SetState(STATE_SENTTERMINATE);
  return true;
}

void Session::VueceForceTerminateMessage()
{
    	SessionError error;
    	if(log_detail)LOG(LS_VERBOSE) << "Session::VueceForceTerminateMessage";

        if (!SendTerminateMessage(STR_TERMINATE_SUCCESS, &error)) {
          LOG(LS_ERROR) << "Session::VueceForceTerminateMessage - Could not send terminate message: " << error.text;
         // return false;
        }
}

bool Session::SendInfoMessage(const XmlElements& elems) {
  ASSERT(signaling_thread_->IsCurrent());
  SessionError error;
  if (!SendMessage(ACTION_SESSION_INFO, elems, &error)) {
    LOG(LS_ERROR) << "Could not send info message " << error.text;
    return false;
  }
  return true;
}

//added by VUece
bool Session::SendTransportAcceptMessage(const XmlElements& elems) {
  ASSERT(signaling_thread_->IsCurrent());

  if(log_detail)LOG(INFO) << "Session::SendTransportAcceptMessage";

  SessionError error;
  if (!SendMessage(ACTION_TRANSPORT_ACCEPT, elems, &error)) {
    LOG(LS_ERROR) << "Could not send transport accept message " << error.text;
    return false;
  }
  return true;
}



TransportProxy* Session::GetTransportProxy(const Transport* transport) {

	  if(log_detail)LOG(INFO) << "Session::GetTransportProxy";

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    TransportProxy* transproxy = iter->second;
    if (transproxy->impl() == transport) {
      return transproxy;
    }
  }

  LOG(LS_ERROR) << "Session::GetTransportProxy:Will return NULL.";

  return NULL;
}

TransportProxy* Session::GetTransportProxy(const std::string& content_name) {
	  if(log_detail)LOG(INFO) << "Session::GetTransportProxy:content_name: " << content_name;
  TransportMap::iterator iter = transports_.find(content_name);
  return (iter != transports_.end()) ? iter->second : NULL;
}

TransportProxy* Session::GetFirstTransportProxy() {
  if (transports_.empty())
    return NULL;
  return transports_.begin()->second;
}

TransportInfos Session::GetEmptyTransportInfos(
    const ContentInfos& contents) const {
  TransportInfos tinfos;
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    tinfos.push_back(
        TransportInfo(content->name, transport_type_, Candidates()));
  }
  return tinfos;
}


bool Session::OnRemoteCandidates(const TransportInfos& tinfos,
		ParseError* error) {

	if(log_detail)LOG(INFO) << "Session::OnRemoteCandidates";

	for (TransportInfos::const_iterator tinfo = tinfos.begin();
			tinfo != tinfos.end(); ++tinfo) {

		if(log_detail)LOG(INFO)
				<< "Session::OnRemoteCandidates:Iterating transport infos: "
				<< tinfo->content_name;

		TransportProxy* transproxy = GetTransportProxy(tinfo->content_name);

		if (transproxy == NULL) {
			LOG(LS_ERROR)
					<< "Session::OnRemoteCandidates:Cannot find transport proxy with content name: "
					<< tinfo->content_name;
			return BadParse("Unknown content name: " + tinfo->content_name,
					error);
		}

		// Must complete negotiation before sending remote candidates, or
		// there won't be any channel impls.
		if(log_detail)LOG(INFO) << "Session::OnRemoteCandidates:Complete negotiation";

		transproxy->CompleteNegotiation();

		if(log_detail)LOG(INFO) << "Session::OnRemoteCandidates:Number of candidates: " << tinfo->candidates.size();

		if (tinfo->candidates.size() == 0) {
			LOG(LS_WARNING)
					<< "Session::OnRemoteCandidates:Candidate list is empty.";
		}

		for (Candidates::const_iterator cand = tinfo->candidates.begin();
				cand != tinfo->candidates.end(); ++cand) {

			if(log_detail)LOG(INFO)
					<< "Session::OnRemoteCandidates:VerifyCandidate - name = "
					<< cand->name();

			if (!transproxy->impl()->VerifyCandidate(*cand, error))
			{
				LOG(LS_ERROR)
						<< "Session::OnRemoteCandidates:VerifyCandidate:Retruned false.";
				return false;
			}

			if(log_detail)LOG(INFO) << "Session::OnRemoteCandidates - Checking candidate: " << cand->name();

			if (!transproxy->impl()->HasChannel(cand->name()))
			{
				//cccccccccccccc
				buzz::XmlElement* extra_info = new buzz::XmlElement(
						QN_GINGLE_P2P_UNKNOWN_CHANNEL_NAME);

				extra_info->AddAttr(buzz::QN_NAME, cand->name());

				error->extra = extra_info;

				return BadParse(
						"channel named in candidate does not exist: "
								+ cand->name() + " for content: "
								+ tinfo->content_name, error);
			}
		}

		transproxy->impl()->OnRemoteCandidates(tinfo->candidates);
	}

	return true;
}


TransportProxy* Session::GetOrCreateTransportProxy(
    const std::string& content_name) {

  if(log_detail)LOG(INFO) << "Session::GetOrCreateTransportProxy:content_name = " << content_name;

  TransportProxy* transproxy = GetTransportProxy(content_name);

  if (transproxy)
  {
	  if(log_detail)LOG(INFO) << "Session::GetOrCreateTransportProxy:transport proxy with this content name already exits, return now";
	    return transproxy;
  }

  if(log_detail)LOG(INFO) << "Session::GetOrCreateTransportProxy:Create new proxy on demand.";

  Transport* transport =
      new P2PTransport(signaling_thread_,
                       session_manager_->worker_thread(),
                       session_manager_->port_allocator());
  transport->set_allow_local_ips(allow_local_ips_);
  transport->SignalConnecting.connect(
      this, &Session::OnTransportConnecting);
  transport->SignalWritableState.connect(
      this, &Session::OnTransportWritable);
  transport->SignalRequestSignaling.connect(
      this, &Session::OnTransportRequestSignaling);
  transport->SignalCandidatesReady.connect(
      this, &Session::OnTransportCandidatesReady);
  transport->SignalTransportError.connect(
      this, &Session::OnTransportSendError);
  transport->SignalChannelGone.connect(
      this, &Session::OnTransportChannelGone);

  transproxy = new TransportProxy(content_name, transport);
  transports_[content_name] = transproxy;

  if(log_detail)LOG(INFO) << "Session::GetOrCreateTransportProxy:Transport proxy created with content name: " << content_name;

  return transproxy;
}

bool Session::CreateTransportProxies(const TransportInfos& tinfos,
                                     SessionError* error) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    if (tinfo->transport_type != transport_type_) {
      error->SetText("No supported transport in offer.");
      return false;
    }

    GetOrCreateTransportProxy(tinfo->content_name);
  }
  return true;
}

void Session::SpeculativelyConnectAllTransportChannels() {
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->SpeculativelyConnectChannels();
  }
}

TransportParserMap Session::GetTransportParsers() {
  TransportParserMap parsers;
  parsers[transport_type_] = transport_parser_;
  return parsers;
}

ContentParserMap Session::GetContentParsers() {
  ContentParserMap parsers;
  parsers[content_type_] = client_;
  return parsers;
}

TransportChannel* Session::CreateChannel(const std::string& content_name,
                                         const std::string& channel_name) {
  // We create the proxy "on demand" here because we need to support
  // creating channels at any time, even before we send or receive
  // initiate messages, which is before we create the transports.
	if(log_detail)LOG(INFO) << "Session::CreateChannel:content_name: " << content_name << ", channel_name: " << channel_name;
	if(log_detail)LOG(INFO) << "Session::CreateChannel:content_type_: " << content_type_;
  TransportProxy* transproxy = GetOrCreateTransportProxy(content_name);
  return transproxy->CreateChannel(channel_name, content_type_);
}

TransportChannel* Session::GetChannel(const std::string& content_name,
                                      const std::string& channel_name) {
	if(log_detail)LOG(INFO) << "Session::GetChannel:Looking for channel with content name: " << content_name << ", channel name: " << channel_name;
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
  {
	  LOG(LS_ERROR) << "Session::GetChannel:Cannot find this channel, return NULL";
	  return NULL;
  }

  else
  {
	  if(log_detail)LOG(INFO) << "Session::GetChannel:Channel found.";
	  return transproxy->GetChannel(channel_name);
  }

}

void Session::DestroyChannel(const std::string& content_name,
                             const std::string& channel_name) {
	if(log_detail)LOG(INFO) << "Session::DestroyChannel:Looking for channel with content name: " << content_name << ", channel name: " << channel_name;
  TransportProxy* transproxy = GetTransportProxy(content_name);
  ASSERT(transproxy != NULL);
  transproxy->DestroyChannel(channel_name);
}

void Session::OnSignalingReady() {

	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnSignalingReady A";

  ASSERT(signaling_thread_->IsCurrent());

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {

	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnSignalingReady B";
    iter->second->impl()->OnSignalingReady();
  }
}

void Session::OnTransportConnecting(Transport* transport) {
  // This is an indication that we should begin watching the writability
  // state of the transport.
  OnTransportWritable(transport);
}

void Session::OnTransportWritable(Transport* transport) {
  ASSERT(signaling_thread_->IsCurrent());

  LOG(LS_VERBOSE) << "Session::OnTransportWritable";

  // If the transport is not writable, start a timer to make sure that it
  // becomes writable within a reasonable amount of time.  If it does not, we
  // terminate since we can't actually send data.  If the transport is writable,
  // cancel the timer.  Note that writability transitions may occur repeatedly
  // during the lifetime of the session.
  signaling_thread_->Clear(this, MSG_TIMEOUT);
  if (transport->HasChannels() && !transport->writable()) {
    signaling_thread_->PostDelayed(
        session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);
  }
}

void Session::OnTransportRequestSignaling(Transport* transport) {
  ASSERT(signaling_thread_->IsCurrent());
  SignalRequestSignaling(this);
}

void Session::OnTransportCandidatesReady(Transport* transport,
                                         const Candidates& candidates) {
  ASSERT(signaling_thread_->IsCurrent());
  //if(log_detail)LOG(LS_VERBOSE) << "Session::OnTransportCandidatesReady";
  TransportProxy* transproxy = GetTransportProxy(transport);
  if (transproxy != NULL) {
    if (!transproxy->negotiated()) {
      transproxy->AddSentCandidates(candidates);
    }
    SessionError error;
    if (!SendTransportInfoMessage(
            TransportInfo(transproxy->content_name(), transproxy->type(),
                          candidates),
            &error)) {
      LOG(LS_ERROR) << "Could not send transport info message: "
                    << error.text;
      return;
    }
  }
}

void Session::OnTransportSendError(Transport* transport,
                                   const buzz::XmlElement* stanza,
                                   const buzz::QName& name,
                                   const std::string& type,
                                   const std::string& text,
                                   const buzz::XmlElement* extra_info) {
	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnTransportSendError";
  ASSERT(signaling_thread_->IsCurrent());
  SignalErrorMessage(this, stanza, name, type, text, extra_info);
}

void Session::OnTransportChannelGone(Transport* transport,
                                     const std::string& name) {

	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnTransportChannelGone";
  ASSERT(signaling_thread_->IsCurrent());
  SignalChannelGone(this, name);
}

void Session::OnIncomingMessage(const SessionMessage& msg) {
  ASSERT(signaling_thread_->IsCurrent());

  if(log_detail)LOG(LS_INFO) << "Session::OnIncomingMessage:state = " << state_ << ", msg.from = " << msg.from << ", remote name = " << remote_name_ ;

  //NOTE this is probably a bug:
  //gmail user account is case insensitive when logging in, so msg.from == remote_name_ will
  //return false if the case doesn't match, this can happen when user signs in with for example,
  //upper case account name, but this user created his account name with lower case characters,
  //gtalk server always returns original account name, which doesn't match the account name used
  //during sign in - We should ignore case here!
  ASSERT(state_ == STATE_INIT || talk_base::ascicmp(msg.from.c_str(),remote_name_.c_str())==0);
  //  ASSERT(state_ == STATE_INIT || msg.from == remote_name_);

  if (current_protocol_== PROTOCOL_HYBRID) {
    if (msg.protocol == PROTOCOL_GINGLE) {
      current_protocol_ = PROTOCOL_GINGLE;
    } else {
      current_protocol_ = PROTOCOL_JINGLE;
    }
  }

  bool valid = false;
  MessageError error;
  switch (msg.type) {
    case ACTION_SESSION_INITIATE:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_SESSION_INITIATE";
      valid = OnInitiateMessage(msg, &error);
      break;
    case ACTION_SESSION_INFO:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_SESSION_INFO";
      valid = OnInfoMessage(msg);
      break;
    case ACTION_SESSION_ACCEPT:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_SESSION_ACCEPT";
      valid = OnAcceptMessage(msg, &error);
      break;
    case ACTION_SESSION_REJECT:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_SESSION_REJECT";
      valid = OnRejectMessage(msg, &error);
      break;
    case ACTION_SESSION_TERMINATE:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_SESSION_TERMINATE";
      valid = OnTerminateMessage(msg, &error);
      break;
    case ACTION_TRANSPORT_INFO:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_TRANSPORT_INFO";
      valid = OnTransportInfoMessage(msg, &error);
      break;
    case ACTION_TRANSPORT_ACCEPT:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_TRANSPORT_ACCEPT";
      valid = OnTransportAcceptMessage(msg, &error);
      break;
    case ACTION_NOTIFY:
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_NOTIFY";
    case ACTION_UPDATE:
      // TODO: Process these non-standard messages, but
      // only once we figure out how in a jingle-specific way (or
      // remove the need altogether).  For now, just don't send an
      // error back, because it disrupts call establishment.
    	//if(log_detail)LOG(LS_VERBOSE) << "Session::OnIncomingMessage - msg.type is ACTION_UPDATE";
      valid = true;
      break;
    default:
      valid = BadMessage(buzz::QN_STANZA_BAD_REQUEST,
                         "unknown session message type",
                         &error);
  }

  if (valid) {
    SendAcknowledgementMessage(msg.stanza);
  } else {
    SignalErrorMessage(this, msg.stanza, error.type,
                       "modify", error.text, NULL);
  }
}

void Session::OnFailedSend(const buzz::XmlElement* orig_stanza,
                           const buzz::XmlElement* error_stanza) {
  ASSERT(signaling_thread_->IsCurrent());

  SessionMessage msg;
  ParseError parse_error;

  LOG(LS_ERROR)  << "Session - OnFailedSend";

  if (!ParseSessionMessage(orig_stanza, &msg, &parse_error)) {
    LOG(LS_ERROR) << "Error parsing failed send: " << parse_error.text
                  << ":" << orig_stanza;
    return;
  }

  // If the error is a session redirect, call OnRedirectError, which will
  // continue the session with a new remote JID.
  SessionRedirect redirect;
  if (FindSessionRedirect(error_stanza, &redirect)) {
    SessionError error;
    if (!OnRedirectError(redirect, &error)) {
      // TODO: Should we send a message back?  The standard
      // says nothing about it.
      LOG(LS_ERROR) << "Failed to redirect: " << error.text;
      SetError(ERROR_RESPONSE);
    }
    return;
  }

  std::string error_type = "cancel";

  const buzz::XmlElement* error = error_stanza->FirstNamed(buzz::QN_ERROR);
  ASSERT(error != NULL);
  if (error) {

	  std::string error_channel_not_found = "channel named in candidate does not exist";

	  std::string error_str = error->Str();

    ASSERT(error->HasAttr(buzz::QN_TYPE));
    error_type = error->Attr(buzz::QN_TYPE);

    if(log_detail)LOG(LS_VERBOSE) << "OnFailedSend - error type is: " << error_type;

    LOG(LS_ERROR) << "Session error:\n" << error_str << "\n"
                  << "in response to:\n" << orig_stanza->Str();

    if (error_str.find(error_channel_not_found) != std::string::npos) {
    	LOG(LS_ERROR) << "Find a channel not exist error but try to ignore it.";
        SetError(ERROR_NONE);
    }
    else
    {

    //Added by JJ - This case happens if remote player is killed during
    //streaming, TerminateWithReason() is used to trigger local
    //termination.
    if(log_detail)LOG(LS_VERBOSE) << "------------------------------------------------";
    LOG(LS_ERROR) << "Error response received, terminate session now!!!";
    if(log_detail)LOG(LS_VERBOSE) << "------------------------------------------------";

    //TODO jj - More work is needed here, this results in a normal termination in stream
    //session and we cannot know the actual reason of the termination, we need another
    //way to tell vuece stream session that the session has been terminated because of an
    //error response XXXXXXXXXXXXXXXXXXXXXX

    LOG(LS_ERROR) << "Set last error to ERROR_RESPONSE";

    SetError(ERROR_RESPONSE);
    TerminateWithReason(STR_TERMINATE_ERROR);
    }
  }

  if (msg.type == ACTION_TRANSPORT_INFO) {
    // Transport messages frequently generate errors because they are sent right
    // when we detect a network failure.  For that reason, we ignore such
    // errors, because if we do not establish writability again, we will
    // terminate anyway.  The exceptions are transport-specific error tags,
    // which we pass on to the respective transport.

    // TODO: This is only used for unknown channel name.
    // For Jingle, find a stanard-compliant way of doing this.  For
    // Gingle, guess the content name based on the channel name.
    for (const buzz::XmlElement* elem = error->FirstElement();
         NULL != elem; elem = elem->NextElement()) {
      TransportProxy* transproxy = GetFirstTransportProxy();
      if (transproxy && transproxy->type() == error->Name().Namespace()) {
        transproxy->impl()->OnTransportError(elem);
      }
    }
  } else if ((error_type != "continue") && (error_type != "wait")) {
    // We do not set an error if the other side said it is okay to continue
    // (possibly after waiting).  These errors can be ignored.
    SetError(ERROR_RESPONSE);
  }
}

bool Session::OnInitiateMessage(const SessionMessage& msg,
                                MessageError* error) {

	if(log_detail)LOG(INFO) << "Session::OnInitiateMessage";

  if (!CheckState(STATE_INIT, error))
    return false;

  SessionInitiate init;

  //if(log_detail)LOG(LS_VERBOSE) << "------------------ Session::ParseSessionInitiate";

  if (!ParseSessionInitiate(msg.protocol, msg.action_elem,
                            GetContentParsers(), GetTransportParsers(),
                            &init, error))
  {
	  LOG(LS_ERROR) << "Session::ParseSessionInitiate:Failed.";
	  return false;
  }


  SessionError session_error;

  //if(log_detail)LOG(LS_VERBOSE) << "------------------ Session::CreateTransportProxies";

  if (!CreateTransportProxies(init.transports, &session_error)) {

	  LOG(LS_ERROR) << "Session::CreateTransportProxies:Failed.";

    return BadMessage(buzz::QN_STANZA_NOT_ACCEPTABLE,
                      session_error.text, error);
  }

  initiator_ = false;
  remote_name_ = msg.from;

  //if(log_detail)LOG(LS_VERBOSE) << "Session::OnInitiateMessage:Call set_remote_description()";

  SessionDescription* s =	new SessionDescription(init.ClearContents());

  const ContentInfo* ci = s->FirstContentByType(NS_GOOGLE_SHARE);

  if(ci)
  {
	  if(log_detail)LOG(INFO) << "Got file share content";
	  if(log_detail)LOG(INFO) << "name: " << ci->name;
	  if(log_detail)LOG(INFO) << "type: " << ci->type;

//	  const ContentDescription* contentDesc = ci->description;

  }

  set_remote_description(s);
  SetState(STATE_RECEIVEDINITIATE);

  // Users of Session may listen to state change and call Reject().
  if (state_ != STATE_SENTREJECT) {
    if (!OnRemoteCandidates(init.transports, error))
      return false;
  }
  return true;
}

bool Session::OnAcceptMessage(const SessionMessage& msg, MessageError* error) {

	if(log_detail)LOG(INFO) << "Session::OnAcceptMessage:protocol: " << msg.protocol;

  if (!CheckState(STATE_SENTINITIATE, error))
    return false;

  SessionAccept accept;
  if (!ParseSessionAccept(msg.protocol, msg.action_elem,
                          GetContentParsers(), GetTransportParsers(),
                          &accept, error))
    return false;

  //if(log_detail)LOG(LS_VERBOSE) << "Session::OnAcceptMessage:Call set_remote_description()";

  set_remote_description(new SessionDescription(accept.ClearContents()));
  SetState(STATE_RECEIVEDACCEPT);

  // Users of Session may listen to state change and call Reject().
  if (state_ != STATE_SENTREJECT) {
    if (!OnRemoteCandidates(accept.transports, error))
      return false;
  }

  return true;
}

bool Session::OnRejectMessage(const SessionMessage& msg, MessageError* error) {

	if(log_detail)LOG(INFO) << "Session::OnRejectMessage";

  if (!CheckState(STATE_SENTINITIATE, error))
    return false;

  SetState(STATE_RECEIVEDREJECT);
  return true;
}

// Only used by app/win32/fileshare.cc.
bool Session::OnInfoMessage(const SessionMessage& msg) {
  SignalInfoMessage(this, CopyOfXmlChildren(msg.action_elem));
  return true;
}

bool Session::OnTerminateMessage(const SessionMessage& msg,
                                 MessageError* error) {
  SessionTerminate term;
  if (!ParseSessionTerminate(msg.protocol, msg.action_elem, &term, error))
    return false;

  SignalReceivedTerminateReason(this, term.reason);
  if (term.debug_reason != buzz::STR_EMPTY) {
    if(log_detail)LOG(LS_VERBOSE) << "Received error on call: " << term.debug_reason;
  }

  SetState(STATE_RECEIVEDTERMINATE);
  return true;
}

bool Session::OnTransportInfoMessage(const SessionMessage& msg,
                                     MessageError* error) {

	  //if(log_detail)LOG(LS_VERBOSE) << "FileShareSession::OnTransportInfoMessage";

  TransportInfos tinfos;

  //if(log_detail)LOG(LS_VERBOSE) << "FileShareSession::OnTransportInfoMessage:ParseTransportInfos";

  if (!ParseTransportInfos(msg.protocol, msg.action_elem,
                           initiator_description()->contents(),
                           GetTransportParsers(), &tinfos, error))
    return false;

  //if(log_detail)LOG(LS_VERBOSE) << "FileShareSession::OnTransportInfoMessage:OnRemoteCandidates";

  if (!OnRemoteCandidates(tinfos, error))
    return false;

  return true;
}

bool Session::OnTransportAcceptMessage(const SessionMessage& msg,
                                       MessageError* error) {
  // TODO: Currently here only for compatibility with
  // Gingle 1.1 clients (notably, Google Voice).

	  //if(log_detail)LOG(LS_VERBOSE) << "FileShareSession::OnTransportAcceptMessage";

  return true;
}

bool BareJidsEqual(const std::string& name1,
                   const std::string& name2) {
  buzz::Jid jid1(name1);
  buzz::Jid jid2(name2);

  return jid1.IsValid() && jid2.IsValid() && jid1.BareEquals(jid2);
}

bool Session::OnRedirectError(const SessionRedirect& redirect,
                              SessionError* error) {
  MessageError message_error;
  if(log_detail)LOG(LS_VERBOSE) << "Session::OnRedirectError";

  if (!CheckState(STATE_SENTINITIATE, &message_error)) {
    return BadWrite(message_error.text, error);
  }

  if (!BareJidsEqual(remote_name_, redirect.target))
    return BadWrite("Redirection not allowed: must be the same bare jid.",
                    error);

  // When we receive a redirect, we point the session at the new JID
  // and resend the candidates.
  remote_name_ = redirect.target;
  return (SendInitiateMessage(local_description(), error) &&
          ResendAllTransportInfoMessages(error));
}

bool Session::CheckState(State state, MessageError* error) {
	if(log_detail)LOG(INFO) << "CheckState current state = " << state_ << ", state =  " << state;
   ASSERT(state_ == state);
  if (state_ != state) {
	  LOG(LS_ERROR) << "Session::CheckState - Message not allowed in current state, current state = " << state_ << ", should be: " << state;
    return BadMessage(buzz::QN_STANZA_NOT_ALLOWED,
                      "message not allowed in current state",
                      error);
  }
  return true;
}

void Session::OnMessage(talk_base::Message *pmsg) {
  // preserve this because BaseSession::OnMessage may modify it
  BaseSession::State orig_state = state_;

  BaseSession::OnMessage(pmsg);

  if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - id = " << pmsg->message_id;

  switch (pmsg->message_id) {
  case MSG_STATE:
    switch (orig_state) {
    case STATE_SENTTERMINATE:
    {
    	//TEST CODE!!!
//    	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - This is a STATE_SENTTERMINATE notify, skip destroying session to see what will happen...";
  	      if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - ******************************************************************";
    	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - This is a STATE_SENTTERMINATE notify, destroy current session now!";
    	  if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - ******************************************************************";
    	  //ORIGINAL CODE WILL TRY TO DESTROY SESSION
          session_manager_->DestroySession(this);
    	break;
    }
    case STATE_RECEIVEDTERMINATE:
    {
    	 if(log_detail)LOG(LS_VERBOSE) << "Session::OnMessage - This is a STATE_RECEIVEDTERMINATE notify, destroy current session now!";
        session_manager_->DestroySession(this);
        break;
    }

    default:
      // Explicitly ignoring some states here.
      break;
    }
    break;
  }
}

bool Session::SendInitiateMessage(const SessionDescription* sdesc,
                                  SessionError* error) {
  SessionInitiate init;
  init.contents = sdesc->contents();
  init.transports = GetEmptyTransportInfos(init.contents);
  return SendMessage(ACTION_SESSION_INITIATE, init, error);
}

bool Session::WriteSessionAction(
    SignalingProtocol protocol, const SessionInitiate& init,
    XmlElements* elems, WriteError* error) {
  ContentParserMap content_parsers = GetContentParsers();
  TransportParserMap trans_parsers = GetTransportParsers();

  return WriteSessionInitiate(protocol, init.contents, init.transports,
                              content_parsers, trans_parsers,
                              elems, error);
}

bool Session::SendAcceptMessage(const SessionDescription* sdesc,
                                SessionError* error) {
  XmlElements elems;
  if (!WriteSessionAccept(current_protocol_,
                          sdesc->contents(),
                          GetEmptyTransportInfos(sdesc->contents()),
                          GetContentParsers(), GetTransportParsers(),
                          &elems, error)) {
    return false;
  }
  return SendMessage(ACTION_SESSION_ACCEPT, elems, error);
}

bool Session::SendRejectMessage(const std::string& reason,
                                SessionError* error) {
  XmlElements elems;
  return SendMessage(ACTION_SESSION_REJECT, elems, error);
}


bool Session::SendTerminateMessage(const std::string& reason,
                                   SessionError* error) {
  SessionTerminate term(reason);

  if(log_detail)LOG(LS_VERBOSE) << "Session::SendTerminateMessage";

  return SendMessage(ACTION_SESSION_TERMINATE, term, error);
}

bool Session::WriteSessionAction(SignalingProtocol protocol,
                                 const SessionTerminate& term,
                                 XmlElements* elems, WriteError* error) {
  WriteSessionTerminate(protocol, term, elems);
  return true;
}

bool Session::SendTransportInfoMessage(const TransportInfo& tinfo,
                                       SessionError* error) {
  return SendMessage(ACTION_TRANSPORT_INFO, tinfo, error);
}

bool Session::WriteSessionAction(SignalingProtocol protocol,
                                 const TransportInfo& tinfo,
                                 XmlElements* elems, WriteError* error) {
  TransportInfos tinfos;
  tinfos.push_back(tinfo);
  TransportParserMap parsers = GetTransportParsers();

  return WriteTransportInfos(protocol, tinfos, parsers,
                             elems, error);
}

bool Session::ResendAllTransportInfoMessages(SessionError* error) {
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    TransportProxy* transproxy = iter->second;
    if (transproxy->sent_candidates().size() > 0) {
      if (!SendTransportInfoMessage(
              TransportInfo(
                  transproxy->content_name(),
                  transproxy->type(),
                  transproxy->sent_candidates()),
              error)) {
        return false;
      }
      transproxy->ClearSentCandidates();
    }
  }
  return true;
}

bool Session::SendMessage(ActionType type, const XmlElements& action_elems,
                          SessionError* error) {
  talk_base::scoped_ptr<buzz::XmlElement> stanza(
      new buzz::XmlElement(buzz::QN_IQ));

  SessionMessage msg(current_protocol_, type, sid_, initiator_name_);
  msg.to = remote_name_;

//  if(log_detail)LOG(LS_VERBOSE) << "Session::SendMessage A - WriteSessionMessage";

  WriteSessionMessage(msg, action_elems, stanza.get());

  SignalOutgoingMessage(this, stanza.get());
  return true;
}

template <typename Action>
bool Session::SendMessage(ActionType type, const Action& action,
                          SessionError* error) {
  talk_base::scoped_ptr<buzz::XmlElement> stanza(
      new buzz::XmlElement(buzz::QN_IQ));

//  if(log_detail)LOG(LS_VERBOSE) << "Session::SendMessage 1 - WriteSessionMessage";

  if (!WriteActionMessage(type, action, stanza.get(), error))
  {
	  LOG(LERROR) << "Session::SendMessage B - WriteActionMessage() failed.";
	  return false;
  }

//  if(log_detail)LOG(LS_VERBOSE) << "Session::SendMessage 2 - WriteSessionMessage";

  SignalOutgoingMessage(this, stanza.get());
  return true;
}

template <typename Action>
bool Session::WriteActionMessage(ActionType type, const Action& action,
                                 buzz::XmlElement* stanza,
                                 WriteError* error) {
  if (current_protocol_ == PROTOCOL_HYBRID) {
    if (!WriteActionMessage(PROTOCOL_JINGLE, type, action, stanza, error))
      return false;
    if (!WriteActionMessage(PROTOCOL_GINGLE, type, action, stanza, error))
      return false;
  } else {
    if (!WriteActionMessage(current_protocol_, type, action, stanza, error))
      return false;
  }
  return true;
}

template <typename Action>
bool Session::WriteActionMessage(SignalingProtocol protocol,
                                 ActionType type, const Action& action,
                                 buzz::XmlElement* stanza, WriteError* error) {
  XmlElements action_elems;
  if (!WriteSessionAction(protocol, action, &action_elems, error))
    return false;

  SessionMessage msg(protocol, type, sid_, initiator_name_);
  msg.to = remote_name_;

  WriteSessionMessage(msg, action_elems, stanza);
  return true;
}

void Session::SendAcknowledgementMessage(const buzz::XmlElement* stanza) {
  talk_base::scoped_ptr<buzz::XmlElement> ack(
      new buzz::XmlElement(buzz::QN_IQ));
  ack->SetAttr(buzz::QN_TO, remote_name_);
  ack->SetAttr(buzz::QN_ID, stanza->Attr(buzz::QN_ID));
  ack->SetAttr(buzz::QN_TYPE, "result");

  SignalOutgoingMessage(this, ack.get());
}

}  // namespace cricket
