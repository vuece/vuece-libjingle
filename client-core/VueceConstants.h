/*
 * vuececonstants.h
 *
 *  Created on: Dec 29, 2012
 *      Author: Jingjing Sun
 */

#ifndef VUECECONSTANTS_H_
#define VUECECONSTANTS_H_

#ifndef  NULL
#define  NULL       0
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif


typedef enum _VueceRosterSubscriptionType{
	VueceRosterSubscriptionType_NA = -1,
	VueceRosterSubscriptionType_Unavailable = 0,
	VueceRosterSubscriptionType_Subscribe,
	VueceRosterSubscriptionType_Unsubscribe,
	VueceRosterSubscriptionType_Subscribed,
	VueceRosterSubscriptionType_Unsubscribed,
	VueceRosterSubscriptionType_None
} VueceRosterSubscriptionType;

#define SUBSCRIPTION_TYPE_UNAVAILABLE 0
#define SUBSCRIPTION_TYPE_SUBSCRIBE 1
#define SUBSCRIPTION_TYPE_UNSUBSCRIBE 2
#define SUBSCRIPTION_TYPE_SUBSCRIBED 3
#define SUBSCRIPTION_TYPE_UNSUBSCRIBED 4

#define VUECE_MAX_CONCURRENT_STREAMING 5
#define VUECE_MAX_SETTING_VALUE_LEN 128+1
#define VUECE_LOG_LEVEL_DEBUG 	0
#define VUECE_LOG_LEVEL_INFO 	1
#define VUECE_LOG_LEVEL_WARN 	2
#define VUECE_LOG_LEVEL_ERROR 	3
#define VUECE_LOG_LEVEL_NONE	4

#define LOCAL_PORT_RECEIVER		3000
#define LOCAL_PORT_SENDER		4000
#define VUECE_PAYLOAD_IDX_AAC	127
#define VUECE_MAX_BUFFER_SIZE	64 * 1024 + 1024//32678 bytes
#define VUECE_MAX_PACKET_SIZE	1000 * 1024
#define VUECE_ENCODE_OUTPUT_BUFFER_SIZE 5 * 1024
#define FAKE_REMOTE_IP 					"127.0.0.1"

#define VUECE_VALUE_NOT_SET -9999

#define VUECE_FOLDER_SEPARATOR 0x1C

#define VUECE_FOLDER_SEPARATOR_STRING "\x1C"

//see http://msdn.microsoft.com/en-us/library/aa365247.aspx for the
//info regarding maximum file path length
#define VUECE_MAX_FILE_PATH_LENGTH 260
#define VUECE_MAX_FOLDER_LIST_STRING_LENGTH VUECE_MAX_FILE_PATH_LENGTH * 10

//allow 40k string length
#define VUECE_MAX_TMP_STRING_BUFFER_LENGTH 4096 * 10

#define VUECE_MEDIA_AUDIO_BUFFER_LOCATION "/sdcard/vuece/tmp/audio/"
#define VUECE_MEDIA_VIDEO_BUFFER_LOCATION "/sdcard/vuece/tmp/video/"

#define VUECE_AUTH_TYPE_PASSWORD 	1
#define VUECE_AUTH_TYPE_OAUTH 		2

//suppose frame rate is 30 frames per second, each video chunk has 10 seconds video
#define VUECE_VIDEO_FRAMES_PER_CHUNK 300

#define VUECE_MAX_FRAME_SIZE 2*1024

/*
 * This is the number of currently available chunk files, it's a threshold value that
 * allows player starts playing/consuming the data, e.g. if we set it to 1, that means
 * when at least 1 chunk file is downloaded, the bumper will start pumping data
 * to decoder, music playing will be started
 *
 * The value of this macro must be >= 1
 */
#define VUECE_BUFFER_AVAIL_INDICATOR 1

////////////////////////////////////////////////////
//Following properties are now injected from VueceConfig.h
//These values are tested so keep the code as a future reference
///*
// * Number of chunk files that will be downloaded in each stream session
// */
//#define VUECE_BUFFER_WINDOW 7//25
//
//Usually duration of one frame is about 20ms, we want each chunk file contains ~10 second frames
//#define VUECE_AUDIO_FRAMES_PER_CHUNK 500
//
////Trigger new download when 30 seconds data left
//#define VUECE_BUFWIN_THRESHOLD_SEC 30
//
////This is used to filter away large file if its size is above predefined value
//#define VUECE_MAX_MUSIC_FILE_SIZE_MB 			200
//
////This is used to filter away small files if the duration is below predefined value
//#define VUECE_MIN_MUSIC_DURATION_SEC         5
//
//#define VUECE_TIMEOUT_WAIT_SESSION_RELEASED 15
//#define VUECE_SESSION_MGR_TIMEOUT 20
//
/////////////////////////////////////////////////////////////

