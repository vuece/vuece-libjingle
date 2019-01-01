/*
 * VueceStreamPlayerMonitorThread2.h
 *
 *  Created on: Mar 19, 2015
 *      Author: jingjing
 */

#ifndef VueceStreamPlayerMonitorThread2_H_
#define VueceStreamPlayerMonitorThread2_H_

#include "jthread.h"
#include "VueceConstants.h"
#include "talk/base/sigslot.h"

class VueceStreamPlayerMonitorThread2 : public JThread, public sigslot::has_slots<>
{

public:
	VueceStreamPlayerMonitorThread2();
	virtual ~VueceStreamPlayerMonitorThread2();

	void* Thread();

	void StopSync();
	void StopStreamPlayer();
	void StopAndResetStreamPlayer();

private:
	void StopStreamPlayerInternal();

private:
	bool running;
	JMutex mutex_running;
	JMutex mutex_release;
	bool released;
	bool stop_cmd_issued;
	bool enable_reset;
};


#endif /* VueceStreamPlayerMonitorThread2_H_ */
