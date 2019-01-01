/*
 * VueceThreadUtil.h
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#ifndef VUECETHREADUTIL_H_
#define VUECETHREADUTIL_H_

#include "jthread.h"

#ifndef int64_t
typedef long long int		int64_t;
#endif

#ifndef uint64_t
typedef unsigned long long int	uint64_t;
#endif


typedef struct VueceTimeSpec{
	int64_t tv_sec;
	int64_t tv_nsec;
}VueceTimeSpec;

class VueceThreadUtil
{
public:
	static void InitMutex(JMutex* m);
	static void DestroyMutex(JMutex* m);
	static void MutexLock(JMutex* m);
	static void MutexUnlock(JMutex* m);
	static void GetCurTime(VueceTimeSpec *ret);
	static uint64_t GetCurTimeMs();
	static void SleepMs(int ms);
	static void SleepSec(int seconds);
	static void ThreadExit(void* ref_val);

#ifdef ANDROID
	static void CondWait(pthread_cond_t* cond, JMutex* m);
	static void CondSignal(pthread_cond_t* cond);
	static int CreateThread(pthread_t *thread, pthread_attr_t *attr, void * (*routine)(void*), void *arg);
	static int ThreadJoin(pthread_t thread);
#endif
};


#endif /* VUECETHREADUTIL_H_ */
