#include "vhubgettask.h"

#include <string>
#include <cstdio>
#include <iostream>

#include "muc.h"
#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"
#include "talk/base/base64.h"

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

namespace buzz
{

bool VHubGetTask::HandleStanza(const XmlElement * stanza)
{
	if (stanza->Name() != QN_IQ)
		return false;

	if (!stanza->HasAttr(QN_TYPE))
	{
		return false;
	}

	if (stanza->Attr(QN_TYPE) != "get")
	{
		return false;
	}

	if (stanza->FirstNamed(QN_VHUB) == NULL)
	{
		return false;
	}

	QueueStanza(stanza);
	return true;
}

int VHubGetTask::ProcessStart()
{
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - Processing vhub iq get.";

	if (!stanza->HasAttr(QN_FROM))
	{
		LOG(WARNING) << "VHubGetTask::ProcessStart - Message doesn't have from attribute.";
		return STATE_BLOCKED;
	}

	if (!stanza->HasAttr(QN_TYPE))
	{
		LOG(WARNING) << "VHubGetTask::ProcessStart - Message doesn't have type attribute.";
		return STATE_BLOCKED;
	}

	buzz::Jid from(stanza->Attr(QN_FROM));

	LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - Source: " << from.node();

	//we don't check null because it's already checked

	std::string msgStr = "";

	const XmlElement* vhubMessage = stanza->FirstNamed(QN_VHUB);

	if (vhubMessage)
	{
//		std::string decoded;
		msgStr = vhubMessage->BodyText();
//		talk_base::Base64::Decode(msgStr, talk_base::Base64::DO_STRICT,  &decoded, NULL);
//		LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - vHub-get:msgStr(decoded): " << decoded;
		SignalVHubGetMessageReceived(from, msgStr);

		//check if this is a resource release message from remote hub
		/**
		 * Example message:
		 * <iq to="alice@gmail.com/vuece.cont1150C8A7" id="qyY7z9qWoRgxux8K" type="get" from="alice@gmail.com/vuece.pcB0811250">
		 	 	 <vhub:vHub xmlns:vhub="vhub">
		 	 	 	 {'action':'notification', 'category':'music', 'type':'streaming-resource-released', 'sid':'2728120425'}
		 	 	 </vhub:vHub>
		 	 </iq>
		 */
		std::size_t found = msgStr.find(VHUB_MSG_STREAMINGRES_RELEASED);
		if (found != std::string::npos)
		{
			char tmp[32];
			int i = 0;

			memset(tmp, '\0', sizeof(tmp));

			LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - This is a resource release notification, extracting session id.";

			std::size_t sid_pos = msgStr.find("'sid'");

			std::string sub_str = msgStr.substr(sid_pos + 7);

			LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - sub string containing sid: " << sub_str;

			for(i = 0; i < sub_str.length(); i++)
			{
				if(sub_str[i] == '\'') break;

				tmp[i] = sub_str[i];
			}

			std::string sessionId(tmp);

			LOG(LS_VERBOSE) << "VHubGetTask::ProcessStart - Extracted sid: " << sessionId;

			SignalRemoteSessionResourceReleaseMsgReceived(sessionId);
		}
	}
	else
	{
		LOG(WARNING) << "VHubGetTask::ProcessStart - No valid vhub message, won't notify.";
	}

	return STATE_START;
}

}
