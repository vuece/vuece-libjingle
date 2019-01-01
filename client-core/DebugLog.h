/*
 * DebugLog.h
 *
 *  Created on: 2014-11-22
 *      Author: Jingjing Sun
 */

#ifndef DEBUGLOG_H_
#define DEBUGLOG_H_

#include "talk/base/sigslot.h"

class DebugLog: public sigslot::has_slots<> {
public:
	DebugLog();
	void Input(const char * data, int len) ;
	 void Output(const char * data, int len) ;
	  static bool IsAuthTag(const char * str, size_t len) ;
	  void DebugPrint(char * buf, int * plen, bool output);

public:
	  char * debug_input_buf_;
	  int debug_input_len_;
	  int debug_input_alloc_;
	  char * debug_output_buf_;
	  int debug_output_len_;
	  int debug_output_alloc_;
	  bool censor_password_;
};

#endif /* DEBUGLOG_H_ */
