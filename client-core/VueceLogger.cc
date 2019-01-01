/*
 * VueceLogger.cc
 *
 *  Created on: Oct 31, 2014
 *      Author: jingjing
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "talk/base/logging.h"
#include "talk/base/common.h"
#include "jthread.h"
#include "VueceLogger.h"

#define MAX_LOG_BUF_LEN 1024*2

static JMutex mutex_state;

static char log_buffer[1024*2];
static bool initialized;

static void InitIfNeeded(void)
{
	if(!initialized)
	{
		initialized = true;

		if(mutex_state.IsInitialized())
		{
			LOG(LERROR) << "VueceLogger::InitIfNeeded - Mutex already initialized";
			abort();
		}
		else
		{
			if( mutex_state.Init() != 0)
			{
				LOG(LERROR) << "VueceThreadUtil::InitIfNeeded - Mutext cannot be initialized.";
				abort();
			}
		}

		LOG(LS_VERBOSE) << "InitIfNeeded - OK";
	}
}



void VueceLogger::Debug(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	InitIfNeeded();

	mutex_state.Lock();

	vsnprintf(log_buffer, MAX_LOG_BUF_LEN-1, format, args);

	va_end(args);

	LOG(LS_VERBOSE) << log_buffer;

	mutex_state.Unlock();
}

void VueceLogger::Info(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	InitIfNeeded();

	mutex_state.Lock();

	vsnprintf(log_buffer, MAX_LOG_BUF_LEN-1, format, args);

	va_end(args);

	LOG(INFO) << log_buffer;

	mutex_state.Unlock();
}


void VueceLogger::Warn(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	InitIfNeeded();

	mutex_state.Lock();


	vsnprintf(log_buffer, MAX_LOG_BUF_LEN-1, format, args);

	va_end(args);

	LOG(WARNING) << log_buffer;

	mutex_state.Unlock();
}


void VueceLogger::Error(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	InitIfNeeded();

	mutex_state.Lock();

	vsnprintf(log_buffer, MAX_LOG_BUF_LEN-1, format, args);

	va_end(args);

	LOG(LS_ERROR) << log_buffer;

	mutex_state.Unlock();
}


void VueceLogger::Fatal(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	InitIfNeeded();

	mutex_state.Lock();

	vsnprintf(log_buffer, MAX_LOG_BUF_LEN-1, format, args);

	va_end(args);

	LOG(LS_ERROR) << "FATAL - " << log_buffer;

	mutex_state.Unlock();

	abort();
}

