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

#ifndef _STATUS_H_
#define _STATUS_H_

#include "talk/xmpp/jid.h"
#include "talk/xmpp/constants.h"

#define GOOGLE_CLIENT_NODE "http://www.google.com/xmpp/client/caps"

namespace buzz {

class Status {
public:
  Status() :
    pri_(0),
    show_(SHOW_NONE),
    available_(false),
    invisible_(false),
    e_code_(0),
    feedback_probation_(false),
    know_capabilities_(false),
    phone_capability_(false),
    fileshare_capability_(false),
    pmuc_capability_(false),
    video_capability_(false),
    camera_capability_(false),
    vhub_capability_(false),
    is_vuece_device_(false),
    is_google_client_(false) {}

  ~Status() {}

  // These are arranged in "priority order", i.e., if we see
  // two statuses at the same priority but with different Shows,
  // we will show the one with the highest show in the following
  // order.
  enum Show {
    SHOW_NONE     = 0,
    SHOW_OFFLINE  = 1,
    SHOW_XA       = 2,
    SHOW_AWAY     = 3,
    SHOW_DND      = 4,
    SHOW_ONLINE   = 5,
    SHOW_CHAT     = 6,
  };

  const Jid & jid() const { return jid_; }
  int priority() const { return pri_; }
  Show show() const { return show_; }
  const std::string & status() const { return status_; }
  bool available() const { return available_ ; }
  bool invisible() const { return invisible_ ; }
  int error_code() const { return e_code_; }
  const std::string & error_string() const { return e_str_; }
  bool know_capabilities() const { return know_capabilities_; }
  bool phone_capability() const { return phone_capability_; }
  bool fileshare_capability() const { return fileshare_capability_; }
  bool pmuc_capability() const { return pmuc_capability_; }
  bool video_capability() const { return video_capability_; }
  bool camera_capability() const { return camera_capability_; }
  bool vhub_capability() const { return vhub_capability_; }
  bool is_google_client() const { return is_google_client_; }
  bool is_vuece_device() const { return is_vuece_device_; }
  const std::string & version() const { return version_; }
  const std::string & photo_hash() const { return photo_hash_; }
  bool feedback_probation() const { return feedback_probation_; }
  const std::string& sent_time() const { return sent_time_; }
  const std::string & hub_name() const { return hub_name_; }
  const std::string & hub_id() const { return hub_id_; }
  const std::string & device_name() const { return device_name_; }


  void set_jid(const Jid & jid) { jid_ = jid; }
  void set_priority(int pri) { pri_ = pri; }
  void set_show(Show show) { show_ = show; }
  void set_status(const std::string & status) { status_ = status; }
  void set_available(bool a) { available_ = a; }
  void set_invisible(bool a) { invisible_ = a; }
  void set_error(int e_code, const std::string e_str)
      { e_code_ = e_code; e_str_ = e_str; }
  void set_know_capabilities(bool f) { know_capabilities_ = f; }
  void set_phone_capability(bool f) { phone_capability_ = f; }
  void set_fileshare_capability(bool f) { fileshare_capability_ = f; }
  void set_pmuc_capability(bool f) { pmuc_capability_ = f; }
  void set_video_capability(bool f) { video_capability_ = f; }
  void set_camera_capability(bool f) { camera_capability_ = f; }
  void set_vhub_capability(bool f) { vhub_capability_ = f; }
  void set_is_google_client(bool f) { is_google_client_ = f; }
  void set_is_vuece_device(bool f) { is_vuece_device_ = f; }
  void set_version(const std::string & v) { version_ = v; }
  void set_photo_hash(const std::string & h) { photo_hash_ = h; }
  void set_feedback_probation(bool f) { feedback_probation_ = f; }
  void set_sent_time(const std::string& time) { sent_time_ = time; }
  void set_hub_name(const std::string & hub_name) { hub_name_ = hub_name; }
  void set_hub_id(const std::string & hub_id) { hub_id_ = hub_id; }
  void set_device_name(const std::string & device_name) { device_name_ = device_name; }


    
  void UpdateWith(const Status & new_value) {
    if (!new_value.know_capabilities()) {
       bool k = know_capabilities();
       bool i = is_google_client();
       bool p = phone_capability();
       std::string v = version();

       *this = new_value;

       set_know_capabilities(k);
       set_is_google_client(i);
       set_phone_capability(p);
       set_version(v);
    }
    else {
      *this = new_value;
    }
  }

  bool HasQuietStatus() const {
    if (status_.empty())
      return false;
    return !(QuietStatus().empty());
  }

  // Knowledge of other clients' silly automatic status strings -
  // Don't show these.
  std::string QuietStatus() const {
    if (jid_.resource().find("Psi") != std::string::npos) {
      if (status_ == "Online" ||
          status_.find("Auto Status") != std::string::npos)
        return STR_EMPTY;
    }
    if (jid_.resource().find("Gaim") != std::string::npos) {
      if (status_ == "Sorry, I ran out for a bit!")
        return STR_EMPTY;
    }
    return TrimStatus(status_);
  }

  std::string ExplicitStatus() const {
    std::string result = QuietStatus();
    if (result.empty()) {
      result = ShowStatus();
    }
    return result;
  }

  std::string ShowStatus() const {
    std::string result;
    if (!available()) {
      result = "Offline";
    }
    else {
      switch (show()) {
        case SHOW_OFFLINE:
          result = "Offline";
          break;
        case SHOW_AWAY:
        case SHOW_XA:
          result = "Idle";
          break;
        case SHOW_DND:
          result = "Busy";
          break;
        case SHOW_CHAT:
          result = "Chatty";
          break;
        default:
          result = "Available";
          break;
      }
    }
    return result;
  }

  static std::string TrimStatus(const std::string & st) {
    std::string s(st);
    int j = 0;
    bool collapsing = true;
    for (unsigned int i = 0; i < s.length(); i+= 1) {
      if (s[i] <= ' ' && s[i] >= 0) {
        if (collapsing) {
          continue;
        }
        else {
          s[j] = ' ';
          j += 1;
          collapsing = true;
        }
      }
      else {
        s[j] = s[i];
        j += 1;
        collapsing = false;
      }
    }
    if (collapsing && j > 0) {
      j -= 1;
    }
    s.erase(j, s.length());
    return s;
  }

private:
  Jid jid_;
  int pri_;
  Show show_;
  std::string status_;
  bool available_;
  bool invisible_;
  int e_code_;
  std::string e_str_;
  bool feedback_probation_;

  // capabilities (valid only if know_capabilities_
  bool know_capabilities_;
  bool phone_capability_;
  bool fileshare_capability_;
  bool pmuc_capability_;
  bool video_capability_;
  bool camera_capability_;
  bool vhub_capability_;
  bool is_google_client_;
  bool is_vuece_device_;
  std::string version_;
  std::string photo_hash_;

  std::string sent_time_; // from the jabber:x:delay element
  std::string hub_name_;
  std::string hub_id_;
  std::string device_name_;
};

class MucStatus : public Status {
public:
  MucStatus() : audio_src_id_(0), video_src_id_(0) {}
  uint32 audio_src_id() const { return audio_src_id_; }
  uint32 video_src_id() const { return video_src_id_; }
  void set_audio_src_id(uint32 audio_src_id) {
    audio_src_id_ = audio_src_id;
  }
  void set_video_src_id(uint32 video_src_id) {
    video_src_id_ = video_src_id;
  }
private:
  uint32 audio_src_id_;
  uint32 video_src_id_;
};

}


#endif
