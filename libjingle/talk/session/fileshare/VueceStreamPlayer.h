/*
 * VueceStreamPlayer.h
 *
 *  Created on: Sep 14, 2014
 *      Author: jingjing
 */

#ifndef VUECESTREAMPLAYER_H_
#define VUECESTREAMPLAYER_H_

#include <iostream>
#include <string>
#ifdef ANDROID
#include <jni.h>
#endif

#include "VueceConstants.h"
#include "VueceNativeInterface.h"

namespace cricket {
	class VueceMediaStreamSessionClient;
}

typedef enum _VuecePlayerFsmState{
	VuecePlayerFsmState_Ready = 0,
	VuecePlayerFsmState_Playing,
	VuecePlayerFsmState_WaitingForNextBufWin, /** this state means player is playing data and downloading next window */
	VuecePlayerFsmState_WaitingAllDataConsumed, /** this state means player is silent because all data have been used */
	VuecePlayerFsmState_Stopping,
	VuecePlayerFsmState_Stopped
}VuecePlayerFsmState;

typedef enum _StreamPlayerFsmEvent{
	StreamPlayerFsmEvent_Start = 0,
	StreamPlayerFsmEvent_BufBelowThreshold,
	StreamPlayerFsmEvent_BufWinConsumed,
	StreamPlayerFsmEvent_BufWinAvailable,
	StreamPlayerFsmEvent_Stop,
	StreamPlayerFsmEvent_Released
}StreamPlayerFsmEvent;

class VueceStreamPlayer {
public:
	static void Init();
	static void UnInit();
	static bool IsStreamingAllowed();
	static void SetStreamAllowed(bool allowed);
	static bool HasStreamEngine();

	static bool CreateStreamEngine(
			int sample_rate,
			int bit_rate,
			int nchannels,
			int duration,
			int frame_dur_ms,
			bool startup_standalone,
			bool download_finished,
			int last_avail_chunk_idx,
			int last_chunk_frame_count,
			int resume_pos
			);

	static void SetStopReason(VueceStreamPlayerStopReason reason);

	static void StartStreamEngine();

	static bool CanLocalSeekProcceed(int targetPos);
	static void StopAndResetPlayerAsync();
//	static bool CanLocalSeekProcceedCrashTest(int targetPos);

#ifdef ANDROID
	static void SetJniEnv(JNIEnv* e);
#endif

	static void SeekStream(int posInSec);

	static void ResumeStreamPlayer(int resume_pos,
			int used_sample_rate,
			int used_bit_rate,
			int used_nchanne,
			int used_duration
			);

	/*
	 * Stop audio playing and destroy audio stream instance
	 */
	static void Stop(bool forced);

	static void HandleForcedStop();

	static void StopStreamEngine();

	/*
	 *
	 */
	static void ResumeStreamBumper();

	/*
	 * Tells the bumper the index of the last chunk file and how many frames are saved in that file
	 */
	static void InjectBumperTerminationInfo(int idx_of_last_chunk_file, int num_framecount_of_last_chunk);

	static void InjectLastAvailChunkIdIntoBumper(int idx);
	/*
	 * Tells the bumper and the audio writer the start position of current PLAY session, usually it's zero, but it
	 * could be a different value if this is a remote seek
	 *
	 * NOTES
	 * 	This function should be called ONLY when a new play session is triggered because it's used to
	 * 	tell the bumper and player the start position of CURRENT play session, so if we are already playing
	 * 	but a new STREAMING SESSION is triggered because buffer window threshold is reach, this function
	 * 	SHOULD NOT be called, because the we are still in the same PLAY SESSION and the previous first frame
	 * 	position is still valid.
	 */
	static void InjectFirstFramePosition(int second);

	/*
	 * This method injects the latest stream terminate position in seconds
	 * It should be called when
	 * 1. The downloading a chunk file is completed and therefore the latest terminate position is available
	 * 2. The whole download is completed, in this case the last chunk file might not be full.
	 */
	static void InjectStreamTerminationPosition(int second, bool force_download_complete);
	static void OnStreamingCompleted(void);

	static VuecePlayerFsmState FsmState();

	static void LogCurrentFsmState();

	static bool StillInTheSamePlaySession(void);

//	static void RegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client);
//	static void UnRegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client);
//	static vuece::NetworkPlayerState GetNetworkPlayerState(void);
//	static void LogNetworkPlayerState(vuece::NetworkPlayerState state);
//	static void GetNetworkPlayerEventString(vuece::NetworkPlayerEvent e, char* buf);
//	static void GetNetworkPlayerStateString(vuece::NetworkPlayerState s, char* buf);
//	static void FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s);
//  static void SetNetworkPlayerState(vuece::NetworkPlayerState state);

	static void EnableBufWinCheckInAudioWriter();

	static void DisableBufWinCheckInAudioWriter();

	static void EnableBufWinDownloadDuringStart();

	static void LogCurrentStreamingParams();

	static void ResetCurrentStreamingParams();

	static void OnAudioWriterStateNotification(VueceStreamAudioWriterExternalEventNotification *event);

	static void OnNewDataAvailable();

	static int GetCurrentPlayingProgress(void);

	static void StopPlayerAsync();

	/**
	 * Some utility methods
	 */

	static void LogStateTranstion(const char* module, const char* event, const char* old_state, const char* new_state);
	static void AbortOnInvalidTranstion(const char* module, const char* event, const char* current_state);
	static void LogIgnoredEvent(const char* module, const char* event, const char* current_state);



private:
	static void StartPlayerMonitor();
	static bool StateTransition(StreamPlayerFsmEvent e);
	static void LogFsmEvent(StreamPlayerFsmEvent e);
	static void GetFsmStateString(VuecePlayerFsmState s, char* state_s);
	static void GetFsmEventString(StreamPlayerFsmEvent e, char* event_s);

};

#endif /* VUECESTREAMPLAYER_H_ */
