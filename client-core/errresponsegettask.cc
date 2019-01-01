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
/*
 * <iq to="alice@gmail.com/vuece.contC7F38BD3" id="yAWcshXBnUXsreA8" type="error" from="alice@gmail.com/vuece.pcE97F2498">

      <vhub:vHub xmlns:vhub="vhub">

       {'action':'play', 'category':'music','control':'start','uri':'31b3dae7eb4d3f43900a698f3bebc07f', 'start':'0', 'need_preview': '1' }

     </vhub:vHub>

     <error code="503" type="cancel">

      <service-unavailable xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>

      </error>

   </iq>
 */
//-----------------------------------------------------------

	/**
	 * Example message:
	 * <iq to="alice@gmail.com/vuece.cont1150C8A7" id="qyY7z9qWoRgxux8K" type="get" from="alice@gmail.com/vuece.pcB0811250">
	 	 	 <vhub:vHub xmlns:vhub="vhub">
	 	 	 	 {'action':'notification', 'category':'music', 'type':'streaming-resource-released', 'sid':'2728120425'}
	 	 	 </vhub:vHub>
	 	 </iq>
	 */


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

	if (stanza->Attr(QN_TYPE) != "error")
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
	//check if this is a resource release message from remote hub


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
		msgStr = vhubMessage->BodyText();
		SignalVHubGetMessageReceived(from, msgStr);

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
