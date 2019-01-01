/*
 * VueceConnectionKeeper.cc
 *
 *  Created on: Jan 30, 2013
 *      Author: Jingjing Sun
 */


#include "VueceConnectionKeeper.h"
#include "talk/base/logging.h"
#include "talk/xmpp/constants.h"
#include "talk/base/stringutils.h"
#include "talk/base/helpers.h"
#include "VueceConstants.h"

#define VUECE_PING_INTERVAL_SECOND 100

using talk_base::CreateRandomId;

VueceConnectionKeeper::VueceConnectionKeeper(buzz::XmppClient* xmpp_client) :
	xmpp_client_(xmpp_client),
	worker_thread_(new talk_base::Thread())
{
	worker_thread_->SetName("VueceConnectionKeeper Thread", NULL);
	bStarted = false;
}

VueceConnectionKeeper::~VueceConnectionKeeper() {
	Stop();
}

void VueceConnectionKeeper::Start() {
	LOG(INFO) << "VueceConnectionKeeper::Start";
	worker_thread_->Start();
	worker_thread_->PostDelayed(VUECE_PING_INTERVAL_SECOND * 1000 / 2, this, VUECE_MSG_PING);
	bStarted = true;
}

void VueceConnectionKeeper::Stop() {
	LOG(INFO) << "VueceConnectionKeeper::Stop";
	if(bStarted){
		worker_thread_->Stop();
	}
}

void VueceConnectionKeeper::SendPingStanza()
{
	LOG(INFO) << "VueceConnectionKeeper:SendPingStanza";
	std::stringstream id;
	id << (CreateRandomId());

	buzz::XmlElement iq(buzz::QN_IQ);
	iq.AddAttr(buzz::QN_TYPE,buzz::STR_GET);
	iq.AddAttr(buzz::QN_ID,id.str());
	buzz::QName ping(true,"urn:xmpp:ping","ping");
	iq.AddElement(new buzz::XmlElement(ping, true));

	xmpp_client_->SendStanza(&iq);
}

void VueceConnectionKeeper::OnMessage(talk_base::Message *msg) {
	switch (msg->message_id) {
	case VUECE_MSG_PING:
	{
		LOG(INFO) << "VueceConnectionKeeper:MSG_PING";

		ASSERT(worker_thread_->IsCurrent());

		SendPingStanza();

		LOG(INFO) << "VueceConnectionKeeper:MSG_PING - Post delayed ping message again.";

		worker_thread_->PostDelayed(VUECE_PING_INTERVAL_SECOND * 1000, this, VUECE_MSG_PING);

		break;
	}
	}
}

