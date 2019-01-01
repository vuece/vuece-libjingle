/*
 * VueceMediaDBManager.cpp
 *
 *  Created on: May 13, 2013
 *      Author: Jingjing Sun
 */

#include "VueceWinUtilities.h"

#include "talk/base/logging.h"
#include "talk/base/stringdigest.h"
#include "VueceMediaDBManager.h"
#include "VueceConstants.h"
#include "VueceCommon.h"

#include "VueceGlobalSetting.h"

using namespace vuece;

std::string 	VueceMediaDBManager::currentDBFileChecksum = "";
bool 			VueceMediaDBManager::modifiedSinceLastBuild = false;

VueceMediaDBManager::VueceMediaDBManager() {

	LOG(INFO) << "VueceMediaDBManager()";

//	InitMediaDB();
}

VueceMediaDBManager::~VueceMediaDBManager() {
	LOG(INFO) << "VueceMediaDBManager - De-constructor called";
}

void VueceMediaDBManager::Close()
{
	LOG(LS_VERBOSE) << "VueceMediaDBManager - Closing db.";
	sqlite3_close(mediaDB);
}

bool  VueceMediaDBManager::Open() {
	int ret = -1;
	char cFullDbPath[256];
//	char *dbErrMsg;

	VueceGlobalSetting* iVueceGlobalSetting = VueceGlobalContext::Instance();

	LOG(LS_VERBOSE) << "VueceMediaDBManager::Open - opening database.";

	strcpy(cFullDbPath, iVueceGlobalSetting->appUserDataDir);
	strcat(cFullDbPath, "\\");
	strcat(cFullDbPath, VUECE_DB_NAME);

	LOG(LS_VERBOSE) << "VueceMediaDBManager::cFullDbPath: " << cFullDbPath;

	ret = sqlite3_open(cFullDbPath, &mediaDB);

	if (ret != SQLITE_OK) {
		LOG(LS_ERROR) << "VueceMediaDBManager::Open - database cannot be opened, error code = " << ret;
		sqlite3_close(mediaDB);
		return false;
	}

	LOG(LS_VERBOSE) << "VueceMediaDBManager::Open - database opened";
	return true;
}

bool  VueceMediaDBManager::SelfCheck() {

	int ret = -1;
	char *dbErrMsg;
	bool bTableFound = false;
	int rows = 0;
	int columns = 0;
	char **results = NULL;
	int numSongs = 0;
	int numDirs = 0;

	VueceMediaItemList* iMediaItemList = NULL;

	VueceGlobalSetting* iVueceGlobalSetting = VueceGlobalContext::Instance();

	LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - Locating table.";

	ret =  sqlite3_get_table(mediaDB, DB_CMD_FIND_TABLE_MEDIA_ITEMS, &results, &rows, &columns, &dbErrMsg);

	if (ret != SQLITE_OK) {
		LOG(LS_ERROR) << "VueceMediaDBManager:SelfCheck - error locating table = " << sqlite3_errmsg(mediaDB);
		sqlite3_free(dbErrMsg);
	} else {
		LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - Query returned OK";
		LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - rows = " << rows;

		if(rows > 0)
		{
			LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - Target table is located.";
			//If table has been found, then we dont need to do anything, otherwise we will rebuild the table
			return true;
		}
	}

	LOG(WARNING) << "VueceMediaDBManager:SelfCheck -Table not found, rebuild it now.";

	LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - Close current db";
	Close();

	//re-open
	LOG(LS_VERBOSE) << "VueceMediaDBManager:SelfCheck - Re-open db";
	Open();

	CreateTables();

	iMediaItemList = new VueceMediaItemList();

	VueceWinUtilities::ScanAndBuildMediaItemList("", iMediaItemList, NULL, iVueceGlobalSetting, &numSongs, &numDirs);

	LOG(LS_VERBOSE) << "VueceMediaDBManager - Scan completed, number of items scanned: " << iMediaItemList->size()
			<<", number of songs: " << numSongs << ", nubmer of folders: " << numDirs;

	UpdateMediaDB(iMediaItemList);

	return true;
}

