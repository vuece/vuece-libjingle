/*
 * VueceGlobalContext.cc
 *
 *  Created on: May 9, 2015
 *      Author: jingjing
 */

#include "talk/base/logging.h"

#include "VueceGlobalSetting.h"
#include "VueceLogger.h"
#include "VueceThreadUtil.h"

#include "jthread.h"

static VueceGlobalSetting* pVueceGlobalSetting;
static JMutex mutex_var;

//This function is only called by windows pc app
void VueceGlobalContext::LogConfigData()
{
	LOG_F(INFO) << "VUECE_BUFFER_WINDOW = " << VUECE_BUFFER_WINDOW;
	LOG_F(INFO) << "VUECE_BUFWIN_THRESHOLD_SEC = "<< VUECE_BUFWIN_THRESHOLD_SEC;
	LOG_F(INFO) << "VUECE_MAX_MUSIC_FILE_SIZE_MB = "<< VUECE_MAX_MUSIC_FILE_SIZE_MB;
	LOG_F(INFO) << "VUECE_MIN_MUSIC_DURATION_SEC = "<< VUECE_MIN_MUSIC_DURATION_SEC;
	LOG_F(INFO) << "VUECE_TIMEOUT_WAIT_SESSION_RELEASED = "<< VUECE_TIMEOUT_WAIT_SESSION_RELEASED;
	LOG_F(INFO) << "VUECE_SESSION_MGR_TIMEOUT = "<< VUECE_SESSION_MGR_TIMEOUT;

	LOG_F(INFO) << "pVueceGlobalSetting->imgPreviewWidth = " << pVueceGlobalSetting->imgPreviewWidth;
	LOG_F(INFO) << "pVueceGlobalSetting->imgPreviewHeight = " << pVueceGlobalSetting->imgPreviewHeight;
	LOG_F(INFO) << "pVueceGlobalSetting->appRole = " << pVueceGlobalSetting->appRole;
	LOG_F(INFO) << "pVueceGlobalSetting->iMaxConcurrentStreaming = " << pVueceGlobalSetting->iMaxConcurrentStreaming;
	LOG_F(INFO) << "pVueceGlobalSetting->bShareImgPreviewEnbaled = " << pVueceGlobalSetting->bShareImgPreviewEnbaled;
	LOG_F(INFO) << "pVueceGlobalSetting->iRescanNeeded = " << pVueceGlobalSetting->iRescanNeeded;
	LOG_F(INFO) << "pVueceGlobalSetting->iAutoLoginAtSysStartup = " << pVueceGlobalSetting->iAutoLoginAtSysStartup;
	LOG_F(INFO) << "pVueceGlobalSetting->iAutoLoginAtAppStartup = " << pVueceGlobalSetting->iAutoLoginAtAppStartup;
	LOG_F(INFO) << "pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes = " << pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes;
	LOG_F(INFO) << "pVueceGlobalSetting->iMediaStreamFileMinDuration = " << pVueceGlobalSetting->iMediaStreamFileMinDuration;

	LOG_F(INFO) << "appUserDataDir: " << VueceGlobalContext::GetAppUserDataDir();
	LOG_F(INFO) << "accountUserName: " << VueceGlobalContext::GetAccountUserName();
	LOG_F(INFO) << "accountDisplayName: " << VueceGlobalContext::GetAccountDisplayName();
	LOG_F(INFO) << "accountImgUrl: " << VueceGlobalContext::GetAccountImgUrl();
	LOG_F(INFO) << "accountRefreshToken: " <<VueceGlobalContext::GetAccountRefreshToken();
	LOG_F(INFO) << "hubName: " << VueceGlobalContext::GetHubName();
	LOG_F(INFO) << "hubID: " <<VueceGlobalContext::GetHubId();
	LOG_F(INFO) << "number of public folders: " << VueceGlobalContext::GetPublicFolderList()->size();
	LOG_F(INFO) << "re-scan needed: " << VueceGlobalContext::IsRescanNeeded();
	LOG_F(INFO) << "auto login at app startup: " << VueceGlobalContext::GetAutoLoginAtAppStartup();
	LOG_F(INFO) << "auto login at system startup: " << VueceGlobalContext::GetAutoLoginAtSysStartup();
	LOG_F(INFO) << "maximum concurrent streaming: " << VueceGlobalContext::GetMaxConcurrentStreaming();
	LOG_F(INFO) << "allow friend access: " << VueceGlobalContext::GetAllowFriendAccess();
}

