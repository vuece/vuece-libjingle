#include "vcardpushtask.h"

#include <string>
#include <cstdio>
#include <iostream>

#include "muc.h"
#include "talk/xmpp/constants.h"
#include "talk/base/logging.h"

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


bool VCardPushTask::HandleStanza(const XmlElement * stanza) {
	if (stanza->Name() != QN_IQ)
		return false;

	if (!stanza->HasAttr(QN_TYPE)) {
		return false;
	}

	if(stanza->Attr(QN_TYPE) != "result"){
		return false;
	}

	if(stanza->FirstNamed(QN_VCARD) == NULL){
		return false;
	}

	QueueStanza(stanza);
	return true;
}

int VCardPushTask::ProcessStart() {
	const XmlElement * stanza = NextStanza();
	if (stanza == NULL)
		return STATE_BLOCKED;

	LOG(LS_VERBOSE)
		<< "ProcessStart:Processing vcard iq result.";

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

	bool hasFn (false);
	bool hasPic (false);
	std::string fNameStr = "";
	std::string picDataStr = "";

	const XmlElement* card = stanza->FirstNamed(QN_VCARD);
	const XmlElement* fn = card->FirstNamed(QN_VCARD_FN);
	const XmlElement* photo = card->FirstNamed(QN_VCARD_PHOTO);

	if(fn){
		hasFn = true;
		fNameStr = fn->BodyText();
		LOG(LS_VERBOSE) << "ProcessStart:vCard:Fn: " << fNameStr;
	}


	if (photo){
		const XmlElement* binVal = photo->FirstNamed(QN_VCARD_PHOTO_BINVAL);
		if(binVal){
			hasPic = true;
			picDataStr = binVal->BodyText();
			LOG(LS_VERBOSE) << "ProcessStart:vCard:BINVAL: " << picDataStr;
		}
	}

	if(hasFn || hasPic){
		LOG(LS_VERBOSE) << "ProcessStart:Send vCard notification.";
		SignalVCardIQResultReceived(from, fNameStr, picDataStr);
	}else
	{
		LOG(WARNING) << "No FN or photo data, won't notify.";
	}

	return STATE_START;
}

}