void VueceMediaDBManager::CreateTables() {

	int ret = -1;
	char *dbErrMsg;

	LOG(LS_VERBOSE) << "VueceMediaDBManager:CreateTables";

	ret = sqlite3_exec(mediaDB, (const char*) DB_CMD_CREATE_TABLE_MEDIA_ITEMS, NULL, NULL, &dbErrMsg);

	if (ret != SQLITE_OK) {
		LOG(LS_ERROR) << "VueceMediaDBManager - error create table = " << sqlite3_errmsg(mediaDB);
		sqlite3_free(dbErrMsg);
	} else {
		LOG(LS_VERBOSE) << "VueceMediaDBManager - Table created.";
	}

	LOG(LS_VERBOSE) << "VueceMediaDBManager:CreateTables - creating index on column uri";

	ret = sqlite3_exec(mediaDB, (const char*) DB_CMD_CREATE_INDEX_ON_URI, NULL, NULL, &dbErrMsg);

	if (ret != SQLITE_OK) {
		LOG(LS_ERROR) << "VueceMediaDBManager - error create index = " << sqlite3_errmsg(mediaDB);
		sqlite3_free(dbErrMsg);
	} else {
		LOG(LS_VERBOSE) << "VueceMediaDBManager - Index created.";
	}

	LOG(LS_VERBOSE) << "VueceMediaDBManager:CreateTables - Done.";

}

void VueceMediaDBManager::DropTables()
{
	int ret = -1;
	char *dbErrMsg;

	LOG(LS_VERBOSE) << "VueceMediaDBManager - DropTables: run DB cmd: " << DB_CMD_DROP_TABLE;

	//Drop table at first
	ret = sqlite3_exec(mediaDB, (const char*) DB_CMD_DROP_TABLE, NULL, NULL, &dbErrMsg);

	if (ret != SQLITE_OK) {
		LOG(LS_ERROR) << "DropTables - error dropping table = " << sqlite3_errmsg(mediaDB);
		sqlite3_free(dbErrMsg);
	} else {
		LOG(LS_VERBOSE) << "DropTables - Table dropped.";
	}

}


VueceMediaItemList* VueceMediaDBManager::QueryMediaItemWithUri(const std::string &uri)
{
	char sqlCmd[256];

	LOG(LS_VERBOSE) << "VueceMediaDBManager::QueryMediaItemWithUri: " << uri;

	sprintf(sqlCmd, DB_CMD_QUERY_ITEM_WITH_URI, uri.c_str());

	VueceMediaItemList* list = QueryMediaItemsWithSqlCmd(sqlCmd);

	LOG(LS_VERBOSE) << "BrowseMediaItem - Done";

	return list;
}


