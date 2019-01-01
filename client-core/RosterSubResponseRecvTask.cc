/*
 * RosterSubResponseRecvTask.cc
 *
 *  Created on: Sep 7, 2011
 *      Author: jingjing
 */

#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"

#include "RosterSubResponseRecvTask.h"
/**
 * EXAMPLE MESSAGE:
 * <presence type="subscribed" from="alice@gmail.com" to="jack@gmail.com"/>
 */

namespace buzz {

bool RosterSubResponseRecvTask::HandleStanza(const XmlElement* stanza) {

//	ms_warning("RosterSubResponseRecvTask. %s", (stanza->Str()).c_str());
	if (stanza->Name() != QN_PRESENCE)
		return false;

	if (!stanza->HasAttr(QN_TYPE)) {
		return false;
	}

	if (!stanza->HasAttr(QN_FROM)) {
		return false;
	}

//	if (!stanza->HasAttr(QN_TO)) {
//		return false;
//	}


	LOG(LS_VERBOSE) << "RosterSubResponseRecvTask:Received a roster subscription response.";
//	ms_warning("RosterSubResponseRecvTask:Received a roster subscription response.");
	QueueStanza(stanza);

	return true;

}

int RosterSubResponseRecvTask::ProcessStart() {
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	SignalRosterSubRespRecieved(stanza);

	return STATE_START;
}

}

