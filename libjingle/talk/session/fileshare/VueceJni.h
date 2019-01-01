/*
 * VueceJni.h
 *
 *  Created on: Nov 8, 2014
 *      Author: jingjing
 */

#ifndef VUECEJNI_H_
#define VUECEJNI_H_

#include <jni.h>

#ifdef __cplusplus
extern "C"{
#endif

class VueceJni
{
public:
	 static void AndroidKeyCleanup(void *data);

	 static void SetJvmForCurrentThread(JavaVM *vm, unsigned int* thread_key, const char* thread_name);

	 static JNIEnv* AttachCurrentThreadToJniEnv(const char* src_name);

	 static JavaVM *GetJvm(void);

	 static JNIEnv *GetJniEnv(const char* src_name);

	 static void ThreadExit(void* ref_val, const char* src_name);

};


#ifdef __cplusplus
}
#endif


#endif /* VUECEJNI_H_ */
