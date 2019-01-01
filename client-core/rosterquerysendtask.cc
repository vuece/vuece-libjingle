/*
 * RosterQuerySendTask.cc
 * How to get a roster list upon login?
 * - Refer to http://xmpp.org/rfcs/rfc3921.html#roster, section 7
 *
 *  Created on: Aug 30, 2011
 *      Author: jingjing
 */
#include "talk/base/logging.h"
#include "talk/xmpp/constants.h"
#include "rosterquerysendtask.h"

namespace buzz {

XmppReturnStatus RosterQuerySendTask::Send() {
	LOG(INFO)
		<< "RosterQuerySendTask:Send() started.";

	if (GetState() != STATE_INIT && GetState() != STATE_START) {
		LOG(LS_ERROR)
			<< "RosterQuerySendTask:Send:Not inited or started yet!";
		return XMPP_RETURN_BADSTATE;
	}

	/*
	 * Message sample:
	 *
	 *   <iq from='juliet@example.com/balcony' type='get' id='roster_1'>
	 	 	 <query xmlns='jabber:iq:roster'/>
	 	 </iq>
	 *
	 */

	LOG(INFO)
		<< "RosterQuerySendTask:Sending roster query message.";

	XmlElement* iq = new XmlElement(QN_IQ);
	//iq->AddAttr(QN_FROM, user.Str());
	iq->AddAttr(QN_TYPE, STR_GET);
	iq->AddAttr(QN_ID, "123456");

	XmlElement* rq = new XmlElement(QN_ROSTER_QUERY);

	iq->AddElement(rq);

	QueueStanza(iq);

	return XMPP_RETURN_OK;
}

int RosterQuerySendTask::ProcessStart() {
	const XmlElement* stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	if (SendStanza(stanza) != XMPP_RETURN_OK)
		return STATE_ERROR;

	LOG(INFO)
		<< "RosterQuerySendTask:Roster query message sent";

	return STATE_START;
}

}
