#include "vhubresulttask.h"

#include <string>
#include <cstdio>
#include <iostream>

#include "muc.h"
#include "talk/base/base64.h"
#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"
#include "VueceConstants.h"
//------------------------------------------------------------
// Stanza example:
//		<iq from="john@gmail.com" to="tom@gmail.com/callDEDCF051" id="lqCqTEiu0Vz6QJBC" type="result">
//			<vCard xmlns="vcard-temp">
//				<FN>
//					john
//				</FN>
//				<PHOTO>
//						  <TYPE>
//							image/png
//						  </TYPE>
//						<BINVAL>
//							IMAGEDATAXXXXXXXXXXXXXXXXXXXXXXXX
//						</BINVAL>
//				</PHOTO>
//		  </vCard>
//		</iq>
//-----------------------------------------------------------

namespace buzz {


bool VHubResultTask::HandleStanza(const XmlElement * stanza) {
	if (stanza->Name() != QN_IQ)
		return false;

	if (!stanza->HasAttr(QN_TYPE)) {
		return false;
	}

	if(stanza->Attr(QN_TYPE) != "result"){
		return false;
	}

	if(stanza->FirstNamed(QN_VHUB) == NULL){
		return false;
	}

	QueueStanza(stanza);
	return true;
}

int VHubResultTask::ProcessStart() {
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	LOG(LS_VERBOSE)
		<< "ProcessStart:Processing vhub iq result.";

	if(!stanza->HasAttr(QN_FROM))
	{
		LOG(WARNING) << "ProcessStart:Message doesn't have from attribute.";
		return STATE_BLOCKED;
	}

	if(!stanza->HasAttr(QN_TYPE))
	{
		LOG(WARNING) << "ProcessStart:Message doesn't have type attribute.";
		return STATE_BLOCKED;
	}


	buzz::Jid from(stanza->Attr(QN_FROM));
	
	LOG(LS_VERBOSE)
		<< "ProcessStart:Source: " << from.node();

	//we don't check null because it's already checked

	std::string msgStr = "";

	const XmlElement* vhubMessage = stanza->FirstNamed(QN_VHUB);

	if(vhubMessage)
	{
		msgStr = vhubMessage->BodyText();
		SignalVHubResultMessageReceived(from, msgStr);

		std::size_t found1 = msgStr.find(VHUB_MSG_STREAMING_TARGET_INVALID);
		std::size_t found2 = msgStr.find(VHUB_MSG_BUSY_NOW);

		if (found1 != std::string::npos || found2 != std::string::npos)
		{
			LOG(WARNING) << "VHUB - [TROUBLESHOOTING] - Steaming target is invalid, cancel current play now.";
			SignalCurrentStreamTargetNotValid();
		}


	}else {
		LOG(WARNING) << "No valid vhub message, won't notify.";
	}

	return STATE_START;
}

}