void VueceGlobalContext::Init(VueceAppRole role)
{

	VueceLogger::Info("VueceGlobalContext::Init");

	VueceThreadUtil::InitMutex(&mutex_var);

	pVueceGlobalSetting = (struct VueceGlobalSetting *) malloc( sizeof( struct VueceGlobalSetting ));

	memset(pVueceGlobalSetting, 0, sizeof(struct VueceGlobalSetting));

	pVueceGlobalSetting->bShareImgPreviewEnbaled = 1; // this is needed to display album art
	//Note 310 is specific to UI display, for desktop, 310 is the best and maximum preview width
	//preview height here is actually not used, it will be determined/scaled based on actual image size
	pVueceGlobalSetting->imgPreviewWidth = 310;
	pVueceGlobalSetting->imgPreviewHeight = 240;
	pVueceGlobalSetting->appRole = role;
	pVueceGlobalSetting->iMaxConcurrentStreaming = 1;

	if(pVueceGlobalSetting->appRole  == VueceAppRole_Media_Hub)
	{
		VueceLogger::Info("VueceGlobalContext::Init - Role is HUB");

		pVueceGlobalSetting->iPublicFolderList = new VuecePublicFolderList();
		pVueceGlobalSetting->bShareImgPreviewEnbaled = true;
		pVueceGlobalSetting->iRescanNeeded = 1;
		pVueceGlobalSetting->iAutoLoginAtSysStartup = 1;
		pVueceGlobalSetting->iAutoLoginAtAppStartup = 0;
		pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes = VUECE_MAX_MUSIC_FILE_SIZE_MB * 1024 * 1024;//200 * 1000* 1000; //200 MB
		pVueceGlobalSetting->iMediaStreamFileMinDuration = VUECE_MIN_MUSIC_DURATION_SEC;

		//Log configurations
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_BUFFER_WINDOW: %d", VUECE_BUFFER_WINDOW);
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_BUFWIN_THRESHOLD_SEC: %d", VUECE_BUFWIN_THRESHOLD_SEC);
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_MAX_MUSIC_FILE_SIZE_MB: %d", VUECE_MAX_MUSIC_FILE_SIZE_MB);
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_MIN_MUSIC_DURATION_SEC: %d", VUECE_MIN_MUSIC_DURATION_SEC);
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_TIMEOUT_WAIT_SESSION_RELEASED: %d", VUECE_TIMEOUT_WAIT_SESSION_RELEASED);
		VueceLogger::Info("VueceGlobalContext::Init - VUECE_SESSION_MGR_TIMEOUT: %d", VUECE_SESSION_MGR_TIMEOUT);
	}

	VueceLogger::Info("VueceGlobalContext::Done");

}

VueceGlobalSetting* VueceGlobalContext::Instance()
{
	return pVueceGlobalSetting;
}


void VueceGlobalContext::Reset()
{
	VueceLogger::Info("VueceGlobalContext::Reset");
	memset(pVueceGlobalSetting, 0, sizeof(struct VueceGlobalSetting));
}


void VueceGlobalContext::UnInit()
{
	VueceLogger::Info("VueceGlobalContext::UnInit");

	if(mutex_var.IsInitialized() )
	{
		mutex_var.Unlock();
	}

	VueceLogger::Info("VueceGlobalContext::UnInit - Done");
}

VueceAppRole VueceGlobalContext::GetAppRole()
{
	return pVueceGlobalSetting->appRole;
}

