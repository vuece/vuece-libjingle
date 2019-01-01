/*
 * rosterquerysendtask.h
 *
 *  Created on: Aug 30, 2011
 *      Author: jingjing
 */

#ifndef ROSTERQUERYSENDTASK_H_
#define ROSTERQUERYSENDTASK_H_

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {

class RosterQuerySendTask : public XmppTask {
public:
	RosterQuerySendTask(Task* parent) : XmppTask(parent) {}
  virtual ~RosterQuerySendTask() {}

  XmppReturnStatus Send();

  virtual int ProcessStart();
};

}

#endif /* ROSTERQUERYSENDTASK_H_ */
