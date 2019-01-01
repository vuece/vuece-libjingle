
/*
 * VueceGlobalSetting.h
 *
 *  Created on: April 11, 2012
 *      Author: jingjing
 */

#ifndef VUECE_GLOBAL_SETTING_H
#define VUECE_GLOBAL_SETTING_H

#include "VueceConstants.h"
#include "VueceConfig.h"
#include <string.h>
#include <list>

typedef std::list<std::string> VuecePublicFolderList;

typedef struct VueceGlobalSetting
{
	int imgPreviewWidth;
	int imgPreviewHeight;

	//TODO - Inject from remote owner client
	int bShareImgPreviewEnbaled;
	VueceAppRole appRole;

	int iLastAvailableAudioChunkFileIdx;
	int iAudioFrameCounterInCurrentChunk;
	int iTotalAudioFrameCounter;

	//TODO - Remove one of these, they should be the same thing
	int iLastStreamTerminationPostionSec; //TODO - Change this to int type.
	unsigned long new_resume_pos;

	bool bIsDowloadCompleted;

	char device_name[VUECE_MAX_SETTING_VALUE_LEN+1];
	char app_version[VUECE_MAX_SETTING_VALUE_LEN+1];

	char accountUserName[VUECE_MAX_SETTING_VALUE_LEN+1];
	char accountDisplayName[VUECE_MAX_SETTING_VALUE_LEN+1];
	char accountImgUrl[512+1];
	char accountRefreshToken[VUECE_MAX_SETTING_VALUE_LEN+1];
	char accountAccessToken[VUECE_MAX_SETTING_VALUE_LEN+1];
	bool accountLinked;
	//TODO - Remove this if it's not used
	char appUserDataDir[VUECE_MAX_SETTING_VALUE_LEN+1];
	char hubName[VUECE_MAX_SETTING_VALUE_LEN+1];
	//Note this is not the jid of the hub, it's the unique ID of the hub
	char hubID[VUECE_MAX_SETTING_VALUE_LEN+1];
	char ownerHubJid[VUECE_MAX_SETTING_VALUE_LEN+1];

	char playerClientJid[VUECE_MAX_SETTING_VALUE_LEN+1];
	char playerClientAccountId[VUECE_MAX_SETTING_VALUE_LEN+1];

	int iRescanNeeded;
	int iAutoLoginAtAppStartup;
	int iAutoLoginAtSysStartup;
	int iAllowFriendAccess;
	int iMaxConcurrentStreaming;

	int iFirstFramePosSec;

	//TODO - This should be injected from owner's remote client
	size_t iMediaStreamFileMaxSizeBytes;
	int 	iMediaStreamFileMinDuration;

	VuecePublicFolderList* iPublicFolderList;

	char downloadLocation[VUECE_MAX_SETTING_VALUE_LEN+1];

	char current_music_uri[VUECE_MAX_SETTING_VALUE_LEN+1];
	char current_server_node[VUECE_MAX_SETTING_VALUE_LEN+1];

	/* A special flag used to indicate incoming session should be rejected
	 * because of this senario
	 * 1. A new play request is sent to download next buffer window, but at this point
	 * there is no session yet
	 * 2. Immediately after step 1, user issues a seek, this flag is set to true
	 * 3. When we are stopping player and releasing related resources, the session request
	 * arrives, we use this flag to cancel this incoming session
	 *
	 *
	 */
	bool bCancelSessionForNextBufWin;

	/*
	 * This flag is associated with bCancelSessionForNextBufWin, we use this flag to tell
	 * us that we don't need a 'resource-released' message from Hub, because if we send
	 * a Decline to Hub, hub won't send a 'resource-released' message
	 */
	bool bSessineDeclinedLocally;

	/**
	 * A special flag that will be set to true IMMEIDATELY when a seek is triggered
	 */
	bool bStopPlayStarted;

	/**
	 * This flag is used to detect interruption caused PC client disconnection/logout immediately after
	 * user issued a remote request (currently implemented in SEEK and SKIP)
	 *
	 * For example, if user issues a SEEK and if player is playing a song, current song playing should be stopped
	 * at first, but if remote HUB logout is detected after stopping current playing, then there is no
	 * need to send the PLAY.
	 */
	bool bIsCurrentTransactionCancelled;

}VueceGlobalSetting;