void VueceGlobalContext::SetRescanNeeded(bool b)
{

	pVueceGlobalSetting->iRescanNeeded = (int)b;
}

bool VueceGlobalContext::IsRescanNeeded()
{
	if(pVueceGlobalSetting->iRescanNeeded == 1)
	{
		return true;
	}
	else
	{
		return false;
	}
}


void VueceGlobalContext::SetAccountLinked(bool b)
{
	pVueceGlobalSetting->accountLinked = b;
}
bool VueceGlobalContext::IsAccountLinked()
{
	return pVueceGlobalSetting->accountLinked;
}


VuecePublicFolderList* VueceGlobalContext::GetPublicFolderList()
{
	return pVueceGlobalSetting->iPublicFolderList;
}


void VueceGlobalContext::AddPublicFolderLocation(const std::string& loc)
{
	LOG(LS_VERBOSE) << "VueceGlobalContext:AddPublicFolderLocation: " << loc;

	VuecePublicFolderList* iPublicFolderList = VueceGlobalContext::GetPublicFolderList();
	iPublicFolderList->push_back(loc);
	SetRescanNeeded(true);
}

void VueceGlobalContext::RemovePublicFolderLocation(const std::string& loc)
{
	LOG(LS_VERBOSE) << "VueceGlobalContext:RemovePublicFolderLocation: " << loc;

	VuecePublicFolderList* iPublicFolderList = VueceGlobalContext::GetPublicFolderList();

	std::list<std::string>::iterator i = iPublicFolderList->begin();
	while (i != iPublicFolderList->end())
	{
		if(*i == loc)
		{
			LOG(LS_VERBOSE) << "VueceGlobalContext:Folder located in the list, remove it now.";
			iPublicFolderList->erase(i);
			break;
		}

		++i;
	}

	SetRescanNeeded(true);

	LOG(LS_VERBOSE) << "VueceGlobalContext:RemovePublicFolderLocation:Current share folder list size: " << iPublicFolderList->size();
}

void VueceGlobalContext::SetAutoLoginAtSysStartup(bool b)
{
	pVueceGlobalSetting->iAutoLoginAtSysStartup = b;
}

bool VueceGlobalContext::GetAutoLoginAtSysStartup()
{
	return pVueceGlobalSetting->iAutoLoginAtSysStartup;
}

void VueceGlobalContext::SetAutoLoginAtAppStartup(bool b)
{
	pVueceGlobalSetting->iAutoLoginAtAppStartup = b;
}

bool VueceGlobalContext::GetAutoLoginAtAppStartup()
{
	return pVueceGlobalSetting->iAutoLoginAtAppStartup;
}

void VueceGlobalContext::SetAllowFriendAccess(bool b)
{
	pVueceGlobalSetting->iAllowFriendAccess = b;
}

bool VueceGlobalContext::GetAllowFriendAccess()
{
	return pVueceGlobalSetting->iAllowFriendAccess;
}

void VueceGlobalContext::SetMaxConcurrentStreaming(int num)
{
	pVueceGlobalSetting->iMaxConcurrentStreaming = num;
}

int VueceGlobalContext::GetMaxConcurrentStreaming()
{
	return pVueceGlobalSetting->iMaxConcurrentStreaming;
}

void VueceGlobalContext::SetCurrentTransactionCancelled(bool b)
{
	mutex_var.Lock();

	pVueceGlobalSetting->bIsCurrentTransactionCancelled = b;

	mutex_var.Unlock();
}

bool VueceGlobalContext::IsCurrentTransactionCancelled()
{
	bool b = false;

	mutex_var.Lock();
	b = pVueceGlobalSetting->bIsCurrentTransactionCancelled;
	mutex_var.Unlock();

	return b;
}

int VueceGlobalContext::GetMediaStreamFileMaxSizeBytes()
{
	return pVueceGlobalSetting->iMediaStreamFileMaxSizeBytes;
}

int VueceGlobalContext::GetMediaStreamFileMinDuration()
{
	return pVueceGlobalSetting->iMediaStreamFileMinDuration;
}

