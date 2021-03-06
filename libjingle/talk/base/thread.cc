/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/thread.h"

#if defined(WIN32)
#include <comdef.h>
#elif defined(POSIX)
#include <time.h>
#endif

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/base/timeutils.h"

#ifdef OSX_USE_COCOA
#ifndef OSX
#error OSX_USE_COCOA is defined but not OSX
#endif
#include "talk/base/maccocoathreadhelper.h"
#include "talk/base/scoped_autorelease_pool.h"
#endif

static bool log_detail;

namespace talk_base {

ThreadManager g_thmgr;

#ifdef POSIX
pthread_key_t ThreadManager::key_;

ThreadManager::ThreadManager() {
  pthread_key_create(&key_, NULL);
  main_thread_ = WrapCurrentThread();
#if defined(OSX_USE_COCOA)
  InitCocoaMultiThreading();
#endif
}

ThreadManager::~ThreadManager() {
#ifdef OSX_USE_COCOA
  // This is called during exit, at which point apparently no NSAutoreleasePools
  // are available; but we might still need them to do cleanup (or we get the
  // "no autoreleasepool in place, just leaking" warning when exiting).
  ScopedAutoreleasePool pool;
#endif
  UnwrapCurrentThread();
  // Unwrap deletes main_thread_ automatically.
  pthread_key_delete(key_);
}

Thread *ThreadManager::CurrentThread() {
  return static_cast<Thread *>(pthread_getspecific(key_));
}

void ThreadManager::SetCurrent(Thread *thread) {
  pthread_setspecific(key_, thread);
}
#endif

#ifdef WIN32
DWORD ThreadManager::key_;

ThreadManager::ThreadManager() {
  key_ = TlsAlloc();
  main_thread_ = WrapCurrentThread();
}

ThreadManager::~ThreadManager() {
  UnwrapCurrentThread();
  TlsFree(key_);
}

Thread *ThreadManager::CurrentThread() {
  return static_cast<Thread *>(TlsGetValue(key_));
}

void ThreadManager::SetCurrent(Thread *thread) {
  TlsSetValue(key_, thread);
}
#endif

// static
Thread *ThreadManager::WrapCurrentThread() {
  Thread* result = CurrentThread();
  if (NULL == result) {
    result = new Thread();
#if defined(WIN32)
    // We explicitly ask for no rights other than synchronization.
    // This gives us the best chance of succeeding.
    result->thread_ = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
    if (!result->thread_)
      LOG_GLE(LS_ERROR) << "Unable to get handle to thread.";
#elif defined(POSIX)
    result->thread_ = pthread_self();
#endif
    result->owned_ = false;
    result->started_ = true;
    SetCurrent(result);
  }

  return result;
}

// static
void ThreadManager::UnwrapCurrentThread() {
  Thread* t = CurrentThread();
  if (t && !(t->IsOwned())) {
    // Clears the platform-specific thread-specific storage.
    SetCurrent(NULL);
#ifdef WIN32
    if (!CloseHandle(t->thread_)) {
      LOG_GLE(LS_ERROR) << "When unwrapping thread, failed to close handle.";
    }
#endif
    t->started_ = false;
    delete t;
  }
}

void ThreadManager::Add(Thread *thread) {
  CritScope cs(&crit_);
  threads_.push_back(thread);
}

void ThreadManager::Remove(Thread *thread) {
  CritScope cs(&crit_);
  threads_.erase(std::remove(threads_.begin(), threads_.end(), thread),
                 threads_.end());
}

void ThreadManager::StopAllThreads_() {
  // TODO: In order to properly implement, Threads need to be ref-counted.
  CritScope cs(&g_thmgr.crit_);
  for (size_t i = 0; i < g_thmgr.threads_.size(); ++i) {
    g_thmgr.threads_[i]->Stop();
  }
}

struct ThreadInit {
  Thread* thread;
  Runnable* runnable;
};

Thread::Thread(SocketServer* ss)
    : MessageQueue(ss),
      priority_(PRIORITY_NORMAL),
      started_(false),
      has_sends_(false),
#if defined(WIN32)
      thread_(NULL),
#endif
      owned_(true) {
	log_detail = false;
  g_thmgr.Add(this);
  SetName("Thread", this);  // default name
}

Thread::~Thread() {
  Stop();
  if (active_)
    Clear(NULL);
  g_thmgr.Remove(this);
}

bool Thread::SleepMs(int milliseconds) {
#ifdef WIN32
  ::Sleep(milliseconds);
  return true;
#else
  // POSIX has both a usleep() and a nanosleep(), but the former is deprecated,
  // so we use nanosleep() even though it has greater precision than necessary.
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  int ret = nanosleep(&ts, NULL);
  if (ret != 0) {
    LOG_ERR(LS_WARNING) << "nanosleep() returning early";
    return false;
  }
  return true;
#endif
}

bool Thread::SetName(const std::string& name, const void* obj) {
  if (started_) return false;
  name_ = name;
  if (obj) {
    char buf[16];
    sprintfn(buf, sizeof(buf), " 0x%p", obj);
    name_ += buf;
  }
  return true;
}

bool Thread::SetPriority(ThreadPriority priority) {
  if (started_) return false;
  priority_ = priority;
  return true;
}

bool Thread::Start(Runnable* runnable) {
  ASSERT(owned_);
  if (!owned_) return false;
  ASSERT(!started_);
  if (started_) return false;

  ThreadInit* init = new ThreadInit;
  init->thread = this;
  init->runnable = runnable;
#if defined(WIN32)
  DWORD flags = 0;
  if (priority_ != PRIORITY_NORMAL) {
    flags = CREATE_SUSPENDED;
  }
  thread_ = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PreRun, init, flags,
                         NULL);
  if (thread_) {
    if (priority_ != PRIORITY_NORMAL) {
      if (priority_ == PRIORITY_HIGH) {
        ::SetThreadPriority(thread_, THREAD_PRIORITY_HIGHEST);
      } else if (priority_ == PRIORITY_ABOVE_NORMAL) {
        ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
      } else if (priority_ == PRIORITY_IDLE) {
        ::SetThreadPriority(thread_, THREAD_PRIORITY_IDLE);
      }
      ::ResumeThread(thread_);
    }
  } else {
    return false;
  }
#elif defined(POSIX)
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (priority_ != PRIORITY_NORMAL) {
    if (priority_ == PRIORITY_IDLE) {
      // There is no POSIX-standard way to set a below-normal priority for an
      // individual thread (only whole process), so let's not support it.
      LOG(LS_WARNING) << "PRIORITY_IDLE not supported";
    } else {
      // Set real-time round-robin policy.
      if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        LOG(LS_ERROR) << "pthread_attr_setschedpolicy";
      }
      struct sched_param param;
      if (pthread_attr_getschedparam(&attr, &param) != 0) {
        LOG(LS_ERROR) << "pthread_attr_getschedparam";
      } else {
        // The numbers here are arbitrary.
        if (priority_ == PRIORITY_HIGH) {
          param.sched_priority = 6;           // 6 = HIGH
        } else {
          ASSERT(priority_ == PRIORITY_ABOVE_NORMAL);
          param.sched_priority = 4;           // 4 = ABOVE_NORMAL
        }
        if (pthread_attr_setschedparam(&attr, &param) != 0) {
          LOG(LS_ERROR) << "pthread_attr_setschedparam";
        }
      }
    }
  }
  int error_code = pthread_create(&thread_, &attr, PreRun, init);
  if (0 != error_code) {
    LOG(LS_ERROR) << "Unable to create pthread, error " << error_code;
    return false;
  }
