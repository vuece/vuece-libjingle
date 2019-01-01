/*
 * VueceWinUtilities.h
 *
 *  Created on: Mar 9, 2013
 *      Author: Jingjing Sun
 */

#ifndef VUECEWINUTILITIES_H_
#define VUECEWINUTILITIES_H_

#ifdef WIN32
#include <string.h>
#include "talk/base/win32.h"
#include "Objbase.h"
#include "Rpc.h"

#else
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/base/base64.h"

#include "vuecemediaitem.h"
#include "VueceProgressUI.h"

#include <gcroot.h>

using namespace vuece;

#define MAX_USERDATA_LEN 50*1024
#define VUECE_MAX_FILE_PATH 256

typedef enum _VueceUserDataLoadResult{
	VueceUserDataLoadResult_NA = 0,
	VueceUserDataLoadResult_OK,
	VueceUserDataLoadResult_Corrupted_Registry,
	VueceUserDataLoadResult_UserData_Missing,
	VueceUserDataLoadResult_Mem_Insufficient,
	VueceUserDataLoadResult_Decrypt_Err,


	VueceUserDataLoadResult_General_Err = 99,
} VueceUserDataLoadResult;

class VueceWinUtilities {
public:

	static void MarshalString ( System::String ^ s, std::string& os );
	static bool GenUUID(char* result );
	static std::wstring utf8_decode(const std::string &str);
	static std::string ws2s(const std::wstring& s);
	static std::string utf8_encode(const std::wstring &wstr);
	static std::wstring s2ws(const std::string& s);
	static std::string wstrtostr(const std::wstring &wstr);
	static void ConvertCharArrayToLPCWSTR(const char* charArray, wchar_t* result, int result_size);
	static bool replace(std::string& str, const std::string& from, const std::string& to) ;
	static std::string tojsonstr(const TagLib::String s);

	static void CountSongsInFolder(const talk_base::Pathname&  p, int *dircount, int *filecount);
	static void GenerateJsonMsg(VueceMediaItemList* itemList, std::string targetUri, std::ostringstream& os);
	static bool IsFileHidden(const talk_base::Pathname &path);
	static bool ExtractAlbumArtFromFile(const std::string& src_path, std::ostringstream& result_preview_path);
	static 	bool ResizeImage(const char* filePath, std::string& destFilePath, int destW, int destH);

	static bool RetrieveCriticalMediaInfo(
			vuece::VueceMediaItem* mediaItem,
			const char* filePath,
			struct VueceGlobalSetting *pVueceGlobalSetting);

	static void ScanAndBuildMediaItemList(
			const std::string &absRootFolderPathUtf8,
			VueceMediaItemList* iMediaItemList,
			gcroot<VueceProgressUI^> ui,
			struct VueceGlobalSetting *pVueceGlobalSetting,
			int* numSongs,
			int* numDirs
			);

	static vuece::VueceMediaItem* AnalyzeFile(const std::string &absFolderPathUtf8,
			const std::string& filename_non_utf8,
			const std::string& filename_utf8,
			struct VueceGlobalSetting *pVueceGlobalSetting);

	static void FindTheFirstImageFileInFolder(const std::string &absolute_folder_path, talk_base::Pathname* resultPath);
	static VueceUserDataLoadResult LoadUserData2(char** result_buf, int* data_len);

	static void EnableAutoStart();
	static bool ParseUserData(const char* data, struct VueceGlobalSetting *pVueceGlobalSetting);
	static bool CompareVersion(const char* currentVerion, const char* latestVersion);
	static void SaveUserDataToRegistry(char* data, int data_len);
	/**
	 *
	 */
	static int GenerateUserSettingString2(char** result_buf_ptr, int* result_buf_size, struct VueceGlobalSetting *pVueceGlobalSetting);

	static bool QueryUpdate(char* latestVersion, char* updateUrl);
	/**
	 * Queries refresh token or access token
	 *
	 * Query refresh token if 'refresh' is true, otherwise query access token
	 */
	static bool QueryAccessToken (bool refresh, const char* code, char* result_access_tok, char* result_refresh_tok);
	static bool ExtracInfoFromOauth2PostResp (const char* response, char* access_tok, char* tok_type, char* exp, char* refresh_tok, char* err_msg);
	static bool QueryUserAccountInfo(const char* access_token, char* user_id, char* display_name, char* img_url);
	static bool RevokeAccountAccess( const char* access_token);

};

#endif /* VUECEWINUTILITIES_H_ */