void VueceGlobalContext::SetCurrentServerNodeJid(const char* jid)
{
	VueceLogger::Debug("VueceGlobalContext::SetCurrentServerNodeJid - %s", jid);

	strcpy(pVueceGlobalSetting->current_server_node, jid);
}

const char* VueceGlobalContext::GetCurrentServerNodeJid()
{
	VueceLogger::Debug("VueceGlobalContext::GetCurrentServerNodeJid");
	return (const char*)pVueceGlobalSetting->current_server_node;
}

const char* VueceGlobalContext::GetHubName()
{
	VueceLogger::Debug("VueceGlobalContext::GetHubName");
	return (const char*)pVueceGlobalSetting->hubName;
}

void VueceGlobalContext::SetHubName(const char* name)
{
	VueceLogger::Debug("VueceGlobalContext::SetHubName - %s", name);
	strcpy(pVueceGlobalSetting->hubName, name);
}


const char* VueceGlobalContext::GetOwnerHubJid()
{
	VueceLogger::Debug("VueceGlobalContext::GetOwnerHubJid");
	return (const char*)pVueceGlobalSetting->ownerHubJid;
}

void VueceGlobalContext::SetOwnerHubJid(const char* jid)
{
	VueceLogger::Debug("VueceGlobalContext::SetOwnerHubJid - %s", jid);
	strcpy(pVueceGlobalSetting->ownerHubJid, jid);
}

const char* VueceGlobalContext::GetPlayerClientJid()
{
	VueceLogger::Debug("VueceGlobalContext::GetPlayerClientJid");
	return (const char*)pVueceGlobalSetting->playerClientJid;
}

void VueceGlobalContext::SetPlayerClientJid(const char* jid)
{
	VueceLogger::Debug("VueceGlobalContext::SetPlayerClientJid - %s", jid);
	strcpy(pVueceGlobalSetting->playerClientJid, jid);
}

const char* VueceGlobalContext::GetPlayerClientAccountId()
{
	VueceLogger::Debug("VueceGlobalContext::GetPlayerClientAccountId");
	return (const char*)pVueceGlobalSetting->playerClientAccountId;
}

void VueceGlobalContext::SetPlayerClientAccountId(const char* id)
{
	VueceLogger::Debug("VueceGlobalContext::SetPlayerClientAccountId - %s", id);
	strcpy(pVueceGlobalSetting->playerClientAccountId, id);
}

void VueceGlobalContext::SetHubId(const char* id)
{
	VueceLogger::Debug("VueceGlobalContext::SetHubId");
	strcpy(pVueceGlobalSetting->hubID, id);
}

const char* VueceGlobalContext::GetHubId()
{
	VueceLogger::Debug("VueceGlobalContext::GetHubId");
//	strcpy(id, pVueceGlobalSetting->hubID);
	return (const char*)pVueceGlobalSetting->hubID;
}

const char* VueceGlobalContext::GetAccountUserName()
{
	return (const char*)pVueceGlobalSetting->accountUserName;
}
void VueceGlobalContext::SetAccountUserName(const char* value)
{
	strcpy(pVueceGlobalSetting->accountUserName, value);
}

const char* VueceGlobalContext::GetAccountDisplayName()
{
	return (const char*)pVueceGlobalSetting->accountDisplayName;
}
void VueceGlobalContext::SetAccountDisplayName(const char* value)
{
	strcpy(pVueceGlobalSetting->accountDisplayName, value);
}

const char* VueceGlobalContext::GetAccountImgUrl()
{
	return (const char*)pVueceGlobalSetting->accountImgUrl;
}
void VueceGlobalContext::SetAccountImgUrl(const char* value)
{
	strcpy(pVueceGlobalSetting->accountImgUrl, value);
}

const char* VueceGlobalContext::GetAppUserDataDir()
{
	return (const char*)pVueceGlobalSetting->appUserDataDir;
}