#endif
  started_ = true;
  return true;
}

void Thread::Join() {
	if(log_detail)LOG(LS_VERBOSE) << "Thread::Join - 1";
  if (started_) {
    ASSERT(!IsCurrent());

#if defined(WIN32)
    if(log_detail)LOG(LS_VERBOSE) << "Thread::Join - 2";
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = NULL;
#elif defined(POSIX)
    if(log_detail)LOG(LS_VERBOSE) << "Thread::Join - 3";
    void *pv;
    pthread_join(thread_, &pv);
#endif
    started_ = false;
  }
  if(log_detail)LOG(LS_VERBOSE) << "Thread::Join - Done";
}

#ifdef WIN32
// As seen on MSDN.
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx
#define MSDEV_SET_THREAD_NAME  0x406D1388
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;
  LPCSTR szName;
  DWORD dwThreadID;
  DWORD dwFlags;
} THREADNAME_INFO;

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = szThreadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;

  __try {
    RaiseException(MSDEV_SET_THREAD_NAME, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<DWORD*>(&info));
  }
  __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}
#endif  // WIN32

void* Thread::PreRun(void* pv) {
  ThreadInit* init = static_cast<ThreadInit*>(pv);
  ThreadManager::SetCurrent(init->thread);

  if(log_detail)LOG(LS_VERBOSE) << "Thread::PreRun - Thread name: " << init->thread->name_;

#if defined(WIN32)
  SetThreadName(GetCurrentThreadId(), init->thread->name_.c_str());
#elif defined(POSIX)
  // TODO: See if naming exists for pthreads.
#endif
#ifdef OSX_USE_COCOA
  // Make sure the new thread has an autoreleasepool
  ScopedAutoreleasePool pool;
#endif
  if (init->runnable) {
    init->runnable->Run(init->thread);
  } else {
    init->thread->Run();
  }
  delete init;
  return NULL;
}

