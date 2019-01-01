/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
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

//-------------------------------------------------
// DUMMY IMPL DEVICEMAGGER FOR VUECE ANDROID LIBRARY
//-------------------------------------------------


#include "talk/session/phone/devicemanager.h"

#if WIN32
#include <atlbase.h>
#include <dbt.h>
#include <strmif.h>  // must come before ks.h
#include <ks.h>
#include <ksmedia.h>
#define INITGUID  // For PKEY_AudioEndpoint_GUID
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <uuids.h>
#include "talk/base/win32.h"  // ToUtf8
#include "talk/base/win32window.h"
#elif OSX
#include <CoreAudio/CoreAudio.h>
#include <QuickTime/QuickTime.h>
#elif LINUX
//#include <libudev.h>
#include <unistd.h>
#ifndef USE_TALK_SOUND
//#include <alsa/asoundlib.h>
#endif
#include "talk/base/linux.h"
#include "talk/base/fileutils.h"
#include "talk/base/pathutils.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/stream.h"
//#include "talk/session/phone/libudevsymboltable.h"
#include "talk/session/phone/v4llookup.h"
#endif

#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/session/phone/mediaengine.h"
#if USE_TALK_SOUND
#include "talk/sound/platformsoundsystem.h"
#include "talk/sound/sounddevicelocator.h"
#include "talk/sound/soundsysteminterface.h"
#endif

namespace cricket {
// Initialize to empty string.
const std::string DeviceManager::kDefaultDeviceName;

#ifdef WIN32
#elif defined(LINUX)
class DeviceWatcher : private talk_base::Dispatcher {
 public:
  explicit DeviceWatcher(DeviceManager* dm);
  bool Start();
  void Stop();

 private:
  virtual uint32 GetRequestedEvents();
  virtual void OnPreEvent(uint32 ff);
  virtual void OnEvent(uint32 ff, int err);
  virtual int GetDescriptor();
  virtual bool IsDescriptorClosed();

  DeviceManager* manager_;
 // LibUDevSymbolTable libudev_;
  struct udev* udev_;
  struct udev_monitor* udev_monitor_;
  bool registered_;
};
//#define LATE(sym) LATESYM_GET(LibUDevSymbolTable, &libudev_, sym)
#elif defined(OSX)
#endif

#ifndef LINUX
static bool ShouldDeviceBeIgnored(const std::string& device_name);
#endif
#ifndef OSX
static bool GetVideoDevices(std::vector<Device>* out);
#endif
#if WIN32
#elif OSX
#endif

DeviceManager::DeviceManager(
#ifdef USE_TALK_SOUND
    SoundSystemFactory *factory
#endif
    )
    : initialized_(false),
      watcher_(new DeviceWatcher(this))
#ifdef USE_TALK_SOUND
      , sound_system_(factory)
#endif
    {
}

DeviceManager::~DeviceManager() {
  if (initialized_) {
    Terminate();
  }
  delete watcher_;
}

bool DeviceManager::Init() {
  if (!initialized_) {
    if (!watcher_->Start()) {
      return false;
    }
    initialized_ = true;
  }
  return true;
}

void DeviceManager::Terminate() {
  if (initialized_) {
    watcher_->Stop();
    initialized_ = false;
  }
}

int DeviceManager::GetCapabilities() {
  std::vector<Device> devices;
  int caps = MediaEngine::VIDEO_RECV;
  if (GetAudioInputDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::AUDIO_SEND;
  }
  if (GetAudioOutputDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::AUDIO_RECV;
  }
  if (GetVideoCaptureDevices(&devices) && !devices.empty()) {
    caps |= MediaEngine::VIDEO_SEND;
  }
  return caps;
}

bool DeviceManager::GetAudioInputDevices(std::vector<Device>* devices) {
  return GetAudioDevicesByPlatform(true, devices);
}

bool DeviceManager::GetAudioOutputDevices(std::vector<Device>* devices) {
  return GetAudioDevicesByPlatform(false, devices);
}

bool DeviceManager::GetAudioInputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(true, name, out);
}

bool DeviceManager::GetAudioOutputDevice(const std::string& name, Device* out) {
  return GetAudioDevice(false, name, out);
}

#ifdef OSX
static bool FilterDevice(const Device& d) {
  return ShouldDeviceBeIgnored(d.name);
}
#endif

bool DeviceManager::GetVideoCaptureDevices(std::vector<Device>* devices) {
  devices->clear();
#ifdef OSX
  if (GetQTKitVideoDevices(devices)) {
    // Now filter out any known incompatible devices
    devices->erase(remove_if(devices->begin(), devices->end(), FilterDevice),
                   devices->end());
    return true;
  }
  return false;
#else
  return GetVideoDevices(devices);
#endif
}

bool DeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
  bool ret = false;
#if WIN32
  // If there are multiple capture devices, we want the first USB one.
  // This avoids issues with defaulting to virtual cameras or grabber cards.
  std::vector<Device> devices;
  ret = (GetVideoDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
    for (size_t i = 0; i < devices.size(); ++i) {
      if (strnicmp(devices[i].id.c_str(), kUsbDevicePathPrefix,
                   ARRAY_SIZE(kUsbDevicePathPrefix) - 1) == 0) {
        *device = devices[i];
        break;
      }
    }
  }
