/*
 * VueceMediaDataBumper.cc
 *
 *  Created on: Oct 31, 2014
 *      Author: jingjing
 */


#include "talk/base/logging.h"
#include "VueceLogger.h"
#include "VueceMediaDataBumper.h"
#include "VueceStreamPlayer.h"
#include "VueceConstants.h"
#include "VueceConfig.h"

VueceMediaDataBumper::VueceMediaDataBumper()
{
	mutex_bumper_state = NULL;
	mutex_chunk_idx = NULL;
	bumper_data = NULL;
	out_q = NULL;
}

bool VueceMediaDataBumper::Init()
{
	bumper_data = (VueceMediaBumperData*) malloc(sizeof(VueceMediaBumperData));

	VueceMediaBumperData* d = bumper_data;

	VueceLogger::Debug("VueceMediaDataBumper - INIT - 1");

	d->readBuf = (uint8_t*)malloc(VUECE_MAX_FRAME_SIZE);
	d->availBufFileCounter = 0;
	d->iActiveBufFileIdx = -1;
	d->iLastAvailChunkFileIdx = -1;
	d->iFrameCountOfLastChunk = 0;
	d->bBufferReadable = false;
	d->fActiveBufferFile = NULL;
	d->iReadCount = 0;
	d->bIsDowloadCompleted = false;
	d->bIsMergingFiles = false;
	d->bSaveTestFlag = false;
	d->bTestFlag = false;
	d->bStandaloneFlag = false;

	d->iTotoalFrameCounter = 0;
	d->iFrameHeaderLen = VUECE_STREAM_FRAME_HEADER_LENGTH;
	d->iFrameDurationInMs = 0;

	d->lAudioChunkDurationInMs = 0;

	d->iFirstFramePosMs = 0;

	d->bumperState = VueceBumperState_Ready;

	VueceLogger::Debug("VueceMediaDataBumper - INIT - initial state is set to READY");

	mutex_chunk_idx = new JMutex();
	mutex_bumper_state = new JMutex();

	mutex_chunk_idx->Init();
	mutex_bumper_state->Init();

	if(!mutex_chunk_idx->IsInitialized())
	{
		VueceLogger::Fatal("Init: mutex_chunk_idx cannot be initialized");
		return false;
	}

	if(!mutex_bumper_state->IsInitialized())
	{
		VueceLogger::Fatal("Init: mutex_chunk_idx cannot be initialized");
		return false;
	}

	VueceLogger::Debug("VueceMediaDataBumper - INIT - Done");

	return true;
}


void VueceMediaDataBumper::Uninit()
{
	VueceLogger::Debug("VueceMediaDataBumper::Uninit - Start");

	mutex_bumper_state->Lock();

	LogFsmState(bumper_data->bumperState);

	VueceLogger::Debug("VueceMediaDataBumper - Uninit: State is switched from %d ---> STOPPED", bumper_data->bumperState);

	bumper_data->bumperState = VueceBumperState_Stopped;

	mutex_bumper_state->Unlock();

	if(bumper_data != NULL)
	{
		if( bumper_data->readBuf != NULL)
		{
			free(bumper_data->readBuf);
		}

		free(bumper_data);
	}


	if( mutex_chunk_idx != NULL)
	{
		mutex_chunk_idx->Unlock();

		delete mutex_chunk_idx;
	}

	if( mutex_bumper_state != NULL)
	{
		mutex_bumper_state->Unlock();

		delete mutex_bumper_state;
	}

	VueceLogger::Debug("VueceMediaDataBumper::Uninit - Done");
}

VueceMediaDataBumper::~VueceMediaDataBumper()
{
	LOG(LS_VERBOSE) << "VueceMediaDataBumper - Destructor called";

	SignalBumperNotification.disconnect_all();
}

#define MODUE_NAME_BUMPER "VueceMediaDataBumper"

