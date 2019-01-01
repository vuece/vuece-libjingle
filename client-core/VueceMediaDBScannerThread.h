/*
 * VeueceMediaDBScannerThread.h
 *
 *  Created on: May 11, 2013
 *      Author: Jingjing Sun
 */

#ifndef VEUECEMEDIADBSCANNERTHREAD_H_
#define VEUECEMEDIADBSCANNERTHREAD_H_

extern "C"
{
	#include "stdint.h"
}

#include "jthread.h"
#include "talk/base/thread.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "VueceProgressUI.h"
#include "vuecemediaitem.h"
#include "sqlite3.h"

class VueceMediaDBScannerThread: public JThread,  public sigslot::has_slots<>
{
public:
	VueceMediaDBScannerThread();
	virtual ~VueceMediaDBScannerThread();

	void* Thread();

private:
//	  void ScanAndBuildMediaItemList(const std::string &absRootFolderPathUtf8);
	  void UpdateMediaDB(VueceMediaItemList* itemList);
	  void Debug_ListScannedItems();
	  bool CollectResourceInfoInFolderDB(const std::string &itemID);
private:
	VueceMediaItemList* iMediaItemList;
	int iNumScannedSongs;
public:
	gcroot<VueceProgressUI^> ui;

 };


#endif /* VEUECEMEDIADBSCANNERTHREAD_H_ */
