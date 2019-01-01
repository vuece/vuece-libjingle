/*
 * VueceMediaDBManager.h
 *
 *  Created on: May 13, 2013
 *      Author: Jingjing Sun
 */

#ifndef VUECEMEDIADBMANAGER_H_
#define VUECEMEDIADBMANAGER_H_

#include "vuecemediaitem.h"
#include "sqlite3.h"

#define VUECE_DB_NAME "VueceMedia.db"
#define DB_TABLE_NAME_MEDIA_ITEMS "VueceMediaItems"
#define DB_CMD_FIND_TABLE_MEDIA_ITEMS "SELECT * FROM sqlite_master WHERE name ='VueceMediaItems' and type='table'"
//Old table without nchannels
//#define DB_CMD_CREATE_TABLE_MEDIA_ITEMS "CREATE TABLE VueceMediaItems (id INTEGER PRIMARY KEY, uri STRING, parent_uri STRING, name STRING, path STRING, type STRING, num_dirs INTEGER, num_songs INTEGER, bitrate INTEGER, samplerate INTEGER, artist STRING, album STRING, title STRING, length INTEGER, size INTEGER)"
//#define DB_CMD_INSERT_MEDIA_ITEM "INSERT INTO VueceMediaItems VALUES(NULL,'%s','%s','%s','%s','%s',%d,%d,%d,%d,'%s','%s','%s',%d,%d)"

//New table with nchannels
#define DB_CMD_CREATE_TABLE_MEDIA_ITEMS "CREATE TABLE VueceMediaItems (id INTEGER PRIMARY KEY, uri STRING, parent_uri STRING, name STRING, path STRING, type STRING, num_dirs INTEGER, num_songs INTEGER, bitrate INTEGER, samplerate INTEGER, nchannels INTEGER, artist STRING, album STRING, title STRING, length INTEGER, size INTEGER)"
#define DB_CMD_INSERT_MEDIA_ITEM "INSERT INTO VueceMediaItems VALUES(NULL,'%s','%s','%s','%s','%s',%d,%d,%d,%d,%d,'%s','%s','%s',%d,%d)"

#define DB_CMD_DROP_TABLE "DROP TABLE VueceMediaItems"
#define DB_CMD_QUERY_ALL_ITEMS_WITH_PARENT_URI "SELECT * FROM VueceMediaItems WHERE parent_uri='%s'"
#define DB_CMD_QUERY_ITEM_WITH_URI "SELECT * FROM VueceMediaItems WHERE uri='%s'"
#define DB_CMD_CREATE_INDEX_ON_URI "CREATE INDEX Uri_Idx ON VueceMediaItems (uri)"

#define DB_COL_ID_ID 0
#define DB_COL_ID_URI 1
#define DB_COL_ID_PARENT_URI 2
#define DB_COL_ID_NAME 3
#define DB_COL_ID_PATH 4
#define DB_COL_ID_TYPE 5
#define DB_COL_ID_NUM_DIRS 6
#define DB_COL_ID_NUM_SONGS 7
#define DB_COL_ID_BITRATE 8
#define DB_COL_ID_SAMPLERATE 9

//Old column index
//#define DB_COL_ID_ARTIST 10
//#define DB_COL_ID_ALBUM 11
//#define DB_COL_ID_TITLE 12
//#define DB_COL_ID_LENGTH 13
//#define DB_COL_ID_SIZE 14

//New column index
#define DB_COL_ID_NCHANNELS 10
#define DB_COL_ID_ARTIST 11
#define DB_COL_ID_ALBUM 12
#define DB_COL_ID_TITLE 13
#define DB_COL_ID_LENGTH 14
#define DB_COL_ID_SIZE 15


class VueceMediaDBManager {

public:
	VueceMediaDBManager();
	virtual ~VueceMediaDBManager();

	bool Open();
	bool SelfCheck();
	void CreateTables();
	void DropTables();
	void Close();

	VueceMediaItemList* BrowseMediaItem(const std::string &uri);
	VueceMediaItemList* QueryMediaItemWithUri(const std::string &uri);
	void UpdateMediaDB(VueceMediaItemList* itemList);

	//static methods

public:
	static std::string RetrieveDBFileChecksum(bool forced);
	static std::string GetFullDBFilePath();

private:

	void InitMediaDB();

	VueceMediaItemList* QueryMediaItemsWithSqlCmd(const char* sqlCmd);

	sqlite3 *mediaDB;

	static std::string currentDBFileChecksum;
	static bool modifiedSinceLastBuild;

private:

};

#endif /* VUECEMEDIADBMANAGER_H_ */
