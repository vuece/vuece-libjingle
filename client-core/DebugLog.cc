/*
 * DebugLog.cc
 *
 *  Created on: 2014-11-22
 *      Author: Jingjing Sun
 */

#include <cstdio>
#include <iostream>
#include <time.h>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <vector>

#include <stdio.h>
#include <stdlib.h>

#include "talk/base/logging.h"
#include "DebugLog.h"

DebugLog::DebugLog() :
		debug_input_buf_(NULL), debug_input_len_(0), debug_input_alloc_(0), debug_output_buf_(
				NULL), debug_output_len_(0), debug_output_alloc_(0), censor_password_(
				false)
{
}

void DebugLog::Input(const char * data, int len)
{
	if (debug_input_len_ + len > debug_input_alloc_)
	{
		char * old_buf = debug_input_buf_;
		debug_input_alloc_ = 4096;
		while (debug_input_alloc_ < debug_input_len_ + len)
		{
			debug_input_alloc_ *= 2;
		}
		debug_input_buf_ = new char[debug_input_alloc_];
		memcpy(debug_input_buf_, old_buf, debug_input_len_);
		delete[] old_buf;
	}
	memcpy(debug_input_buf_ + debug_input_len_, data, len);
	debug_input_len_ += len;
	DebugPrint(debug_input_buf_, &debug_input_len_, false);
}

void DebugLog::Output(const char * data, int len)
{
	if (debug_output_len_ + len > debug_output_alloc_)
	{
		char * old_buf = debug_output_buf_;
		debug_output_alloc_ = 4096;
		while (debug_output_alloc_ < debug_output_len_ + len)
		{
			debug_output_alloc_ *= 2;
		}
		debug_output_buf_ = new char[debug_output_alloc_];
		memcpy(debug_output_buf_, old_buf, debug_output_len_);
		delete[] old_buf;
	}
	memcpy(debug_output_buf_ + debug_output_len_, data, len);
	debug_output_len_ += len;
	DebugPrint(debug_output_buf_, &debug_output_len_, true);
}

bool DebugLog::IsAuthTag(const char * str, size_t len)
{
	LOG(WARNING) << "IsAuthTag";
	if (str[0] == '<' && str[1] == 'a' && str[2] == 'u' && str[3] == 't'
			&& str[4] == 'h' && str[5] <= ' ')
	{
		std::string tag(str, len);

		if (tag.find("mechanism") != std::string::npos)
			return true;
	}
	return false;
}

void DebugLog::DebugPrint(char * buf, int * plen, bool output)
{
	int len = *plen;
	if (len > 0)
	{
		time_t tim = time(NULL);
		struct tm * now = localtime(&tim);
		char *time_string = asctime(now);
		if (time_string)
		{
			size_t time_len = strlen(time_string);
			if (time_len > 0)
			{
				time_string[time_len - 1] = 0; // trim off terminating \n
			}
		}

		if (output)
		{
			LOG(WARNING) << "SEND >>>>>>>>>>>>>>>>" << " : " << time_string;
		}
		else
		{
			LOG(WARNING) << "RECV <<<<<<<<<<<<<<<<" << " : " << time_string;
		}

		bool indent;
		int start = 0, nest = 3;
		for (int i = 0; i < len; i += 1)
		{
			if (buf[i] == '>')
			{
				if ((i > 0) && (buf[i - 1] == '/'))
				{
					indent = false;
				}
				else if ((start + 1 < len) && (buf[start + 1] == '/'))
				{
					indent = false;
					nest -= 2;
				}
				else
				{
					indent = true;
				}

				// Output a tag
				LOG(WARNING) << std::setw(nest) << " "
						<< std::string(buf + start, i + 1 - start);

				if (indent)
					nest += 2;

				// Note if it's a PLAIN auth tag
				//					if (IsAuthTag(buf + start, i + 1 - start)) {
				//						censor_password_ = true;
				//					}

				// incr
				start = i + 1;
			}

			if (buf[i] == '<' && start < i)
			{
				if (censor_password_)
				{
					LOG(WARNING) << std::setw(nest) << " "
							<< "## TEXT REMOVED ##";
					censor_password_ = false;
				}
				else
				{
					LOG(WARNING) << std::setw(nest) << " "
							<< std::string(buf + start, i - start);
				}
				start = i;
			}
		}
		len = len - start;
		memcpy(buf, buf + start, len);
		*plen = len;
	}
}
