/*
 * VueceJni.cc
 *
 *  Created on: Nov 8, 2014
 *      Author: jingjing
 *
 * Useful docs about JVM attach/detach
 *  https://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/invocation.html
 *
 *  Some notes about how attach/detach works
 *  1. At first, during start up, function VueceJni::SetJvmForCurrentThread() is called from vtalk.cc::JNI_OnLoad(), which runs in the 'main' thread, and the key value
 *  of the main thread is initialized and stored in static variable 'jnienv_key'.
 *
 *  2. When a new thread is created in our native library, before it starts running, attaching is done by calling VueceJni::AttachCurrentThreadToJniEnv(), this function checks
 *  if current newly created  thread has already been registered with jnienv_key - obviously the answer is no in this case, so AttachCurrentThread() is called to obtain a JNI
 *  interface pointer 'env', and 'env' is registered with key 'jnienv_key' in CURRENT thread environment
 */

#include "VueceLogger.h"
#include "VueceJni.h"

static JavaVM *vuece_vm=0;

#ifndef WIN32
#include <pthread.h>

static pthread_key_t jnienv_key;

void VueceJni::AndroidKeyCleanup(void *data){
	VueceLogger::Debug("VueceJni::AndroidKeyCleanup[TROUBLESHOOTING]  - jnienv_key = %d", (unsigned int)jnienv_key);
	JNIEnv* env=(JNIEnv*)pthread_getspecific(jnienv_key);

	if (env != NULL) {

		VueceLogger::Debug("VueceJni::AndroidKeyCleanup [TROUBLESHOOTING] - Thread end, detaching jvm from current thread");
		vuece_vm->DetachCurrentThread();//DetachCurrentThread(vuece_vm);
		pthread_setspecific(jnienv_key,NULL);
	}
	else
	{
		VueceLogger::Debug("VueceJni::AndroidKeyCleanup [TROUBLESHOOTING] - env is null, do nothing");
	}

}
#endif

/**
 * DESCRIPTION

	The pthread_key_create() function shall create a thread-specific data key visible to all threads in the process. Key values provided by pthread_key_create() are opaque objects used to locate thread-specific data. Although the same key value may be used by different threads, the values bound to the key by pthread_setspecific() are maintained on a per-thread basis and persist for the life of the calling thread.

	Upon key creation, the value NULL shall be associated with the new key in all active threads. Upon thread creation, the value NULL shall be associated with all defined keys in the new thread.

	An optional destructor function may be associated with each key value. At thread exit, if a key value has a non-NULL destructor pointer, and the thread has a non-NULL value associated with that key, the value of the key is set to NULL, and then the function pointed to is called with the previously associated value as its sole argument. The order of destructor calls is unspecified if more than one destructor exists for a thread when it exits.

	If, after all the destructors have been called for all non-NULL values with associated destructors, there are still some non-NULL values with associated destructors, then the process is repeated. If, after at least {PTHREAD_DESTRUCTOR_ITERATIONS} iterations of destructor calls for outstanding non-NULL values, there are still some non-NULL values with associated destructors, implementations may stop calling destructors, or they may continue calling destructors until no non-NULL values with associated destructors exist, even though this might result in an infinite loop.

	RETURN VALUE

	If successful, the pthread_key_create() function shall store the newly created key value at *key and shall return zero. Otherwise, an error number shall be returned to indicate the error.

	ERRORS

	The pthread_key_create() function shall fail if:

	[EAGAIN]
	The system lacked the necessary resources to create another thread-specific data key, or the system-imposed limit on the total number of keys per process {PTHREAD_KEYS_MAX} has been exceeded.
	[ENOMEM]
	Insufficient memory exists to create the key.
	The pthread_key_create() function shall not return an error code of [EINTR].
 */