#else
  // We just return the first device.
  std::vector<Device> devices;
  ret = (GetVideoCaptureDevices(&devices) && !devices.empty());
  if (ret) {
    *device = devices[0];
  }
#endif
  return ret;
}


bool DeviceManager::GetAudioDevice(bool is_input, const std::string& name,
                                   Device* out) {
  // If the name is empty, return the default device id.
  if (name.empty() || name == kDefaultDeviceName) {
    *out = Device(name, -1);
    return true;
  }

  std::vector<Device> devices;
  bool ret = is_input ? GetAudioInputDevices(&devices) :
                        GetAudioOutputDevices(&devices);
  if (ret) {
    ret = false;
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].name == name) {
        *out = devices[i];
        ret = true;
        break;
      }
    }
  }
  return ret;
}

bool DeviceManager::GetAudioDevicesByPlatform(bool input,
                                              std::vector<Device>* devs) {
  devs->clear();

#if defined(ANDROID)

  LOG(INFO) << "GetAudioDevicesByPlatform:Android dummy impl.";
  return true;

#elif defined(USE_TALK_SOUND)
  if (!sound_system_.get()) {
    return false;
  }
  SoundSystemInterface::SoundDeviceLocatorList list;
  bool success;
  if (input) {
    success = sound_system_->EnumerateCaptureDevices(&list);
  } else {
    success = sound_system_->EnumeratePlaybackDevices(&list);
  }
  if (!success) {
    LOG(LS_ERROR) << "Can't enumerate devices";
    sound_system_.release();
    return false;
  }
  int index = 0;
  for (SoundSystemInterface::SoundDeviceLocatorList::iterator i = list.begin();
       i != list.end();
       ++i, ++index) {
    devs->push_back(Device((*i)->name(), index));
  }
  SoundSystemInterface::ClearSoundDeviceLocatorList(&list);
  sound_system_.release();
  return true;

#elif defined(WIN32)
  if (talk_base::IsWindowsVistaOrLater()) {
    return GetCoreAudioDevices(input, devs);
  } else {
    return GetWaveDevices(input, devs);
  }

#elif defined(OSX)
  std::vector<AudioDeviceID> dev_ids;
  bool ret = GetAudioDeviceIDs(input, &dev_ids);
  if (ret) {
    for (size_t i = 0; i < dev_ids.size(); ++i) {
      std::string name;
      if (GetAudioDeviceName(dev_ids[i], input, &name)) {
        devs->push_back(Device(name, dev_ids[i]));
      }
    }
  }
  return ret;

#elif defined(LINUX)
  int card = -1, dev = -1;
  snd_ctl_t *handle = NULL;
  snd_pcm_info_t *pcminfo = NULL;

  snd_pcm_info_malloc(&pcminfo);

  while (true) {
    if (snd_card_next(&card) != 0 || card < 0)
      break;

    char *card_name;
    if (snd_card_get_name(card, &card_name) != 0)
      continue;

    char card_string[7];
    snprintf(card_string, sizeof(card_string), "hw:%d", card);
    if (snd_ctl_open(&handle, card_string, 0) != 0)
      continue;

    while (true) {
      if (snd_ctl_pcm_next_device(handle, &dev) < 0 || dev < 0)
        break;
      snd_pcm_info_set_device(pcminfo, dev);
      snd_pcm_info_set_subdevice(pcminfo, 0);
      snd_pcm_info_set_stream(pcminfo, input ? SND_PCM_STREAM_CAPTURE :
                                               SND_PCM_STREAM_PLAYBACK);
      if (snd_ctl_pcm_info(handle, pcminfo) != 0)
        continue;

      char name[128];
      talk_base::sprintfn(name, sizeof(name), "%s (%s)", card_name,
          snd_pcm_info_get_name(pcminfo));
      // TODO: We might want to identify devices with something
      // more specific than just their card number (e.g., the PCM names that
      // aplay -L prints out).
      devs->push_back(Device(name, card));

      LOG(LS_INFO) << "Found device: id = " << card << ", name = "
          << name;
    }
    snd_ctl_close(handle);
  }
  snd_pcm_info_free(pcminfo);
  return true;
#else
  return false;
#endif
}

#if defined(WIN32)

#elif defined(OSX)

#elif defined(LINUX)
static const std::string kVideoMetaPathK2_4("/proc/video/dev/");
static const std::string kVideoMetaPathK2_6("/sys/class/video4linux/");

enum MetaType { M2_4, M2_6, NONE };

