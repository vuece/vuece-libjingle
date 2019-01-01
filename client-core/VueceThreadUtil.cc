/*
 * VueceThreadUtil.cc
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#include <errno.h>
#include "VueceThreadUtil.h"
#include "VueceLogger.h"


void VueceThreadUtil::InitMutex(JMutex* m)
{
//	VueceLogger::Debug( "VueceThreadUtil::InitMutex");

	if(m == NULL)
	{
		VueceLogger::Fatal("VueceThreadUtil::InitMutex - Input is not null!");
		return;
	}

	if(m->IsInitialized())
	{
		VueceLogger::Warn("VueceThreadUtil::InitMutex - Already initialized");
	}
	else
	{
		if( m->Init() != 0)
		{
			VueceLogger::Fatal("VueceThreadUtil::InitMutex - Mutext cannot be initialized.");
		}
	}

}



void VueceThreadUtil::DestroyMutex(JMutex* m)
{
	if(m == NULL || !m->IsInitialized())
	{
		VueceLogger::Fatal("VueceThreadUtil::DestroyMutex - Input is null or not initialized!");
		return;
	}

	m->Unlock();

	delete m;
}

void VueceThreadUtil::MutexLock(JMutex* m)
{
	m->Lock();
}

void VueceThreadUtil::MutexUnlock(JMutex* m)
{
	m->Unlock();
}

#ifdef ANDROID
void VueceThreadUtil::CondWait(pthread_cond_t* cond, JMutex* m)
{
	pthread_cond_wait(cond, (pthread_mutex_t*)m->Handle());
}

void VueceThreadUtil::CondSignal(pthread_cond_t* cond)
{
	pthread_cond_signal(cond);
}

int VueceThreadUtil::CreateThread(pthread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg)
{
	pthread_attr_t my_attr;
	pthread_attr_init(&my_attr);
	if (attr)
		my_attr = *attr;

	return pthread_create(thread, &my_attr, routine, arg);
}

int VueceThreadUtil::ThreadJoin(pthread_t thread)
{
	int err=pthread_join(thread,0);
	if (err!=0) {

		VueceLogger::Debug( "VueceThreadUtil::ThreadJoin - Error: %d", err);

	}
	return err;
}

#endif

void VueceThreadUtil::GetCurTime(VueceTimeSpec *ret)
{
#if defined(_WIN32_WCE) || defined(WIN32)
	DWORD timemillis;
#	if defined(_WIN32_WCE)
	timemillis=GetTickCount();
#	else
	timemillis=timeGetTime();
#	endif
	ret->tv_sec=timemillis/1000;
	ret->tv_nsec=(timemillis%1000)*1000000LL;
#elif defined(__MACH__) && defined(__GNUC__) && (__GNUC__ >= 3)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ret->tv_sec=tv.tv_sec;
	ret->tv_nsec=tv.tv_usec*1000LL;
#elif defined(__MACH__)
	struct timeb time_val;

	ftime (&time_val);
	ret->tv_sec = time_val.time;
	ret->tv_nsec = time_val.millitm * 1000000LL;
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC,&ts)<0){
		VueceLogger::Fatal("GetCurTime does not work");
	}
	ret->tv_sec=ts.tv_sec;
	ret->tv_nsec=ts.tv_nsec;
#endif
}

uint64_t VueceThreadUtil::GetCurTimeMs()
{
	VueceTimeSpec ts;
	GetCurTime(&ts);
	return (ts.tv_sec*1000LL) + ((ts.tv_nsec+500000LL)/1000000LL);
}

void VueceThreadUtil::SleepMs(int ms)
{
#ifdef WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec=0;
	ts.tv_nsec=ms*1000000LL;
	nanosleep(&ts,NULL);
#endif
}

void VueceThreadUtil::SleepSec(int seconds){
#ifdef WIN32
	Sleep(seconds*1000);
#else
	struct timespec ts,rem;
	int err;
	ts.tv_sec=seconds;
	ts.tv_nsec=0;
	do {
		err=nanosleep(&ts,&rem);
		ts=rem;
	}while(err==-1 && errno==EINTR);
#endif
}


