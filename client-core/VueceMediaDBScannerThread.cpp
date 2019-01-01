/*
 * VueceMediaDBScannerThread.c
 *
 *  Created on: May 11, 2013
 *      Author: Jingjing Sun
 */

#include "talk/base/logging.h"
#include "talk/base/stringdigest.h"
#include "VueceConstants.h"
#include "vuecemediaitem.h"
#include "VueceWinUtilities.h"
#include "VueceGlobalSetting.h"
#include "VueceMediaDBScannerThread.h"
#include "VueceMediaDBManager.h"

using namespace vuece;

VueceMediaDBScannerThread::VueceMediaDBScannerThread()
{
	LOG(INFO) << "VueceMediaDBScannerThread constructor called.";
	iNumScannedSongs = -1;
}


VueceMediaDBScannerThread::~VueceMediaDBScannerThread()
{
	LOG(INFO) << "VueceMediaDBScannerThread:De-constructor called.";

	delete iMediaItemList;
}


void* VueceMediaDBScannerThread::Thread()
{

	int numSongs = 0;
	int numDirs = 0;

	LOG(INFO) << "VueceMediaDBScannerThread - Call ThreadStarted()";

	ThreadStarted();

	std::string test_dir ("");
	std::string test_uri = talk_base::MD5(test_dir);

	VueceMediaDBManager* dbMgr = new VueceMediaDBManager();

	VueceGlobalSetting* iVueceGlobalSetting = VueceGlobalContext::Instance();

	dbMgr->Open();

	dbMgr->DropTables();

	dbMgr->CreateTables();

	iMediaItemList = new VueceMediaItemList();

	VueceWinUtilities::ScanAndBuildMediaItemList("", iMediaItemList, ui, iVueceGlobalSetting, &numSongs, &numDirs);

	LOG(LS_VERBOSE) << "VueceMediaDBScannerThread - Scan completed, number of items scanned: " << iMediaItemList->size()
			<<", number of songs: " << numSongs << ", nubmer of folders: " << numDirs;

	ui->OnMessage(VueceProgressUI::MSG_HINT_SCAN_DONE, "");

	dbMgr->UpdateMediaDB(iMediaItemList);

	//TEST CODE, REMOVE LATER
//	VueceMediaItemList* list = dbMgr->BrowseMediaItem(test_uri);
//
//	if(list != NULL)
//	{
//		std::ostringstream os;
//		VueceWinUtilities::GenerateJsonMsg(list, os);
//		LOG(INFO) << "VueceMediaDBScannerThread - Final json message: " << os.str();
//
//		delete list;
//	}

	dbMgr->Close();

	delete dbMgr;

	iVueceGlobalSetting->iRescanNeeded = 0;

	ui->OnFinish(iNumScannedSongs);

	return NULL;

}

void VueceMediaDBScannerThread::Debug_ListScannedItems()
{
		VueceMediaItemList* itemList = iMediaItemList;

		int counter = 0;

		LOG(LS_VERBOSE) << "=============== LIST SCANNED ITEMS START =====================";

		LOG(LS_VERBOSE) << "Found " << itemList->size() << " items that can be shared.";

		std::list<vuece::VueceMediaItem*>::iterator iter = itemList->begin();

		while (iter != itemList->end()) {

			VueceMediaItem* v = *iter;
			counter++;

			if(!v->IsFolder())
			{
				LOG(LS_VERBOSE) << "Item[" << counter << "] - "
				<< " name: " << v->Name()
				<< ", bitrate: " << v->BitRate() << ", samplerate: " << v->SampleRate()
				<< ", artist: " << VueceWinUtilities::tojsonstr(v->Artist())
				<< ", album:" << VueceWinUtilities::tojsonstr(v->Album())
				<< ", title:" << VueceWinUtilities::tojsonstr(v->Title())
				<< ", length:" << v->Duration()
				<< ", size:" << v->Size()
				<< ", uri:" << v->Uri()
				<< ", parent_uri:" << v->ParentUri();
			}
			else
			{
				LOG(LS_VERBOSE) << "Item[" << counter << "] - "
				<< " name: " << v->Name()
				<< ", type : dir , num-dirs: " << v->NumDirs()
				<< ", num-songs:" << v->NumSongs()
				<< ", uri:" << v->Uri()
				<< ", parent_uri:" << v->ParentUri();
			}

			LOG(INFO) << "items processed so far: " << counter;

			iter++;

		}

		LOG(LS_VERBOSE) << "=============== LIST SCANNED ITEMS END =====================";
}