void VueceGlobalContext::SetAppUserDataDir(const char* value)
{
	strcpy(pVueceGlobalSetting->appUserDataDir, value);
}


const char* VueceGlobalContext::GetAccountAccessToken()
{
	VueceLogger::Debug("VueceGlobalContext::GetAccountAccessToken");
//	strcpy(value, pVueceGlobalSetting->accountAccessToken);
	return (const char*)pVueceGlobalSetting->accountAccessToken;
}

void VueceGlobalContext::SetAccountAccessToken(const char* value)
{
	VueceLogger::Debug("VueceGlobalContext::SetAccountAccessToken");
	strcpy(pVueceGlobalSetting->accountAccessToken, value);
}

const char* VueceGlobalContext::GetAccountRefreshToken()
{
	return (const char*)pVueceGlobalSetting->accountRefreshToken;
}

void VueceGlobalContext::SetAccountRefreshToken(const char* value)
{
	strcpy(pVueceGlobalSetting->accountRefreshToken, value);
}


void VueceGlobalContext::SetCurrentMusicUri(const char* c)
{
	VueceLogger::Debug("VueceGlobalContext::SetCurrentMusicUri");
	strcpy(pVueceGlobalSetting->current_music_uri, c);
}

void VueceGlobalContext::GetCurrentMusicUri(char* uri)
{
	VueceLogger::Debug("VueceGlobalContext::GetCurrentMusicUri");
	strcpy(uri, pVueceGlobalSetting->current_music_uri);
}


void VueceGlobalContext::SetDeviceName(const char* v)
{
	VueceLogger::Debug("VueceGlobalContext::SetDeviceName - %s", v);
	strcpy(pVueceGlobalSetting->device_name, v);
}

void VueceGlobalContext::GetDeviceName(char* v)
{
	strcpy(v, pVueceGlobalSetting->device_name);
}

void VueceGlobalContext::SetAppVersion(const char* v)
{
	VueceLogger::Debug("VueceGlobalContext::SetAppVersion - %s", v);
	strcpy(pVueceGlobalSetting->app_version, v);
}

void VueceGlobalContext::GetAppVersion(char* v)
{
	strcpy(v, pVueceGlobalSetting->app_version);
}

void VueceGlobalContext::SetDeclineSessionForNextBufWin(bool b)
{
	mutex_var.Lock();

	pVueceGlobalSetting->bCancelSessionForNextBufWin = b;

	if(pVueceGlobalSetting->bCancelSessionForNextBufWin)
	{
		VueceLogger::Info("VueceGlobalContext::SetDeclineSessionForNextBufWin[TRICKYDEBUG] - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::SetDeclineSessionForNextBufWin[TRICKYDEBUG] - False");
	}

	mutex_var.Unlock();
}

