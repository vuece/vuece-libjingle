/*
 * rosterqueryresultrecvtask.h
 *
 *  Created on: Sep 1, 2011
 *      Author: jingjing
 */

#ifndef ROSTERQUERYRESULTRECVTASK_H_
#define ROSTERQUERYRESULTRECVTASK_H_

#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {
	class RosterQueryResultRecvTask : public XmppTask {
	public:
		RosterQueryResultRecvTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}
		virtual int ProcessStart();

		sigslot::signal1<const XmlElement*> SignalRosterRecieved;

	protected:
		virtual bool HandleStanza(const XmlElement* stanza);

	};
}

#endif /* ROSTERQUERYRESULTRECVTASK_H_ */
