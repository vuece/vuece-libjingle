/*
 * VueceConnectionKeeper.h
 *
 *  Created on: Jan 30, 2013
 *      Author: Jingjing Sun
 */

#ifndef VUECECONNECTIONKEEPER_H_
#define VUECECONNECTIONKEEPER_H_

#include "talk/base/sigslot.h"
#include "talk/p2p/base/constants.h"
#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "talk/xmpp/xmppclient.h"

class VueceConnectionKeeper : public talk_base::MessageHandler, public sigslot::has_slots<> {

public:
	VueceConnectionKeeper(buzz::XmppClient* xmpp_client);
	~VueceConnectionKeeper();

	virtual void OnMessage(talk_base::Message *msg);

	void Start();
	// Stops reading lines. Cannot be restarted.
	void Stop();

private:
	  void SendPingStanza();
	  buzz::XmppClient* xmpp_client_;
	  talk_base::Thread* worker_thread_;
	  bool bStarted;

};


#endif /* VUECECONNECTIONKEEPER_H_ */
