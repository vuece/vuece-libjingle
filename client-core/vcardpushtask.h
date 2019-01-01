/*
 * vcardpushtask.h
 *
 *  Created on: May 2, 2011
 *      Author: jingjing
 */

#ifndef VCARDPUSHTASK_H_
#define VCARDPUSHTASK_H_

#include <iosfwd>
#include <string>

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"

namespace buzz {

	class VCardPushTask : public XmppTask {
	public:

		VCardPushTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}

		virtual int ProcessStart();

		//	public void onReceiveVCard(String jid, String fullName, String iconBase64String);
		sigslot::signal3<const Jid&, const std::string&, const std::string&> SignalVCardIQResultReceived;

	protected:
		virtual bool HandleStanza(const XmlElement * stanza);

	};

}

#endif /* VCARDPUSHTASK_H_ */
