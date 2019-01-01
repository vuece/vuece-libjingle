/*
 * chatpushtask.h
 *
 *  Created on: May 1, 2011
 *      Author: jingjing
 */

#ifndef CHATPUSHTASK_H_
#define CHATPUSHTASK_H_

#include <iosfwd>
#include <string>

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/base/sigslot.h"

namespace buzz {

class ChatPushTask : public XmppTask {
 public:

  ChatPushTask(Task* parent) : XmppTask(parent, XmppEngine::HL_TYPE) {}

  virtual int ProcessStart();

  sigslot::signal2<const Jid&, const std::string&> SignalChatMessageReceived;

 protected:
  virtual bool HandleStanza(const XmlElement * stanza);

};


}


#endif /* CHATPUSHTASK_H_ */
