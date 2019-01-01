#include "chatpushtask.h"

#include "talk/base/stringencode.h"
#include "muc.h"
#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"

//------------------------------------------------------------
// Stanza example:
//    <message to="tom@gmail.com" type="chat" id="135" from="jack@gmail.com/Talk.v10414754C02">
//		  <body>
//			Hi JJ?
//		  </body>
//		  <active xmlns="http://jabber.org/protocol/chatstates"/>
//		  <nos:x value="disabled" xmlns:nos="google:nosave"/>
//		  <arc:record otr="false" xmlns:arc="http://jabber.org/protocol/archive"/>
//    </message>
//-----------------------------------------------------------

namespace buzz {

bool ChatPushTask::HandleStanza(const XmlElement * stanza) {
	if (stanza->Name() != QN_MESSAGE)
		return false;


	QueueStanza(stanza);
	return true;
}


int ChatPushTask::ProcessStart() {
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	LOG(LS_VERBOSE) << "ProcessStart:Processing chat message.";

	Jid from(stanza->Attr(QN_FROM));

	LOG(LS_VERBOSE) << "ProcessStart:Source: " << from.node();

	const XmlElement * msgBodyElem = stanza->FirstNamed(QN_BODY);
    if (msgBodyElem != NULL) {
    	LOG(LS_VERBOSE) << "ProcessStart:Found message body:[" << msgBodyElem->BodyText() << "]";

    	SignalChatMessageReceived(from, msgBodyElem->BodyText());

    }else
    {
    	LOG(LS_ERROR) << "ProcessStart:Cannot find message body!";
    }

	return STATE_START;
}

}
