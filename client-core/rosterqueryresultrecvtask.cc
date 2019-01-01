/*
 * rosterqueryresultrecvtask.cc
 *
 *  Created on: Sep 1, 2011
 *      Author: jingjing
 */

#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"

#include "rosterqueryresultrecvtask.h"

/**
 * EXAMPLE MESSAGE:
 *     <iq to="tom@gmail.com/call54E75630" id="123456" type="result">
		 <query xmlns="jabber:iq:roster">
			 <item jid="john@gmail.com" subscription="both" name="john"/>
			 <item jid="jack@gmail.com" subscription="both" name="Jingjing Sun"/>
			 <item jid="msn.transport.talkonaut.com" subscription="both"/>
			 <item jid="service@gtalk2voip.com" subscription="both"/>
			 <item jid="android.test.htc2@gmail.com" subscription="both"/>
			 <item jid="tom@gmail.com" subscription="none" name="tom"/>
			 <item jid="alice@gmail.com" subscription="both" name="alice"/>
		 </query>
 	 </iq>
 */

namespace buzz {

bool RosterQueryResultRecvTask::HandleStanza(const XmlElement* stanza) {
	if (stanza->Name() != QN_IQ)
		return false;

	if (!stanza->HasAttr(QN_TO)) {
		return false;
	}

	if (!stanza->HasAttr(QN_ID)) {
		return false;
	}

	if (!stanza->HasAttr(QN_TYPE)) {
		return false;
	}

	if (stanza->Attr(QN_TYPE) != "result") {
		return false;
	}

	const XmlElement * query = stanza->FirstNamed(QN_ROSTER_QUERY);

	if (query == NULL) {
		return false;
	}

	LOG(LS_VERBOSE) << "RosterQueryResultRecvTask:Received a roster query response.";

	QueueStanza(stanza);

	return true;

}

int RosterQueryResultRecvTask::ProcessStart() {
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	SignalRosterRecieved(stanza);

	return STATE_START;
}

}