/*
 * Frame header format
 *  [Signal] [FrameLength] [TimeStamp]
 *		1		 4				4
 */
#define VUECE_STREAM_FRAME_HEADER_LENGTH 9

#define VUECE_CHUNK_BUF_LEN VUECE_MAX_FRAME_SIZE * VUECE_AUDIO_FRAMES_PER_CHUNK

#define VUECE_STREAM_PACKET_TYPE_EOF	0
#define VUECE_STREAM_PACKET_TYPE_AUDIO 	1
#define VUECE_STREAM_PACKET_TYPE_VIDEO 	2

#define CLIENT_STATE_STANDBY 0
#define CLIENT_EVENT_CONNECTION_START 1
#define CLIENT_EVENT_CONNECTION_OPENING 2
#define CLIENT_EVENT_SIGNED_IN 3
#define CLIENT_EVENT_SIGNED_OUT 4
#define CLIENT_EVENT_AUTH_FAILED 5
#define CLIENT_EVENT_CONNECTION_FAILED 6

#define MAX_MESSAGE_BUFFER_SIZE 50000

#define VHUB_MSG_STREAMINGRES_RELEASED "streaming-resource-released"
#define VHUB_MSG_STREAMING_TARGET_INVALID "Target file is invalid"
#define VHUB_MSG_BUSY_NOW "Hub is busy now"

//Vuece constant commands
#define VUECE_CMD_SIGN_IN "sign-in"

#define VUECE_CMD_BUDDY_STATUS_UPDATE "buddy-status-update"
#define VUECE_CMD_SEND_CHAT "send-chat"
#define VUECE_CMD_RECEIVE_CHAT "receive-chat"
#define VUECE_CMD_SEND_PRESENCE "send-presence"
#define VUECE_CMD_SEND_SIGNATURE "send-signature"
#define VUECE_CMD_SEND_STATUS_SIGNATURE "send-status-signature"
#define VUECE_CMD_SEND_VOICE_CALL_REQUEST "send-voice-call-req"
#define VUECE_CMD_SEND_VOICE_CALL_ACCEPT "send-voice-call-accept"
#define VUECE_CMD_SEND_VOICE_CALL_REJECT "send-voice-call-reject"
#define VUECE_CMD_SEND_VOICE_CALL_END "send-voice-call-end"
#define VUECE_CMD_SEND_FS_REQUEST "send-fs-req"
#define VUECE_CMD_SEND_FS_ACCEPT  "send-fs-accept"
#define VUECE_CMD_SEND_FS_DECLINE  "send-fs-decline"
#define VUECE_CMD_SEND_FS_CANCEL "send-fs-cancel"

#define VUECE_CMD_RECEIVE_ROSTER_LIST "recv-roster-list"
#define VUECE_CMD_RECEIVE_VOICE_CALL_REQUEST "receive-voice-call-req"
#define VUECE_CMD_RECEIVE_VOICE_CALL_REJECTED "receive-voice-call-rejected"
#define VUECE_CMD_RECEIVE_VOICE_CALL_ACCEPTED "receive-voice-call-accepted"
#define VUECE_CMD_RECEIVE_VOICE_CALL_ENDED "receive-voice-call-ended"
#define VUECE_CMD_RECEIVE_VCARD "receive-vcard"
#define VUECE_CMD_RECEIVE_FS_OFFER "receive-fs-offer"
#define VUECE_CMD_RECEIVE_FS_ACCEPTED "receive-fs-accepted"
#define VUECE_CMD_RECEIVE_FS_REJECTED "receive-fs-rejected"
#define VUECE_CMD_RECEIVE_FS_PROGRESS_UPDATE "receive-fs-progress-update"
#define VUECE_CMD_RECEIVE_FS_PREVIEW "receive-fs-preview"
#define VUECE_CMD_RECEIVE_FS_STATE "receive-fs-state"

