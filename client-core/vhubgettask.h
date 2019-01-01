/*
 * vhubtask.h
 *
 *  Created on: June 28, 2012
 *      Author: Shunde
 */

#ifndef VHUBGETTASK_H_
#define VHUBGETTASK_H_

#include <iosfwd>
#include <string>

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"

namespace buzz {

	class VHubGetTask : public XmppTask {
	public:

		VHubGetTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}

		virtual int ProcessStart();

		//	public void onReceiveVHubGetMessage(String jid, String message);
		sigslot::signal2<const Jid&, const std::string&> SignalVHubGetMessageReceived;
		sigslot::signal1<const std::string&> SignalRemoteSessionResourceReleaseMsgReceived;

	protected:
		virtual bool HandleStanza(const XmlElement * stanza);

	};

}

#endif /* VHUBGETTASK_H_ */