void Thread::Run() {
	////if(log_detail)LOG(LS_VERBOSE) << "Run() called in thread, name is: " << this->name();
  ProcessMessages(kForever);
}

bool Thread::IsOwned() {
  return owned_;
}

void Thread::Stop() {
	if(log_detail)LOG(LS_VERBOSE) << "Thread::Stop - calling MessageQueue::Quit";
  MessageQueue::Quit();

  if(log_detail)LOG(LS_VERBOSE) << "Thread::Stop - calling Join()";
  Join();
  
  if(log_detail)LOG(LS_VERBOSE) << "Thread::Stop - calling Join() returned";
}

void Thread::Send(MessageHandler *phandler, uint32 id, MessageData *pdata) {

	////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - id: " << id;

	if (fStop_)
	{
		  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - fStop_ is true, message ignored.";
		  return;
	}

	////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 1";

  // Sent messages are sent to the MessageHandler directly, in the context
  // of "thread", like Win32 SendMessage. If in the right context,
  // call the handler directly.

  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (IsCurrent()) {
    phandler->OnMessage(&msg);
    return;
  }

  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 2";

  AutoThread thread;
  Thread *current_thread = Thread::Current();
  ASSERT(current_thread != NULL);  // AutoThread ensures this

  bool ready = false;
  {
    CritScope cs(&crit_);
    EnsureActive();
    _SendMessage smsg;
    smsg.thread = current_thread;
    smsg.msg = msg;
    smsg.ready = &ready;
    sendlist_.push_back(smsg);
    has_sends_ = true;
  }

  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 3";

  // Wait for a reply

  ss_->WakeUp();

  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 4";

  bool waited = false;
  while (!ready) {

	  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - Waiting - 1";

    current_thread->ReceiveSends();

	  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - Waiting - 2";

    current_thread->socketserver()->Wait(kForever, false);

	  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - Waiting - 3";

    waited = true;
  }

  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 5";

  // Our Wait loop above may have consumed some WakeUp events for this
  // MessageQueue, that weren't relevant to this Send.  Losing these WakeUps can
  // cause problems for some SocketServers.
  //
  // Concrete example:
  // Win32SocketServer on thread A calls Send on thread B.  While processing the
  // message, thread B Posts a message to A.  We consume the wakeup for that
  // Post while waiting for the Send to complete, which means that when we exit
  // this loop, we need to issue another WakeUp, or else the Posted message
  // won't be processed in a timely manner.

  if (waited) {

	  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 5 WakeUp";

    current_thread->socketserver()->WakeUp();
  }

  ////if(log_detail)LOG(LS_VERBOSE) << "Thread::Send - 6";
}

