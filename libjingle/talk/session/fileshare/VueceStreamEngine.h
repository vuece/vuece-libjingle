/*
 * VueceStreamEngine.h
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#ifndef VUECESTREAMENGINE_H_
#define VUECESTREAMENGINE_H_

#include "jthread.h"

#include "VueceConstants.h"

#include "talk/base/sigslot.h"

class VueceMediaDataBumper;
class VueceAACDecoder;
class VueceAudioWriter;
class VueceMemQueue;

class VueceStreamEngine : public JThread, public sigslot::has_slots<>
{
public:

	VueceStreamEngine();
	virtual ~VueceStreamEngine();

	bool Init(
			int sample_rate,
			int bit_rate,
			int channel_num,
			int duration,
			int frame_dur_ms,
			bool startup_standalone,
			bool download_finished,
			int last_avail_chunk_idx,
			int last_chunk_frame_count,
			int resume_pos
			);

	void* Thread();

	void StopSync();

	void OnBumperExternalEventNotification(VueceBumperExternalEventNotification *event);
	void OnAudioWriterExternalEventNotification(VueceStreamAudioWriterExternalEventNotification *event);

public:
	VueceMediaDataBumper* 	bumper;
	VueceAACDecoder* 		decoder;
	VueceAudioWriter* 		writer;

private:

	//TODO - Maybe we don't need class variables here, just put every
	//int the thread loop as local variable because they will be destroyed
	//when the thread exits.
	bool running;
	JMutex mutex_running;
	JMutex mutex_release;
	bool released;
	bool stop_cmd_issued;

};


#endif /* VUECESTREAMENGINE_H_ */