//Vuece constant strings
#define VUECE_STR_CMD "cmd"
#define VUECE_STR_PWD "pwd"
#define VUECE_STR_RESP "resp"
#define VUECE_STR_SIGN_IN_RESP "sign-in-resp"
#define VUECE_STR_OK "ok"
#define VUECE_STR_ROSTER_LIST "roster-list"
#define VUECE_STR_JID "jid"
#define VUECE_STR_JINGLE_SESSION_ID "jingleSessionId"
#define VUECE_STR_SUB "sub"
#define VUECE_STR_NAME "name"
#define VUECE_STR_STATUS "status"
#define VUECE_STR_ICON "icon"
#define VUECE_STR_PRIORITY "priority"
#define VUECE_STR_SHOW "show"
#define VUECE_STR_PHONE_CAP "phoneCap"
#define VUECE_STR_VIDEO_CAP "videoCap"
#define VUECE_STR_CAM_CAP "camCap"
#define VUECE_STR_PMUC_CAP "pmucCap"
#define VUECE_STR_SHARE_CAP "shareCap"
#define VUECE_STR_MESSAGE "message"
#define VUECE_STR_FNAME "fname"
#define VUECE_STR_PHOTO "photo"
#define VUECE_STR_FILE_PATH "filePath"
#define VUECE_STR_PREVIEW_FILE_PATH "filePreviewPath"
#define VUECE_STR_PROGRESS "progress"
#define VUECE_STR_STATE "state"
#define VUECE_STR_SIZE "size"
#define VUECE_STR_FILE_NAME "fileName"
#define VUECE_STR_WIDTH "width"
#define VUECE_STR_HEIGHT "height"


#define USER_DATA_TAG_ALLOW_FRIEND_ACCESS "allow_friend_ac"
#define USER_DATA_TAG_MAX_CONCURRENT_STREAMING "max_concurrent_str"
#define USER_DATA_TAG_PUBLIC_FOLDERS "publicfolders"

enum VueceCmd{
	VUECE_MSG_START = 0,
	VUECE_CMD_SEND_CHAT_MSG,
	VUECE_CMD_PLACE_CALL,
	VUECE_CMD_ACCEPT_CALL,
	VUECE_CMD_REJECT_CALL,
	VUECE_CMD_HANG_UP,
	VUECE_CMD_PLACE_VOICE_CALL_TO_REMOTE_TARGET,
	VUECE_CMD_PLACE_VIDEO_CALL_TO_REMOTE_TARGET,
	VUECE_CMD_SEND_FILE,
	VUECE_CMD_PLAY_MUSIC,
	VUECE_CMD_PAUSE_MUSIC,
	VUECE_CMD_RESUME_MUSIC,
	VUECE_CMD_SEEK_MUSIC,
	VUECE_CMD_CANCEL_FILE,
	VUECE_CMD_ACCEPT_FILE,
	VUECE_CMD_DECLINE_FILE,
	VUECE_CMD_SIGN_OUT,
//	VUECE_CMD_STOP_STREAM_PLAYER,
//	VUECE_CMD_RESUME_STREAM_PLAYER,
//	VUECE_CMD_MEDIASTREAM_SEEK,
	VUECE_MSG_PING,
	VUECE_CMD_END
};

typedef enum VueceAppRole{
	VueceAppRole_Normal,
	VueceAppRole_Media_Hub,
	VueceAppRole_Media_Hub_Client
} VueceAppRole;

typedef enum VueceStreamingMode{
	VueceStreamingMode_Normal,
	VueceStreamingMode_Music
} VueceStreamingMode;


typedef enum VueceStreamType{
	VueceStreamType_File,
	VueceStreamType_Music,
	VueceStreamType_None
} VueceStreamType;


typedef enum VueceStreamSessionInitErrType{
	VueceStreamSessionInitErrType_OK = 0,
	VueceStreamSessionInitErrType_NodeBusy,
	VueceStreamSessionInitErrType_FileAbsent,
	VueceStreamSessionInitErrType_NotAFile,
	VueceStreamSessionInitErrType_EmptyFile,
	VueceStreamSessionInitErrType_SysErr,
	VueceStreamSessionInitErrType_None
} VueceStreamSessionInitErrType;


typedef enum VueceEvent{
	VueceEvent_Client_SignedIn = 100,
	VueceEvent_Client_SignedOut,
	VueceEvent_Client_AuthFailed,
	VueceEvent_Client_BackOnLine,
	VueceEvent_Client_Destroyed,

	VueceEvent_FileAccess_Denied = 200,

	VueceEvent_Connection_Started = 300,
	VueceEvent_Connection_Failed,
	VueceEvent_Connection_FailedWithAutoReconnect,

	VueceEvent_None
} VueceEvent;


#define VUECE_ANDROID_CHANNEL_CONFIGURATION_MONO 	2
#define VUECE_ANDROID_CHANNEL_CONFIGURATION_STEREO 	3

//see http://developer.android.com/reference/android/media/AudioManager.html#STREAM_VOICE_CALL
#define VUECE_ANDROID_AUDIO_STREAM_MODE_VOICE_CALL 	0
#define VUECE_ANDROID_AUDIO_STREAM_MODE_MUSIC 	3