bool VueceMediaDataBumper::StateTranstion(VueceBumperFsmEvent e)
{
	bool allowed = false;
	char current_state_s[32];
	char new_state_s[32];
	char event_s[32];

	VueceMediaBumperData *d = bumper_data;

	mutex_bumper_state->Lock();

	GetFsmEventString(e, event_s);
	GetFsmStateString(d->bumperState, current_state_s);

	switch(d->bumperState)
	{
	case VueceBumperState_Ready:
	{
		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
			if(d->bBufferReadable)
			{
				allowed = true;
				d->bumperState = VueceBumperState_Bumping;
			}
			else
			{
				allowed = false;
				d->bumperState = VueceBumperState_Buffering;

				GetFsmStateString(d->bumperState, new_state_s);
				VueceStreamPlayer::LogStateTranstion(MODUE_NAME_BUMPER, event_s, current_state_s, new_state_s);
			}

			break;
		}
		case VueceBumperFsmEvent_Pause:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Paused;
			break;
		}
		case VueceBumperFsmEvent_LocalSeekSucceeded:
		{
			allowed = true;
			//TTTT
			d->bumperState = VueceBumperState_Bumping;
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceBumperState_Buffering:
	{

		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
			if(d->bBufferReadable)
			{
				allowed = true;
				d->bumperState = VueceBumperState_Bumping;
			}
			else
			{
				//stay at the same state
				allowed = false;
//				VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			}
			break;
		}
		case VueceBumperFsmEvent_Pause:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Paused;
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceBumperState_Bumping:
	{
		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
			if(d->bBufferReadable)
			{
				allowed = true;
//				VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			}
			else
			{
				allowed = false;
				d->bumperState = VueceBumperState_Buffering;
				GetFsmStateString(d->bumperState, new_state_s);
				VueceStreamPlayer::LogStateTranstion(MODUE_NAME_BUMPER, event_s, current_state_s, new_state_s);
			}

			break;
		}
		case VueceBumperFsmEvent_Pause:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Paused;
			break;
		}
		case VueceBumperFsmEvent_DataBelowThreshold:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Buffering;
			break;
		}
		case VueceBumperFsmEvent_AllDataConsumed:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Completed;
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceBumperState_Paused:
	{
		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
//			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		case VueceBumperFsmEvent_Resume:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Bumping;
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}

		break;
	}
	case VueceBumperState_Completed:
	{
		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
//			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		case VueceBumperFsmEvent_Pause:
		{
//			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			allowed = true;
			d->bumperState = VueceBumperState_Stopped;
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}
		break;
	}
	case VueceBumperState_Stopped:
	{
		switch(e)
		{
		case VueceBumperFsmEvent_Tick:
		{
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		case VueceBumperFsmEvent_Pause:
		{
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		case VueceBumperFsmEvent_Stop:
		{
			VueceStreamPlayer::LogIgnoredEvent(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		default:
		{
			VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
			break;
		}
		}
		break;
	}
	case VueceBumperState_Err:
	{
		//Not handled for now
		VueceStreamPlayer::AbortOnInvalidTranstion(MODUE_NAME_BUMPER, event_s, current_state_s);
		break;
	}
	}

	if(allowed)
	{
		GetFsmStateString(d->bumperState, new_state_s);
		VueceStreamPlayer::LogStateTranstion(MODUE_NAME_BUMPER, event_s, current_state_s, new_state_s);
	}

	mutex_bumper_state->Unlock();
	return allowed;
}



void VueceMediaDataBumper::Process(VueceMemQueue* in_q, VueceMemQueue* _out_q)
{
	bool do_return = false;
	//Debug only - This will generate massive trace output
//	VueceLogger::Debug("VueceMediaDataBumper - Process START");

	VueceMediaBumperData *d = bumper_data;

	out_q = _out_q;

	//one-time operation
	if(d->bStandaloneFlag)
	{
		VueceLogger::Debug("VueceMediaDataBumper - Process: Started standalone, seek at first");
		d->bStandaloneFlag = false;

		//Note this method will set bBufferReadable to true
		//TODO - Need to handle seek error here
		StreamSeek(d);

		VueceLogger::Debug("VueceMediaDataBumper - Process: Seek returned");

		if( !StateTranstion(VueceBumperFsmEvent_LocalSeekSucceeded) )
		{
			return;
		}
	}

	if( !StateTranstion(VueceBumperFsmEvent_Tick) )
	{
		//further operation is not allowed
		return;
	}

    //consume all available buffered data in a loop
	while(d->bBufferReadable)
	{

//		VueceLogger::Debug("VueceMediaDataBumper - Processing data loop - Start");

		//don't play if user has pressed pause button or playing is finished
		mutex_bumper_state->Lock();
		if(d->bumperState != VueceBumperState_Bumping)
		{
			VueceLogger::Warn("VueceMediaDataBumper - bumper_process: Bumper is not in BUMPING state.");
			mutex_bumper_state->Unlock();
			return;
		}

		mutex_bumper_state->Unlock();

		ReadAndQueueFrame(d);

//		VueceLogger::Debug("VueceMediaDataBumper - Processing data loop - END");
	}

//	VueceLogger::Debug("VueceMediaDataBumper - Process END");
}

void VueceMediaDataBumper::GetChunkFileNameFromIdx(int idx, char* fname)
{
	char tmp[16];

	LOG(LS_VERBOSE) << "VueceMediaDataBumper - bumper_get_chunk_file_name_from_idx: active file idx = " << idx;

	memset(tmp, 0, sizeof(tmp));

	strcpy(fname, VUECE_MEDIA_AUDIO_BUFFER_LOCATION);

	sprintf(tmp, "%d", idx);

	strcat(fname, tmp);
}

bool VueceMediaDataBumper::ActivateBufferFile(VueceMediaBumperData *d)
{
	char cfilename[128];
	char mode[8];
	FILE* file_;

	memset(cfilename, 0, sizeof(cfilename));
	memset(mode, 0, sizeof(mode));

	VueceLogger::Debug("VueceMediaDataBumper - bumper_activate_buffer_file");


	if(d->fActiveBufferFile != NULL)
	{
		VueceLogger::Fatal("VueceMediaDataBumper - bumper_activate_buffer_file: fActiveBufferFile should be null!");
	}

	//NOTE: The initial value of iActiveBufFileIdx is -1, the first buffer file index is 0,
	//so we need to increase iActiveBufFileIdx by 1 at first
	d->iActiveBufFileIdx++;

	if(d->iActiveBufFileIdx > d->iLastAvailChunkFileIdx)
	{
		VueceLogger::Fatal("FATAL ERROR - VueceMediaDataBumper - bumper_activate_buffer_file: target chunk idx is beyond boundary!");
	}

	LOG(LS_VERBOSE) << "VueceMediaDataBumper - bumper_activate_buffer_file: target chunk file idx = " << d->iActiveBufFileIdx;

	//check special case
	if(d->bIsDowloadCompleted && d->iActiveBufFileIdx == d->iLastAvailChunkFileIdx)
	{
		if(d->iFrameCountOfLastChunk == 0)
		{
			LOG(LS_VERBOSE) << "VueceMediaDataBumper - Last available chunk file is empty, all data consumed";

			OnAllDataConsumed();

			return true;
		}
	}

	//open buffer file to read
	GetChunkFileNameFromIdx(d->iActiveBufFileIdx, cfilename);

	LOG(LS_VERBOSE) << "VueceMediaDataBumper - bumper_activate_buffer_file: file name = " << cfilename;

	strcpy(mode, "r");

#ifdef WIN32
		//NOTE - Empty impl for now because Utf8ToWindowsFilename is defined
	//in libjingle
	//std::wstring wfilename;
  //std::string str(cfilename);
  //if (Utf8ToWindowsFilename(str, &wfilename)) {
  //  file_ = _wfopen(wfilename.c_str(), ToUtf16(mode).c_str());
  //} else {
  //  file_ = NULL;
  //}
#else
  file_ = fopen(cfilename, mode);
#endif

  if(file_ == NULL)
  {
	  VueceLogger::Fatal("VueceMediaDataBumper - bumper_activate_buffer_file: file open failed!");
  }

  d->fActiveBufferFile = file_;

  return true;
}

int VueceMediaDataBumper::ByteArrayToInt(uint8_t* b)
{
	int i = 0;
    int value = 0;
    for (i = 0; i < 4; i++) {
        int shift = (4 - 1 - i) * 8;
        value += (b[i] & 0x000000FF) << shift;
    }
    return value;
}

int VueceMediaDataBumper::ReadFrame(VueceMediaBumperData *d)
{
	int frame_len;
	int sig;
	int ts;

	size_t result;
	bool bFileCompleted = false;

//	VueceLogger::Debug("VueceMediaDataBumper - bumper_read_frame");

	if(d->fActiveBufferFile == NULL)
	{
		VueceLogger::Fatal ("VueceMediaDataBumper - bumper_read_frame: No active buffer file.");
		return 0;
	}

	//read header - [SignalByte][FrameLen][FrameTS][DATA]
	result = fread(d->readBuf, 1, d->iFrameHeaderLen, d->fActiveBufferFile);

	sig = d->readBuf[0];

	frame_len = ByteArrayToInt(d->readBuf+1);

	ts = ByteArrayToInt(d->readBuf+5);

	if(d->bTestFlag == true)
	{
		VueceLogger::Debug("VueceMediaDataBumper - bumper_read_frame: sig = %d, Frame len = %d, ts = %d, file idx = %d", sig, frame_len, ts, d->iActiveBufFileIdx);
	}


	if(frame_len > VUECE_MAX_FRAME_SIZE || frame_len <= 0)
	{
		VueceLogger::Error("FATAL ERROR - iActiveBufFileIdx = %d, iReadCount = %d", d->iActiveBufFileIdx, d->iReadCount);
		VueceLogger::Error("VueceMediaDataBumper - bumper_read_frame:file position: %ld", ftell(d->fActiveBufferFile));
		VueceLogger::Fatal("VueceMediaDataBumper - bumper_read_frame: Frame length(%d) is too long?, sth is wrong.", frame_len);
		return 0;
	}

	//read actual frame
	result = fread(d->readBuf, 1, frame_len, d->fActiveBufferFile);

//	VueceLogger::Debug(tmp_buf, "VueceMediaDataBumper - bumper_read_frame: iReadCount = %d", d->iReadCount);

	if(result != frame_len)
	{
		VueceLogger::Fatal("VueceMediaDataBumper - bumper_read_frame: Frame is not read completely, target frame len = %d, actual bytes read = %d", frame_len, result);
		return 0;
	}

	d->iReadCount++;
	d->iTotoalFrameCounter++;

	if (feof(d->fActiveBufferFile) || d->iReadCount == VUECE_AUDIO_FRAMES_PER_CHUNK)
	{
		bFileCompleted = true;
	}
	else if(d->bIsDowloadCompleted)
	{
		if(d->iActiveBufFileIdx == d->iLastAvailChunkFileIdx)
		{
			//note 0 is the first frame index
			if(d->iReadCount == d->iFrameCountOfLastChunk)
			{
				bFileCompleted = true;
			}
		}
	}


    if (bFileCompleted)
    {
//    	VueceLogger::Debug ("VueceMediaDataBumper - bumper_read_frame: Buffer file end OR read count reached, close file now.");
    	fclose(d->fActiveBufferFile);
        d->fActiveBufferFile = NULL;
        d->iReadCount = 0;

        d->availBufFileCounter--;

 		if(d->bIsDowloadCompleted)
        {
 			VueceLogger::Debug("VueceMediaDataBumper - bumper_read_frame: Download is completed");

 			if(d->bIsMergingFiles)
 			{
 				d->bIsChunkMerged = true;
 				return result;
 			}

 			//All data consumed and transfered to decoder module, so now we can
 			//calculate the actual duration
 			//TODO
        	if(d->availBufFileCounter == -1)
        	{
        		VueceLogger::Debug ("VueceMediaDataBumper - Bumper is finished, the last chunk idx is = %d, total frame count = %lu",
            			d->iLastAvailChunkFileIdx, 	d->iTotoalFrameCounter);
        	}
        }
        else if(d->availBufFileCounter < VUECE_BUFFER_AVAIL_INDICATOR)
        {
        	StateTranstion(VueceBumperFsmEvent_DataBelowThreshold);

        	//commented out for now, no need to notify
//        	VueceBumperExternalEventNotification n;
//        	n.event = VueceBumperExternalEvent_BUFFERING;
        	//TODO enable this later
        	//SignalBumperNotification(&n);

        	d->bBufferReadable = false;

        	VueceLogger::Debug("VueceMediaDataBumper - --------------------------------");
        	VueceLogger::Debug ("VueceMediaDataBumper - bumper_read_frame: Number of available buffer files (%d) is below threshold (%d), buffer not readable, enter buffering state",
        			d->availBufFileCounter, VUECE_BUFFER_AVAIL_INDICATOR);
        	VueceLogger::Debug ("VueceMediaDataBumper - -------------------------------");
        }
    }

	return result;
}

void VueceMediaDataBumper::ReadAndQueueFrame(VueceMediaBumperData *d){

//	VueceLogger::Debug("VueceMediaDataBumper - bumper_read_and_queue_frame");

	if(d->bBufferReadable)
	{
		VueceMemBulk *om;
		size_t result_frame_len;

		if(d->fActiveBufferFile == NULL)
		{
			VueceLogger::Debug("VueceMediaDataBumper - Buffer is readable but there is no active buffer now, last active file idx: %d, last file idx: %d",
					d->iActiveBufFileIdx, d->iLastAvailChunkFileIdx);

			if(d->bIsDowloadCompleted)
			{
				//the end is reached, all data has been pumped into next module
				if(d->iActiveBufFileIdx >= d->iLastAvailChunkFileIdx)
				{
					VueceLogger::Debug("VueceMediaDataBumper - The whole stream transfer is completed, last chunk file is reached, stop bumper now");
					VueceLogger::Debug("VueceMediaDataBumper - The whole stream transfer is completed, mark bumper state as COMPLETED and trigger notification");

					OnAllDataConsumed();

					return;
				}
			}


			ActivateBufferFile(d);

			if(	d->bumperState == VueceBumperState_Completed)
			{
				return;
			}
		}

		//read one frame from buffer file
		result_frame_len = ReadFrame(d);

//		VueceLogger::Debug("VueceMediaDataBumper - One frame has been read from buffer file, length = %d", result_frame_len);

		if(result_frame_len > 0)
		{
			om = VueceMemQueue::AllocMemBulk(result_frame_len);

			memcpy(om->data, d->readBuf, result_frame_len);

			om->size_orginal = result_frame_len;

			out_q->Put(om);

//			VueceLogger::Debug("bumper_read_and_queue_frame - buffer successfully queued, size = %d", result_frame_len);
		}

	}
	else
	{
//		VueceLogger::Debug("bumper_read_and_queue_frame - buffer is not readable yet.");
	}
}

void VueceMediaDataBumper::OnAllDataConsumed()
{
	VueceBumperExternalEventNotification n;

	VueceLogger::Debug("VueceMediaDataBumper - OnAllDataConsumed");

	StateTranstion(VueceBumperFsmEvent_AllDataConsumed) ;

	n.event = VueceBumperExternalEvent_COMPLETED;

	SignalBumperNotification(&n);

	VueceLogger::Debug("VueceMediaDataBumper - OnAllDataConsumed: state is switched to COMPLETED");
}

void VueceMediaDataBumper::SetTerminateInfo(int last_avail_chunk_file_idx, int nr_frames_of_last_chunk)
{
	VueceLogger::Debug("VueceMediaDataBumper - SetTerminateInfo: last_avail_chunk_file_idx: %d, nr_frames_of_last_chunk: %d",
			last_avail_chunk_file_idx, nr_frames_of_last_chunk);

	VueceMediaBumperData *d = bumper_data;


	mutex_chunk_idx->Lock();
	d->iLastAvailChunkFileIdx = last_avail_chunk_file_idx;

	//Note - We found a bug here, if the ID of the last available chunk is zero, that means there is no
	// readable chunk at all, so we also need to set the bBufferReadable to false

	if(d->iLastAvailChunkFileIdx  < 0)
	{
		VueceLogger::Debug("VueceMediaDataBumper - SetTerminateInfo:Last available chunk ID is 0, buffer is not readable.");
		d->bBufferReadable = false;
	}

	d->iFrameCountOfLastChunk = nr_frames_of_last_chunk;

	d->bIsDowloadCompleted = true;

	//if download is completed, then buffer is always available
	d->bBufferReadable = true;

	d->availBufFileCounter = d->iLastAvailChunkFileIdx - d->iActiveBufFileIdx;

	VueceLogger::Debug("VueceMediaDataBumper - SetTerminateInfo:last chunk id = %d, active chunk id = %d", d->iLastAvailChunkFileIdx, d->iActiveBufFileIdx);

	mutex_chunk_idx->Unlock();

}

//inject last available chunk file index
int VueceMediaDataBumper::SetLastAvailChunkFileIdx(int i){

	VueceMediaBumperData *d = bumper_data;

	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx with value: %d", i);

	mutex_chunk_idx->Lock();
	d->iLastAvailChunkFileIdx = i;

	//Note - We found a bug here, if the ID of the last available chunk is zero, that means there is no
	// readable chunk at all, so we also need to set the bBufferReadable to false

	if(d->iLastAvailChunkFileIdx  < 0)
	{
		VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx:Last available chunk ID is 0, buffer is not readable.");
		d->bBufferReadable = false;
	}

	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx: %d", d->iLastAvailChunkFileIdx );
	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Update availBufFileCounter, current value: %d", d->availBufFileCounter );
	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Update availBufFileCounter, iLastAvailChunkFileIdx: %d", d->iLastAvailChunkFileIdx );
	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Update availBufFileCounter, iActiveBufFileIdx: %d", d->iActiveBufFileIdx );

	/*
	 * Note - This is very important, available buffer file counter is updated here, if it's above
	 * the threshold, bumping data will be enabled, and player will be produce sound
	 */
	d->availBufFileCounter = d->iLastAvailChunkFileIdx - d->iActiveBufFileIdx;

	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - availBufFileCounter is updated to: %d", d->availBufFileCounter );


    if(d->availBufFileCounter < VUECE_BUFFER_AVAIL_INDICATOR)
    {
    	VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Buffer still below threshold, not readable");
    	d->bBufferReadable = false;
    }
    else
    {
		VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Buffer is readable now, available buffer counter = %d", d->availBufFileCounter);

		/*
		 * If buffer is readable, we need to notify stream player so it can update its own state
		 * if it's in WAITING state
		 */
		d->bBufferReadable = true;

		VueceLogger::Debug("VueceMediaDataBumper::SetLastAvailChunkFileIdx - Fire an external notification because data is available for playing");

		VueceBumperExternalEventNotification external_notify;
		external_notify.event = VueceBumperExternalEvent_DATA_AVAILABLE;

		SignalBumperNotification(&external_notify);
    }

	mutex_chunk_idx->Unlock();

	return 0;
}

int VueceMediaDataBumper::SetFrameCountOfLastChunk(int i){

	VueceMediaBumperData *d = bumper_data;

	mutex_chunk_idx->Lock();
	d->iFrameCountOfLastChunk = i;
	VueceLogger::Debug("VueceMediaDataBumper - set_frame_count_of_last_chunk: %d", d->iFrameCountOfLastChunk );

	mutex_chunk_idx->Unlock();

	return 0;
}

int VueceMediaDataBumper::SetFrameDuration(int i){

	VueceMediaBumperData *d = bumper_data;

	d->iFrameDurationInMs = i;

	VueceLogger::Debug("VueceMediaDataBumper - set_frame_duration: %d ms", d->iFrameDurationInMs );

	//update
	d->lAudioChunkDurationInMs = (long)d->iFrameDurationInMs * VUECE_AUDIO_FRAMES_PER_CHUNK;

	VueceLogger::Debug("VueceMediaDataBumper - lAudioChunkDurationInMs(ms per chunk file) is updated to: %lu ms", d->lAudioChunkDurationInMs );

	return 0;
}

int VueceMediaDataBumper::SetSeekPosition(int i){

	int targetTimePosMs = i;

	VueceLogger::Debug("VueceMediaDataBumper - set_seek_position");

	//note the input arg is in seconds, need to convert it to ms
	targetTimePosMs = targetTimePosMs*1000;

	VueceLogger::Debug("VueceMediaDataBumper - set_seek_position targetTimePosMs = %d", targetTimePosMs );

	bumper_data->iSeekTargetPosInMs = targetTimePosMs;

	return 0;
}

int VueceMediaDataBumper::SetFirstFramePosition(int i)
{

	int targetTimePosMs = i;

	//convert to ms
	targetTimePosMs = targetTimePosMs * 1000;

	VueceLogger::Debug("VueceMediaDataBumper - SetFirstFramePosition targetTimePosMs = %dms", targetTimePosMs );

	bumper_data->iFirstFramePosMs = targetTimePosMs;

	return 0;
}

int VueceMediaDataBumper::StreamSeek(VueceMediaBumperData *d){

	size_t result = 0;
	int frame_len;
	int ts;
	bool bTargetFrameFound = false;
	long ltmp = 0;
	int frame_counter = 0;

	VueceBumperExternalEventNotification external_notify;

	int targetTimePosMs = d->iSeekTargetPosInMs;

	int idx = -1;

	VueceLogger::Debug("VueceMediaDataBumper - SEEK: targetTimePosMs = %d, first frame pos in ms = %d", targetTimePosMs, d->iFirstFramePosMs);

	//get actual target position
	targetTimePosMs -= d->iFirstFramePosMs;

	VueceLogger::Debug("VueceMediaDataBumper - SEEK: actual target position = %d", targetTimePosMs);

	//locate target chunk index
	idx = targetTimePosMs / d->lAudioChunkDurationInMs;

	VueceLogger::Debug("VueceMediaDataBumper - SEEK: target idx = %d, last available chunk idx = %d, current active chunk file = %d",
			idx, d->iLastAvailChunkFileIdx, d->iActiveBufFileIdx);

	if(idx > d->iLastAvailChunkFileIdx)
	{
		//this should not happen, remote seek should be handle by java layer at first.
		VueceLogger::Fatal("VueceMediaDataBumper - SEEK: FATAL ERROR! target chunk not downloaded yet, trigger remote seek operation.");
		return -1;
	}

	//chunk file located, locate the target frame now
	VueceLogger::Debug("Close current active file");

	if(d->fActiveBufferFile != NULL)
	{
		fclose(d->fActiveBufferFile);
		d->fActiveBufferFile = NULL;
//		d->iActiveBufFileIdx = -1;
	}

	//NOTE - we are doing a local seek, so we know buffered data is available,
	VueceLogger::Debug("VueceMediaDataBumper - SEEK: Buffer is marked as available");
	d->bBufferReadable = true;

	//note bumper_activate_buffer_file will automatically increase iActiveBufFileIdx
	//by 1 before opening the file so here we need to decrease it by 1 at first
	d->iActiveBufFileIdx = idx;

	VueceLogger::Debug("VueceMediaDataBumper - SEEK: Active chunk file idx is updated to: %d", d->iActiveBufFileIdx);

	d->iActiveBufFileIdx--;

	ActivateBufferFile(d);

	VueceLogger::Debug("VueceMediaDataBumper - SEEK: Target chunk file is activated, seek target frame now.");

	//reset read/frame count
	d->iReadCount = 0;

	while(true)
	{
		if(feof(d->fActiveBufferFile))
		{
			VueceLogger::Debug("End of file reached, break loop now.");
			break;
		}

		//read header
		result = fread(d->readBuf, 1, d->iFrameHeaderLen, d->fActiveBufferFile);

		//get timestamp
		ts = ByteArrayToInt(d->readBuf+5);
		VueceLogger::Debug("Read one frame, ts = %d, target ts = %d", ts, targetTimePosMs);

//		VueceLogger::Debug("VueceMediaDataBumper - stream_seek:file position: %ld", ftell(d->fActiveBufferFile));

		if(ts >= targetTimePosMs)
		{
			VueceLogger::Debug("Target frame located.");
			bTargetFrameFound = true;
			break;
		}
		else
		{
			//skip to the next frame
			frame_len = ByteArrayToInt(d->readBuf+1);

			if(fseek(d->fActiveBufferFile, frame_len, SEEK_CUR) == 0)
			{
//				VueceLogger::Debug("Skipped to next frame: %d", frame_len);
//				VueceLogger::Debug("VueceMediaDataBumper - stream_seek:file position: %ld", ftell(d->fActiveBufferFile));
			}
			else
			{
				VueceLogger::Fatal("FATAL ERROR - Skipping to next frame failed, frame len = %d", frame_len);
				return -1;
			}

			d->iReadCount++;
		}

		frame_counter++;

		VueceLogger::Debug("Seek frame counter = %d", frame_counter);

		if(frame_counter == VUECE_AUDIO_FRAMES_PER_CHUNK)
		{
			VueceLogger::Fatal("FATAL ERROR - All frames in this chunk have been checked, sth is wrong");
			break;
		}
	}

	if(!bTargetFrameFound)
	{
		VueceLogger::Fatal("FATAL ERROR!! VueceMediaDataBumper - stream_seek: Target frame not found.");
		return -1;
	}


	VueceLogger::Debug("VueceMediaDataBumper - stream_seek: Target frame located, resume bumper with the target frame.");

	d->availBufFileCounter = d->iLastAvailChunkFileIdx - d->iActiveBufFileIdx;

	VueceLogger::Debug("VueceMediaDataBumper - stream_seek: iLastAvailChunkFileIdx=%d, iActiveBufFileIdx=%d, availBufFileCounter=%d",
			d->iLastAvailChunkFileIdx, d->iActiveBufFileIdx, d->availBufFileCounter);

	//move file position indicator back by frame header length because bumper_read_frame will read
	//frame again
	ltmp = ftell(d->fActiveBufferFile);
	ltmp -= d->iFrameHeaderLen;

	VueceLogger::Debug("VueceMediaDataBumper - stream_seek: new file position is: %ld", ltmp);

	if(fseek(d->fActiveBufferFile, ltmp, SEEK_SET) == 0)
	{
		VueceLogger::Debug("VueceMediaDataBumper - stream_seek: position indicator moved back to start of frame, ready to resume bumper now.");

		VueceLogger::Debug("VueceMediaDataBumper - stream_seek:file position: %ld", ftell(d->fActiveBufferFile));
	}
	else
	{
		VueceLogger::Fatal("FATA ERROR - VueceMediaDataBumper - stream_seek: Cannot move position indicator back to start of frame, sth is wrong!");
		return -1;
	}

	//Notify audiotrack to resume playing with new data, this notification is
	//fired to StreamPlayer then forwarded to audio writer

	//dont send notification if bumper is already stopped

	mutex_bumper_state->Lock();
	if(d->bumperState == VueceBumperState_Paused)
	{
		VueceLogger::Warn("VueceMediaDataBumper - stream_seek: Bumper is paused, seek notify will not be sent.");
		mutex_bumper_state->Unlock();
		return -1;
	}
	mutex_bumper_state->Unlock();


	external_notify.event = VueceBumperExternalEvent_SEEK_FINISHED;
	external_notify.data = d->iSeekTargetPosInMs/1000;

	SignalBumperNotification(&external_notify);

	return 0;
}

int VueceMediaDataBumper::PauseBumper()
{

	VueceLogger::Debug("VueceMediaDataBumper - pause_vuece_bumper");

	StateTranstion(VueceBumperFsmEvent_Pause);

	return 0;
}

int VueceMediaDataBumper::ResumeBumper()
{
	VueceLogger::Debug("VueceMediaDataBumper - resume_vuece_bumper");

	StateTranstion(VueceBumperFsmEvent_Resume);

	return 0;
}

int VueceMediaDataBumper::MarkAsCompleted()
{

	VueceMediaBumperData *d = bumper_data;

	VueceLogger::Debug("VueceMediaDataBumper - mark_as_completed");

	d->bIsDowloadCompleted = true;

	//if download is completed, then buffer is always available
	d->bBufferReadable = true;


	mutex_chunk_idx->Lock();
	d->availBufFileCounter = d->iLastAvailChunkFileIdx - d->iActiveBufFileIdx;
	VueceLogger::Debug("VueceMediaDataBumper - mark_as_completed:last chunk id = %d, active chunk id = %d", d->iLastAvailChunkFileIdx, d->iActiveBufFileIdx);
	mutex_chunk_idx->Unlock();

	return 0;
}

int VueceMediaDataBumper::MarkAsStandalone()
{

	VueceMediaBumperData *d = bumper_data;

	VueceLogger::Debug("VueceMediaDataBumper - mark_as_standalone");

	d->bStandaloneFlag = true;

	return 0;
}


int VueceMediaDataBumper::SaveFile(VueceMediaBumperData *d)
{
	char cfilename[128];
	char mode[8];
	FILE* fTargetFile = NULL;
	int i = 0;
	size_t result_frame_len = 0;
	size_t result_write = 0;

	VueceLogger::Debug("VueceMediaDataBumper - -------------------------------");
	VueceLogger::Debug("VueceMediaDataBumper - save_file");
	VueceLogger::Debug("VueceMediaDataBumper - -------------------------------");

	if(!d->bIsDowloadCompleted)
	{
		VueceLogger::Fatal("VueceMediaDataBumper - save_file: Fatal error! File is not completed.");
		return -1;
	}

	if(d->fActiveBufferFile != NULL)
	{
		VueceLogger::Fatal("VueceMediaDataBumper - save_file: fActiveBufferFile should be null!");
	}

	d->bIsMergingFiles = true;

	memset(cfilename, 0, sizeof(cfilename));
	memset(mode, 0, sizeof(mode));

	strcpy(mode, "w");
	strcpy(cfilename, VUECE_MEDIA_AUDIO_BUFFER_LOCATION);
	strcat(cfilename, "result.aac");

	VueceLogger::Debug("VueceMediaDataBumper - save_file: file name is: %s", cfilename);
	VueceLogger::Debug("VueceMediaDataBumper - save_file: Last available chunk idx = %d", d->iLastAvailChunkFileIdx);

	fTargetFile = fopen(cfilename, mode);

	if(fTargetFile == NULL)
	{
	  VueceLogger::Fatal("VueceMediaStream - bumper_activate_buffer_file: file open failed!");
	  return -1;
	}

	VueceLogger::Debug("VueceMediaDataBumper - save_file: file opened, save data now");

	//start from the 0th chunk file
	d->iActiveBufFileIdx = -1;

	for(i = 0; i < d->iLastAvailChunkFileIdx; i++)
	{
		d->bIsChunkMerged = false;

		//open chunk file
		ActivateBufferFile(d);

		//start reading and saving frames
		while(!d->bIsChunkMerged)
		{
			result_frame_len = ReadFrame(d);
			result_write = fwrite(d->readBuf, 1, result_frame_len, fTargetFile);
		}

		VueceLogger::Debug("VueceMediaDataBumper - save_file: One chunk successfully merged, idx = %d", d->iActiveBufFileIdx);
	}

	VueceLogger::Debug("VueceMediaDataBumper - save_file: All chunks successfully merged, idx = %d", d->iActiveBufFileIdx);

  	fclose(fTargetFile);
  	fTargetFile = NULL;

  	return 0;
}

void VueceMediaDataBumper::LogFsmEvent(VueceBumperFsmEvent e)
{
	switch(e)
	{
	case VueceBumperFsmEvent_Tick:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: TICK"); break;
	case VueceBumperFsmEvent_Pause:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: PAUSE"); break;
	case VueceBumperFsmEvent_Resume:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: RESUME"); break;
	case VueceBumperFsmEvent_DataBelowThreshold:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: DATA NOT AVAILABLE"); break;
	case VueceBumperFsmEvent_AllDataConsumed:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: ALL DATA CONSUMED"); break;
	case VueceBumperFsmEvent_Stop:
		VueceLogger::Debug("VueceMediaDataBumper - Event happening: STOP"); break;
	default:
		VueceLogger::Fatal("VueceMediaDataBumper - Event happening: UNKNOW (code: %d)", (int)e); break;
	}
}


void VueceMediaDataBumper::LogFsmState(VueceBumperFsmState state)
{
	switch(state)
	{
	case VueceBumperState_Ready:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: READY");
		break;
	case VueceBumperState_Buffering:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: BUFFERING");
		break;
	case VueceBumperState_Bumping:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: BUMPING");
		break;
	case VueceBumperState_Paused:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: PAUSED");
		break;
	case VueceBumperState_Completed:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: COMPLETED");
		break;
	case VueceBumperState_Stopped:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: STOPPED");
		break;
	case VueceBumperState_Err:
		VueceLogger::Debug("VueceMediaDataBumper - Current state is: Error");
		break;
	default:
		VueceLogger::Fatal("VueceMediaDataBumper - Current state is: UNKNOWN(%d)", (int)state);
		break;
	}
}

void VueceMediaDataBumper::GetFsmEventString(VueceBumperFsmEvent e, char* event_s)
{
	switch(e)
	{
	case VueceBumperFsmEvent_Tick:
		strcpy(event_s, "TICK");
		break;
	case VueceBumperFsmEvent_Pause:
		strcpy(event_s, "PAUSE");
		break;
	case VueceBumperFsmEvent_Resume:
		strcpy(event_s, "RESUME");
		break;
	case VueceBumperFsmEvent_DataBelowThreshold:
		strcpy(event_s, "DATA NOT AVAILABLE");
		break;
	case VueceBumperFsmEvent_AllDataConsumed:
		strcpy(event_s, "ALL DATA CONSUMED");
		break;
	case VueceBumperFsmEvent_Stop:
		strcpy(event_s, "STOP");
		break;
	case VueceBumperFsmEvent_LocalSeekSucceeded:
		strcpy(event_s, "LOCAL SEEK SUCCEEDED");
		break;
	default:
		VueceLogger::Fatal("VueceMediaDataBumper - Event happening: UNKNOW (code: %d)", (int)e);
		break;
	}
}

void VueceMediaDataBumper::GetFsmStateString(VueceBumperFsmState s, char* state_s)
{
	switch(s)
	{
	case VueceBumperState_Ready:
		strcpy(state_s, "READY");
		break;
	case VueceBumperState_Buffering:
		strcpy(state_s, "BUFFERING");
		break;
	case VueceBumperState_Bumping:
		strcpy(state_s, "BUMPING");
		break;
	case VueceBumperState_Paused:
		strcpy(state_s, "PAUSED");
		break;
	case VueceBumperState_Completed:
		strcpy(state_s, "COMPLETED");
		break;
	case VueceBumperState_Stopped:
		strcpy(state_s, "STOPPED");
		break;
	case VueceBumperState_Err:
		strcpy(state_s, "ERROR");
		break;
	default:
		VueceLogger::Fatal("VueceMediaDataBumper - Current state is: UNKNOWN(%d)", (int)s);
		break;
	}
}