VueceMediaItemList* VueceMediaDBManager::QueryMediaItemsWithSqlCmd(const char*  sqlCmd) {
	int ret = -1;
	char *dbErrMsg;
	char **results = NULL;
	int rows, columns;

	LOG(LS_VERBOSE) << "=============== QueryMediaItemsWithSqlCmd START =====================";

	LOG(INFO) << "VueceMediaDBManager::QueryMediaItemsWithSqlCmd - Cmd: " << sqlCmd;

	VueceMediaItemList* list = new VueceMediaItemList();

	int rc =  sqlite3_get_table(mediaDB, sqlCmd, &results, &rows, &columns, &dbErrMsg);
	if (rc)
	{
		LOG(LS_ERROR) << "Error executing SQLite3 query: " << sqlite3_errmsg(mediaDB);
		LOG(LS_ERROR) << "dbErrMsg: " << dbErrMsg;
		sqlite3_free(dbErrMsg);
	}
	else
	{
		/**
		 * Row[0]: id
			Row[0]: uri
			Row[0]: parent_uri
			Row[0]: name
			Row[0]: type
			Row[0]: num_dirs
			Row[0]: num_songs
			Row[0]: bitrate
			Row[0]: samplerate
			Row[0]: nchannels
			Row[0]: artist
			Row[0]: album
			Row[0]: title
			Row[0]: length
			Row[0]: size
		 */
		LOG(LS_VERBOSE) << "Found " << rows << " results, columns: " << columns;

		//Skip header
		for (int rowCtr = 1; rowCtr <= rows; ++rowCtr)
		{
			VueceMediaItem* mediaItem = new VueceMediaItem();

			 for (int colCtr = 0; colCtr < columns; ++colCtr)
			 {
				// Determine Cell Position
				int cellPosition = (rowCtr * columns) + colCtr;

				char* cellVar = results[cellPosition];
				std::string valueString(cellVar);

				// Display Cell Value
//				LOG(LS_VERBOSE) << "Row[" << rowCtr << "]: " << "[" << cellVar << "]";

				switch(colCtr)
				{
				case DB_COL_ID_ID:
					break;
				case DB_COL_ID_URI:
					mediaItem->SetUriUtf8(valueString);
					break;
				case DB_COL_ID_PARENT_URI:
					mediaItem->SetParentUriUtf8(valueString);
					break;
				case DB_COL_ID_NAME:
					mediaItem->SetName(valueString);
					break;
				case DB_COL_ID_PATH:
					mediaItem->SetPath(valueString);
					break;
				case DB_COL_ID_TYPE:
					if(valueString.compare("dir") == 0)
					{
						mediaItem->SetFolder(true);
						LOG(LS_VERBOSE) << "This item represents a folder";
					}
					else
					{
						mediaItem->SetFolder(false);
						LOG(LS_VERBOSE) << "This item represents a file";
					}
					break;
				case DB_COL_ID_NUM_DIRS:
					mediaItem->SetNumDirs(atoi(cellVar));
					break;
				case DB_COL_ID_NUM_SONGS:
					mediaItem->SetNumSongs(atoi(cellVar));
					break;
				case DB_COL_ID_BITRATE:
					mediaItem->SetBitRate(atoi(cellVar));
					break;
				case DB_COL_ID_SAMPLERATE:
					mediaItem->SetSampleRate(atoi(cellVar));
					break;
				case DB_COL_ID_NCHANNELS:
					mediaItem->SetNChannels(atoi(cellVar));
					break;
				case DB_COL_ID_ARTIST:
					mediaItem->SetArtistB64(valueString);
					break;
				case DB_COL_ID_ALBUM:
					mediaItem->SetAlbumB64(valueString);
					break;
				case DB_COL_ID_TITLE:
					mediaItem->SetTitleB64(valueString);
					break;
				case DB_COL_ID_LENGTH:
					mediaItem->SetDuration(atoi(cellVar));
					break;
				case DB_COL_ID_SIZE:
					mediaItem->SetSize(atoi(cellVar));
					break;
				}
			 }

			 list->push_back(mediaItem);

			 LOG(LS_VERBOSE) << "Media item added to list, current size = " << list->size();

		}
	}

	sqlite3_free_table(results);

   LOG(LS_VERBOSE) << "QueryMediaItemsWithSqlCmd - Done";

	return list;
}


VueceMediaItemList* VueceMediaDBManager::BrowseMediaItem(const std::string &itemID) {
	char sqlCmd[256];

	LOG(LS_VERBOSE) << "=============== BrowseMediaItem START =====================";

	LOG(INFO) << "VueceMediaDBManager::BrowseMediaItem - itemID: " << itemID;

	sprintf(sqlCmd, DB_CMD_QUERY_ALL_ITEMS_WITH_PARENT_URI, itemID.c_str());

	VueceMediaItemList* list = QueryMediaItemsWithSqlCmd(sqlCmd);

   LOG(LS_VERBOSE) << "BrowseMediaItem - Done";

	return list;
}

