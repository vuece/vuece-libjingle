/*
 * RosterSubResponseRecvTask.h
 *
 *  Created on: Sep 7, 2011
 *      Author: jingjing
 */

#ifndef ROSTERSUBRESPONSERECVTASK_H_
#define ROSTERSUBRESPONSERECVTASK_H_

#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {
	class RosterSubResponseRecvTask : public XmppTask {
	public:
		RosterSubResponseRecvTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}
		virtual int ProcessStart();

		sigslot::signal1<const XmlElement*> SignalRosterSubRespRecieved;

	protected:
		virtual bool HandleStanza(const XmlElement* stanza);

	};
}


#endif /* ROSTERSUBRESPONSERECVTASK_H_ */