static void ScanDeviceDirectory(const std::string& devdir,
                                std::vector<Device>* devices) {


  LOG(INFO) << "ScanDeviceDirectory:Android dummy impl.";

//
//  talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator(
//      talk_base::Filesystem::IterateDirectory());
//
//  if (directoryIterator->Iterate(talk_base::Pathname(devdir))) {
//    do {
//      std::string filename = directoryIterator->Name();
//      std::string device_name = devdir + filename;
//      if (!directoryIterator->IsDots()) {
//        if (filename.find("video") == 0 &&
//            V4LLookup::IsV4L2Device(device_name)) {
//          devices->push_back(Device(device_name, device_name));
//        }
//      }
//    } while (directoryIterator->Next());
//  }


}


static std::string Trim(const std::string& s, const std::string& drop = " \t") {
  std::string::size_type first = s.find_first_not_of(drop);
  std::string::size_type last  = s.find_last_not_of(drop);

  if (first == std::string::npos || last == std::string::npos)
    return std::string("");

  return s.substr(first, last - first + 1);
}

static std::string GetVideoDeviceNameK2_4(const std::string& device_meta_path) {
  talk_base::ConfigParser::MapVector all_values;

  LOG(INFO) << "GetVideoDeviceNameK2_4:Android dummy impl.";

//  talk_base::ConfigParser config_parser;
//  talk_base::FileStream* file_stream =
//      talk_base::Filesystem::OpenFile(device_meta_path, "r");
//
//  if (file_stream == NULL) return "";
//
//  config_parser.Attach(file_stream);
//  config_parser.Parse(&all_values);
//
//  for (talk_base::ConfigParser::MapVector::iterator i = all_values.begin();
//      i != all_values.end(); ++i) {
//    talk_base::ConfigParser::SimpleMap::iterator device_name_i =
//        i->find("name");
//
//    if (device_name_i != i->end()) {
//      return device_name_i->second;
//    }
//  }

  return "";
}

static std::string GetVideoDeviceName(MetaType meta,
    const std::string& device_file_name) {
  std::string device_meta_path;
  std::string device_name;
  std::string meta_file_path;

  LOG(INFO) << "GetVideoDeviceName:Android dummy impl.";

  LOG(LS_INFO) << "Name for " << device_file_name << " is " << device_name;

  return Trim(device_name);
}

static void ScanV4L2Devices(std::vector<Device>* devices) {
  LOG(LS_INFO) << ("Enumerating V4L2 devices");
  MetaType meta;
  std::string metadata_dir;

  LOG(INFO) << "GetVideoDeviceNameK2_4:Android dummy impl.";
  LOG(LS_INFO) << "Total V4L2 devices found : " << devices->size();
}

static bool GetVideoDevices(std::vector<Device>* devices) {
  ScanV4L2Devices(devices);
  return true;
}

DeviceWatcher::DeviceWatcher(DeviceManager* dm)
    : manager_(dm), udev_(NULL), udev_monitor_(NULL), registered_(false) {}

bool DeviceWatcher::Start() {
  // We deliberately return true in the failure paths here because libudev is
  // not a critical component of a Linux system so it may not be present/usable,
  // and we don't want to halt DeviceManager initialization in such a case.
	  LOG(INFO) << "DeviceWatcher::Start:Android dummy impl.";

	  return true;
}

void DeviceWatcher::Stop() {
	  LOG(INFO) << "DeviceWatcher::Stop:Android dummy impl.";
}

uint32 DeviceWatcher::GetRequestedEvents() {
	  LOG(INFO) << "DeviceWatcher::GetRequestedEvents:Android dummy impl.";
  return talk_base::DE_READ;
}

void DeviceWatcher::OnPreEvent(uint32 ff) {
  // Nothing to do.
}

void DeviceWatcher::OnEvent(uint32 ff, int err) {
	LOG(INFO) << "DeviceWatcher::OnEvent:Android dummy impl.";
  manager_->OnDevicesChange();
}

int DeviceWatcher::GetDescriptor() {
	LOG(INFO) << "DeviceWatcher::GetDescriptor:Android dummy impl.";
  return 0;
}

bool DeviceWatcher::IsDescriptorClosed() {
  // If it is closed then we will just get an error in
  // udev_monitor_receive_device and unregister, so we don't need to check for
  // it separately.
  return false;
}

#endif

// TODO: Try to get hold of a copy of Final Cut to understand why we
//               crash while scanning their components on OS X.
#ifndef LINUX
static bool ShouldDeviceBeIgnored(const std::string& device_name) {
  static const char* const kFilteredDevices[] =  {
      "Google Camera Adapter",   // Our own magiccams
#ifdef WIN32
      "Asus virtual Camera",     // Bad Asus desktop virtual cam
      "Bluetooth Video",         // Bad Sony viao bluetooth sharing driver
#elif OSX
      "DVCPRO HD",               // Final cut
      "Sonix SN9C201p",          // Crashes in OpenAComponent and CloseComponent
#endif
  };

  for (int i = 0; i < ARRAY_SIZE(kFilteredDevices); ++i) {
    if (strnicmp(device_name.c_str(), kFilteredDevices[i],
        strlen(kFilteredDevices[i])) == 0) {
      LOG(LS_INFO) << "Ignoring device " << device_name;
      return true;
    }
  }
  return false;
}
#endif

};  // namespace cricket