void VueceMediaDBManager::UpdateMediaDB(VueceMediaItemList* itemList) {
	int rc = -1;
	char *error;
	char tmpBuf[2048];

	LOG(LS_VERBOSE) << "=============== UpdateMediaDB START =====================";

	modifiedSinceLastBuild = true;

	std::list<vuece::VueceMediaItem*>::iterator iter = itemList->begin();

	while (iter != itemList->end()) {

		VueceMediaItem* v = *iter;
		/**
		 * #define DB_CMD_CREATE_TABLE_MEDIA_ITEMS "CREATE TABLE VueceMediaItems (
		 * id INTEGER PRIMARY KEY,
		 * uri STRING,
		 * parent_uri STRING,
		 * name STRING,
		 * path STRING,
		 * type STRING,
		 * num-dirs INTEGER,
		 * num-songs INTEGER,
		 * bitrate INTEGER,
		 * samplerate INTEGER,
		 * nchannels INTEGER,
		 * artist STRING,
		 * album STRING
		 * title STRING,
		 * length INTEGER,
		 * size INTEGER)"
		 *
		 */

		//Note - since we base64 encode item name here, there is no need to encode again when we
		//convert the item info to json message, see VueceWinUtilities::GenerateJsonMsg()
		sprintf(tmpBuf, DB_CMD_INSERT_MEDIA_ITEM,
				v->Uri().c_str(),
				v->ParentUri().c_str(),
				talk_base::Base64::Encode(v->Name()).c_str(),
				v->Path().c_str(),
				v->Type().c_str(),
				v->NumDirs(),
				v->NumSongs(),
				v->BitRate(),
				v->SampleRate(),
				v->NChannels(),
				VueceWinUtilities::tojsonstr(v->Artist()).c_str(),
				VueceWinUtilities::tojsonstr(v->Album()).c_str(),
				VueceWinUtilities::tojsonstr(v->Title()).c_str(),
				v->Duration(), v->Size());

		LOG(LS_VERBOSE) << "Run DB command: " << tmpBuf;

		rc = sqlite3_exec(mediaDB, tmpBuf, NULL, NULL, &error);

		if (rc) {
			LOG(LS_ERROR) << "Error executing SQLite3 statement: " << sqlite3_errmsg(mediaDB);
			sqlite3_free(error);
			return;
		} else {
			LOG(LS_VERBOSE) << "Insert was successful";
		}

		iter++;
	}

	LOG(LS_VERBOSE) << "=============== UpdateMediaDB END =====================";
}

std::string VueceMediaDBManager::GetFullDBFilePath()
{
	char cFullDbPath[256];

	VueceGlobalSetting* iVueceGlobalSetting = VueceGlobalContext::Instance();

	LOG(LS_VERBOSE) << "VueceMediaDBManager:GetFullDBFilePath";

	strcpy(cFullDbPath, iVueceGlobalSetting->appUserDataDir);
	strcat(cFullDbPath, "\\");
	strcat(cFullDbPath, VUECE_DB_NAME);

	std::string result(cFullDbPath);

	return result;
}

std::string VueceMediaDBManager::RetrieveDBFileChecksum(bool forced)
{
	LOG(LS_VERBOSE) << "VueceMediaDBManager:RetrieveDBFileChecksum";

	if(forced)
	{
		LOG(LS_VERBOSE) << "VueceMediaDBManager:RetrieveDBFileChecksum - Force checksum calculation";
	}

	if(modifiedSinceLastBuild)
	{
		LOG(LS_VERBOSE) << "VueceMediaDBManager:RetrieveDBFileChecksum - Modified since last build";
	}

	if(forced || modifiedSinceLastBuild)
	{
		std::string db_path	 = GetFullDBFilePath();
		currentDBFileChecksum	 = VueceCommon::CalculateMD5FromFile(db_path);
		modifiedSinceLastBuild = false;

	}

	LOG(LS_VERBOSE) << "VueceMediaDBManager:RetrieveDBFileChecksum - Current checksum: " << currentDBFileChecksum;

	return currentDBFileChecksum;
}