bool VueceGlobalContext::ShouldDeclineSessionForNextBufWin()
{
	bool ret = false;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->bCancelSessionForNextBufWin;

	if(ret)
	{
		VueceLogger::Info("VueceGlobalContext::ShouldDeclineSessionForNextBufWin[TRICKYDEBUG] - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::ShouldDeclineSessionForNextBufWin[TRICKYDEBUG] - False");
	}

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetSessionDeclinedLocally(bool b)
{
	mutex_var.Lock();

	pVueceGlobalSetting->bSessineDeclinedLocally = b;

	if(pVueceGlobalSetting->bSessineDeclinedLocally)
	{
		VueceLogger::Info("VueceGlobalContext::SetSessionDeclinedLocally - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::SetSessionDeclinedLocally - False");
	}

	mutex_var.Unlock();
}

bool VueceGlobalContext::IsSessionDeclinedLocally()
{
	bool ret = false;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->bSessineDeclinedLocally;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetStopPlayStarted(bool b)
{
	mutex_var.Lock();

	pVueceGlobalSetting->bStopPlayStarted = b;

	if(pVueceGlobalSetting->bStopPlayStarted)
	{
		VueceLogger::Info("VueceGlobalContext::SetStopPlayStarted - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::SetStopPlayStarted - False");
	}

	mutex_var.Unlock();
}

bool VueceGlobalContext::IsStopPlayStarted()
{
	bool ret = false;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->bStopPlayStarted;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetLastAvailableAudioChunkFileIdx(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->iLastAvailableAudioChunkFileIdx = i;

	VueceLogger::Info("VueceGlobalContext::SetLastAvailableAudioChunkFileIdx - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetLastAvailableAudioChunkFileIdx()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->iLastAvailableAudioChunkFileIdx;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetFirstFramePositionSec(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->iFirstFramePosSec = i;

	VueceLogger::Info("VueceGlobalContext::SetFirstFramePositionSec - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetFirstFramePositionSec()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->iFirstFramePosSec;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetNewResumePos(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->new_resume_pos = i;

	VueceLogger::Info("VueceGlobalContext::SetNewResumePos - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetNewResumePos()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->new_resume_pos;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetLastStreamTerminationPositionSec(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->iLastStreamTerminationPostionSec = i;

	VueceLogger::Info("VueceGlobalContext::SetLastStreamTerminationPositionSec - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetLastStreamTerminationPositionSec()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->iLastStreamTerminationPostionSec;

	mutex_var.Unlock();

	return ret;
}

int VueceGlobalContext::GetPreviewImgWidth()
{
	int ret = VUECE_VALUE_NOT_SET;

	ret = pVueceGlobalSetting->imgPreviewWidth;

	return ret;
}

int VueceGlobalContext::GetPreviewImgHeight()
{
	int ret = VUECE_VALUE_NOT_SET;

	ret = pVueceGlobalSetting->imgPreviewHeight;

	return ret;
}

void VueceGlobalContext::SetImgPreviewEnabled(bool b)
{

	pVueceGlobalSetting->bShareImgPreviewEnbaled = b;

	if(pVueceGlobalSetting->bShareImgPreviewEnbaled)
	{
		VueceLogger::Info("VueceGlobalContext::SetImgPreviewEnabled - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::SetImgPreviewEnabled - False");
	}

}

bool VueceGlobalContext::IsImgPreviewEnabled()
{
	bool ret = false;

	ret = pVueceGlobalSetting->bShareImgPreviewEnbaled;

	return ret;
}

void VueceGlobalContext::SetDownloadCompleted(bool b)
{

	mutex_var.Lock();

	pVueceGlobalSetting->bIsDowloadCompleted = b;

	if(pVueceGlobalSetting->bIsDowloadCompleted)
	{
		VueceLogger::Info("VueceGlobalContext::SetDownloadCompleted - True");
	}
	else
	{
		VueceLogger::Info("VueceGlobalContext::SetDownloadCompleted - False");
	}

	mutex_var.Unlock();

}

bool VueceGlobalContext::IsDownloadCompleted()
{
	bool ret = false;

	mutex_var.Lock();
	ret = pVueceGlobalSetting->bIsDowloadCompleted;
	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetAudioFrameCounterInCurrentChunk(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->iAudioFrameCounterInCurrentChunk = i;

	VueceLogger::Info("VueceGlobalContext::SetAudioFrameCounterInCurrentChunk - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetAudioFrameCounterInCurrentChunk()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->iAudioFrameCounterInCurrentChunk;

	mutex_var.Unlock();

	return ret;
}

void VueceGlobalContext::SetTotalAudioFrameCounter(int i)
{
	mutex_var.Lock();

	pVueceGlobalSetting->iTotalAudioFrameCounter = i;

	VueceLogger::Info("VueceGlobalContext::SetTotalAudioFrameCounter - %d", i);

	mutex_var.Unlock();
}


int VueceGlobalContext::GetTotalAudioFrameCounter()
{
	int ret = VUECE_VALUE_NOT_SET;

	mutex_var.Lock();

	ret = pVueceGlobalSetting->iTotalAudioFrameCounter;

	mutex_var.Unlock();

	return ret;
}