void Thread::ReceiveSends() {
  // Before entering critical section, check boolean.

//	  //if(log_detail)LOG(LS_VERBOSE) << "Thread::ReceiveSends - 1";

  if (!has_sends_)
  {
//	  //if(log_detail)LOG(LS_VERBOSE) << "Thread::ReceiveSends - 2";
	    return;
  }


//  //if(log_detail)LOG(LS_VERBOSE) << "Thread::ReceiveSends - 3";

  // Receive a sent message. Cleanup scenarios:
  // - thread sending exits: We don't allow this, since thread can exit
  //   only via Join, so Send must complete.
  // - thread receiving exits: Wakeup/set ready in Thread::Clear()
  // - object target cleared: Wakeup/set ready in Thread::Clear()
  crit_.Enter();
  while (!sendlist_.empty()) {

//	  //if(log_detail)LOG(LS_VERBOSE) << "Thread::ReceiveSends - 4";

    _SendMessage smsg = sendlist_.front();
    sendlist_.pop_front();
    crit_.Leave();
    smsg.msg.phandler->OnMessage(&smsg.msg);
    crit_.Enter();
    *smsg.ready = true;
    smsg.thread->socketserver()->WakeUp();
  }
  has_sends_ = false;
  crit_.Leave();

//  //if(log_detail)LOG(LS_VERBOSE) << "Thread::ReceiveSends - 5";
}

void Thread::Clear(MessageHandler *phandler, uint32 id,
                   MessageList* removed) {
  CritScope cs(&crit_);

  // Remove messages on sendlist_ with phandler
  // Object target cleared: remove from send list, wakeup/set ready
  // if sender not NULL.

  std::list<_SendMessage>::iterator iter = sendlist_.begin();
  while (iter != sendlist_.end()) {
    _SendMessage smsg = *iter;
    if (smsg.msg.Match(phandler, id)) {
      if (removed) {
        removed->push_back(smsg.msg);
      } else {
        delete smsg.msg.pdata;
      }
      iter = sendlist_.erase(iter);
      *smsg.ready = true;
      smsg.thread->socketserver()->WakeUp();
      continue;
    }
    ++iter;
  }

  MessageQueue::Clear(phandler, id, removed);
}

bool Thread::ProcessMessages(int cmsLoop) {
	////if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages() called in thread.";

  uint32 msEnd = (kForever == cmsLoop) ? 0 : TimeAfter(cmsLoop);

//  //if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages 1";

  int cmsNext = cmsLoop;

  while (true) {

//	  //if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages 2";

    Message msg;
    if (!Get(&msg, cmsNext))
    {
    	 ////if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages - Didn't get any msg.";

    	 return !IsQuitting();
    }

//    //if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages - Got a message, dispatch now.";
    //crashed here!!!!
    Dispatch(&msg);

//    //if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages 5";

    if (cmsLoop != kForever) {
      cmsNext = TimeUntil(msEnd);

      ////if(log_detail)LOG(LS_VERBOSE) << "ProcessMessages 6";

      if (cmsNext < 0)
        return true;
    }
  }
}

AutoThread::AutoThread(SocketServer* ss) : Thread(ss) {
  if (!ThreadManager::CurrentThread()) {
    ThreadManager::SetCurrent(this);
  }
}

AutoThread::~AutoThread() {
  if (ThreadManager::CurrentThread() == this) {
    ThreadManager::SetCurrent(NULL);
  }
}

#ifdef WIN32
void ComThread::Run() {
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ASSERT(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    Thread::Run();
    CoUninitialize();
  } else {
    LOG(LS_ERROR) << "CoInitialize failed, hr=" << hr;
  }
}
#endif

}  // namespace talk_base