class VueceGlobalContext {
public:
	static void Init(VueceAppRole role);
	static void LogConfigData();
	static VueceGlobalSetting* Instance();

	static VueceAppRole GetAppRole();
	static void Reset();
	static void UnInit();

	static void SetCurrentMusicUri(const char* c);
	static void GetCurrentMusicUri(char* uri);

	static void SetDeviceName(const char* v);
	static void GetDeviceName(char* v);

	static void SetAppVersion(const char* v);
	static void GetAppVersion(char* v);


	//For Hub app
	static const char* GetAccountUserName();
	static void SetAccountUserName(const char* value);

	static const char* GetAccountDisplayName();
	static void SetAccountDisplayName(const char* value);

	static const char* GetAppUserDataDir();
	static void SetAppUserDataDir(const char* value);

	static const char* GetAccountImgUrl();
	static void SetAccountImgUrl(const char* value);

	static  const char* GetHubName();
	static void SetHubName(const char* name);

	static const char*  GetHubId();
	static void SetHubId(const char* id);

	static const char* GetAccountAccessToken();
	static void SetAccountAccessToken(const char* value);

	static const char* GetAccountRefreshToken();
	static void SetAccountRefreshToken(const char* value);

	static void SetRescanNeeded(bool b);
	static bool IsRescanNeeded();

	static void SetAccountLinked(bool b);
	static bool IsAccountLinked();

	static void SetAutoLoginAtSysStartup(bool b);
	static bool GetAutoLoginAtSysStartup();

	static void SetAutoLoginAtAppStartup(bool b);
	static bool GetAutoLoginAtAppStartup();

    static void SetAllowFriendAccess(bool b);
    static bool GetAllowFriendAccess();

    static void SetMaxConcurrentStreaming(int num);
    static int GetMaxConcurrentStreaming();

	static void SetCurrentTransactionCancelled(bool b);
	static bool IsCurrentTransactionCancelled();


	static int GetMediaStreamFileMaxSizeBytes();
	static int GetMediaStreamFileMinDuration();

	static VuecePublicFolderList* GetPublicFolderList();

	static void AddPublicFolderLocation(const std::string& loc);
	static void RemovePublicFolderLocation(const std::string& loc);

	//For Player app

	static const char* GetOwnerHubJid();
	static void SetOwnerHubJid(const char* jid);

	static const char* GetPlayerClientJid();
	static void SetPlayerClientJid(const char* jid);

	static const char* GetPlayerClientAccountId();
	static void SetPlayerClientAccountId(const char* id);

	static void SetDeclineSessionForNextBufWin(bool b);
	static bool ShouldDeclineSessionForNextBufWin();

	static void SetSessionDeclinedLocally(bool b);
	static bool IsSessionDeclinedLocally();

	static void SetCurrentServerNodeJid(const char* jid);
	static const char* GetCurrentServerNodeJid();

	static void SetStopPlayStarted(bool b);
	static bool IsStopPlayStarted();

	static void SetLastAvailableAudioChunkFileIdx(int i);
	static int GetLastAvailableAudioChunkFileIdx();

	static void SetFirstFramePositionSec(int i);
	static int GetFirstFramePositionSec();

	static void SetLastStreamTerminationPositionSec(int i);
	static int GetLastStreamTerminationPositionSec();

	static int GetPreviewImgWidth();
	static int GetPreviewImgHeight();

	static void SetImgPreviewEnabled(bool b);
	static bool IsImgPreviewEnabled();

	static void SetAudioFrameCounterInCurrentChunk(int i);
	static int GetAudioFrameCounterInCurrentChunk();

	static void SetTotalAudioFrameCounter(int i);
	static int GetTotalAudioFrameCounter();

	static void SetNewResumePos(int i);
	static int GetNewResumePos();

	static void SetDownloadCompleted(bool b);
	static bool IsDownloadCompleted();

};

#endif /* VUECE_GLOBAL_SETTING_H */
