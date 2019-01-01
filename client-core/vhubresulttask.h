/*
 * vhubtask.h
 *
 *  Created on: June 28, 2012
 *      Author: Shunde
 */

#ifndef VHUBRESULTTASK_H_
#define VHUBRESULTTASK_H_

#include <iosfwd>
#include <string>

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"

namespace buzz {

	class VHubResultTask : public XmppTask {
	public:

		VHubResultTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}

		virtual int ProcessStart();

		sigslot::signal2<const Jid&, const std::string&> SignalVHubResultMessageReceived;
		sigslot::signal0<> SignalCurrentStreamTargetNotValid;

	protected:
		virtual bool HandleStanza(const XmlElement * stanza);

	};

}

#endif /* VHUBRESULTTASK_H_ */