//see same definitions in VTalkListener.java
typedef enum _Vuece_StreamPlayerEvent{
	VueceStreamPlayerEvent_Stopped = 0,
	//not used for now, commented away to avoid confusion
	VueceStreamPlayerEvent_Buffering,
	VueceStreamPlayerEvent_Playing,
//	VueceStreamPlayerEvent_Paused,
	VueceStreamPlayerEvent_Completed,
	VueceStreamPlayerEvent_Timedout
}VueceStreamPlayerEvent;


typedef enum _Vuece_StreamPlayerRequest{
	VueceStreamPlayerRequest_Download_Next_BufWin = 0,
	//not used for now, commented away to avoid confusion
}VueceStreamPlayerRequest;

typedef enum _VueceStreamAudioWriterExternalEvent{
	VueceStreamAudioWriterExternalEvent_Playing = 0,
	VueceStreamAudioWriterExternalEvent_BufWindowThresholdReached,
	VueceStreamAudioWriterExternalEvent_BufWinConsumed,
	VueceStreamAudioWriterExternalEvent_Completed,
	VueceStreamAudioWriterExternalEvent_WatchdogExpired
}VueceStreamAudioWriterExternalEvent;

typedef enum _VueceStreamPlayerNotificationType{
//	VueceStreamPlayerNotificationType_Event = 0,
	VueceStreamPlayerNotificationType_StateChange = 0,
//	VueceStreamPlayerNotificationType_NetWorkPlayerEvent,
	VueceStreamPlayerNotificationType_Request
}VueceStreamPlayerNotificationType;

typedef enum _VueceStreamPlayerStopReason{
	VueceStreamPlayerStopReason_Completed = 0,
	VueceStreamPlayerStopReason_PausedByUser,
	VueceStreamPlayerStopReason_WatchdogExpired,
	VueceStreamPlayerStopReason_NetworkErr
}VueceStreamPlayerStopReason;

/**
 * Events used to notify outside entity, external use only
 */

typedef enum _VueceBumperExternalEvent{
	VueceBumperExternalEvent_SEEK_FINISHED = 0,
	VueceBumperExternalEvent_BUFFERING,
	VueceBumperExternalEvent_DATA_AVAILABLE,
	VueceBumperExternalEvent_COMPLETED,
	VueceBumperExternalEvent_NONE,
}VueceBumperExternalEvent;


/**
 * This structure is used to send notification to outside
 * entity a bumper state change
 */
typedef struct _VueceBumperExternalEventNotification{
	VueceBumperExternalEvent event;
	int data;
}VueceBumperExternalEventNotification;

//State event used between the native audio writer and VueceStreamPlayer
typedef struct _VueceStreamAudioWriterExternalEventNotification{
	VueceStreamAudioWriterExternalEvent id;
	int value;

}VueceStreamAudioWriterExternalEventNotification;

//Notification used between VueceStreamPlayer and session client, session client uses
//such notification to trigger further event up to application layer
typedef struct _VueceStreamPlayerNotificaiontMsg{
	VueceStreamPlayerNotificationType notificaiont_type; // type of notification, this could be a command or an event
	VueceStreamPlayerRequest request_type;
	int value1;
	int value2;
}VueceStreamPlayerNotificaiontMsg;

#define MAX_LEN_USER_ID 64
#define MAX_LEN_USER_NAME 64
#define MAX_LEN_DEVICE 64
#define MAX_LEN_SESSION_ID 24
#define MAX_LEN_FILE_NAME 128

typedef enum _VueceRemoteDeviceActivityType{
	VueceRemoteDeviceActivityType_StreamingStarted = 0,
	VueceRemoteDeviceActivityType_StreamingTerminated,
	VueceRemoteDeviceActivityType_None
} VueceRemoteDeviceActivityType;

typedef struct _VueceStreamingDevice{
	VueceRemoteDeviceActivityType activity;
	//full jid
	char user_id[MAX_LEN_USER_ID+1]; // full jid
	char user_name[MAX_LEN_USER_NAME+1]; //user name
	char device_name[MAX_LEN_DEVICE+1];
	char session_id[MAX_LEN_SESSION_ID+1];
	char file_url[MAX_LEN_FILE_NAME+1];
}VueceStreamingDevice;

typedef struct _VueceRosterSubscriptionMsg{
	VueceRosterSubscriptionType subscribe_type;
	char user_id[MAX_LEN_USER_ID+1]; // full jid
}VueceRosterSubscriptionMsg;


#endif /* VUECECONSTANTS_H_ */