void VueceJni::SetJvmForCurrentThread(JavaVM *vm, unsigned int* thread_key, const char* src_thread_name)
{
	int ret = 0;

	vuece_vm=vm;
#ifndef WIN32
	VueceLogger::Debug("VueceJni::SetJvmForCurrentThread[TROUBLESHOOTING] 1 - Registering a JNI env for thread[%s]", src_thread_name);
	ret = pthread_key_create(&jnienv_key, AndroidKeyCleanup);
	if(ret == 0)
	{
		VueceLogger::Debug("VueceJni::SetJvmForCurrentThread[TROUBLESHOOTING] 2 - Thread key for [%s] =  %d", src_thread_name, *thread_key);

		*thread_key = (unsigned int)jnienv_key;
	}
	else
	{
		VueceLogger::Debug("VueceJni::SetJvmForCurrentThread[TROUBLESHOOTING] 3 - pthread_key_create returned error for [%s] =  %d", src_thread_name, ret);
	}

#endif
}

JavaVM* VueceJni::GetJvm(void){
	return vuece_vm;
}


JNIEnv * VueceJni::AttachCurrentThreadToJniEnv(const char* src_name)
{
	JNIEnv *env=NULL;
	if (vuece_vm==NULL)
	{
		VueceLogger::Fatal("VueceJni::AttachCurrentThreadToJniEnv[%s] - Jni Env is not null, must be set at first, abort.", src_name);
		return NULL;
	}
	else
	{
#ifndef WIN32
		VueceLogger::Debug("VueceJni::AttachCurrentThreadToJniEnv[%s] - jnienv_key = %d", src_name, (unsigned int)jnienv_key);

		env=(JNIEnv*)pthread_getspecific(jnienv_key);

		if (env==NULL)
		{
			VueceLogger::Info("VueceJni::AttachCurrentThreadToJniEnv[%s] - pthread_getspecific returned null value, calling vuece_vm->AttachCurrentThread", src_name);

			if (vuece_vm->AttachCurrentThread(&env,NULL)!=0)
			{
				VueceLogger::Fatal("VueceJni::AttachCurrentThreadToJniEnv [%s]- AttachCurrentThread() failed !", src_name);
				return NULL;
			}

			/**
			 * Important note - Some doc about this function:
			 *
			 * The pthread_setspecific() function shall associate a thread-specific value with a key obtained via a previous call to pthread_key_create().
			 * Different threads may bind different values to the same key. These values are typically pointers to blocks of dynamically allocated memory
			 * that have been reserved for use by the calling thread.
			 */
			pthread_setspecific(jnienv_key,env);
		}

#else
		VueceLogger::Fatal("VueceJni::AttachCurrentThreadToJniEnv - not implemented on windows.");
#endif

	}
	return env;
}

void VueceJni::ThreadExit(void* ref_val, const char* thread_name)
{
#ifdef ANDROID
	VueceLogger::Debug("VueceJni::ThreadExit - From thread[%s]", thread_name);
	// due to a bug in old Bionic version
	// cleanup of jni manually
	// works directly with Android 2.2
	VueceJni::AndroidKeyCleanup(NULL);
#endif
	pthread_exit(ref_val);
}



JNIEnv * VueceJni::GetJniEnv(const char* src_name){
	JNIEnv *env=NULL;
	if (vuece_vm==NULL){
		VueceLogger::Fatal("VueceJni::GetJniEnv[%s] - Calling GetJniEnv() while no jvm has been set using SetJvm().", src_name);
	}else{
#ifndef WIN32
		VueceLogger::Debug("VueceJni::GetJniEnv[%s] - jnienv_key = %d", src_name, (unsigned int)jnienv_key);

		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL){
			VueceLogger::Info("VueceJni::GetJniEnv[%s] - pthread_getspecific returned null value, calling vuece_vm->AttachCurrentThread", src_name);
//			if ((*vuece_vm)->AttachCurrentThread(vuece_vm,&env,NULL)!=0){
			if (vuece_vm->AttachCurrentThread(&env,NULL)!=0){
				VueceLogger::Fatal("VueceJni::GetJniEnv [%s]- AttachCurrentThread() failed !", src_name);
				return NULL;
			}

			pthread_setspecific(jnienv_key,env);
		}

#else
		VueceLogger::Fatal("VueceJni::GetJniEnv - not implemented on windows.");
#endif
	}
	return env;
}
