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

#include <iostream>

#include "talk/base/logging.h"

#include "talk/xmllite/qname.h"
#include "xmppauth.h"

#include <algorithm>

#include "talk/xmpp/saslcookiemechanism.h"
#include "talk/xmpp/saslplainmechanism.h"

XmppAuth::XmppAuth() : done_(false) {
}

XmppAuth::~XmppAuth() {
}

void XmppAuth::StartPreXmppAuth(const buzz::Jid & jid,
	const talk_base::SocketAddress & server,
	const talk_base::CryptString & pass,
    const std::string& auth_mechanism,
	const std::string & auth_token) {

	LOG(LS_VERBOSE) << "XmppAuth::StartPreXmppAuth";

		jid_ = jid;
		passwd_ = pass;
		auth_token_ = auth_token;
		auth_mechanism_ = auth_mechanism;
		done_ = true;

		SignalAuthDone();
}

std::string XmppAuth::ChooseBestSaslMechanism(
	const std::vector<std::string> & mechanisms,
	bool encrypted) {

		std::cout << "XmppAuth::ChooseBestSaslMechanism\n";

		LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - START";

		std::vector<std::string>::const_iterator it;

		/*
		 * SAMPLE MESSAGE
		 * [[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:123): RECV <<<<<<<<<<<<<<<< : Thu Nov 20 22:01:58 2014
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):    <stream:features>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):      <mechanisms xmlns="urn:ietf:params:xml:ns:xmpp-sasl">
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        <mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:161):          X-OAUTH2
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        </mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        <mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:161):          X-GOOGLE-TOKEN
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        </mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        <mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:161):          PLAIN
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):        </mechanism>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):      </mechanisms>
[[4] 2014-11-20 22:01:58] [20dc] Warning(VueceNativeClientImplWin.cpp:141):    </stream:features>
		 */
//		it = std::find(mechanisms.begin(), mechanisms.end(), "X-OAUTH2");
//		if (it != mechanisms.end())
//		{
//			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - X-OAUTH2";
//			return "X-OAUTH2";
//		}

		// a token is the weakest auth - 15s, service-limited, so prefer it.
		it = std::find(mechanisms.begin(), mechanisms.end(), "X-OAUTH2");

		if (it != mechanisms.end() && !auth_token_.empty())
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - Return X-OAUTH2";
		    return buzz::AUTH_MECHANISM_OAUTH2;
		}
		else
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - Auth method X-OAUTH2 not found";
		}

		// a cookie is the next weakest - 14 days
		it = std::find(mechanisms.begin(), mechanisms.end(), "X-GOOGLE-TOKEN");
		if (it != mechanisms.end() && !auth_token_.empty())
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - Return X-GOOGLE-TOKEN";
		    return buzz::AUTH_MECHANISM_GOOGLE_TOKEN;
		}
		else
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - Auth method X-GOOGLE-TOKEN not found";
		}

		it = std::find(mechanisms.begin(), mechanisms.end(), "PLAIN");
		if (it != mechanisms.end())
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - PLAIN";
		    return buzz::AUTH_MECHANISM_PLAIN;
		}
		else
		{
			LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - Auth method PLAIN not found";
		}

		// No good mechanism found
		LOG(LS_VERBOSE) << "XmppAuth::ChooseBestSaslMechanism - END, No good mechanism found";
		return "";
}

buzz::SaslMechanism* XmppAuth::CreateSaslMechanism(
		const std::string & mechanism)
{

	LOG(LS_VERBOSE) << "XmppAuth::CreateSaslMechanism";

	if (mechanism == buzz::AUTH_MECHANISM_OAUTH2)
	{

		LOG(LS_VERBOSE)
				<< "XmppAuth::CreateSaslMechanism - Mechanism is OATH2, return SaslCookieMechanism";

	    return new buzz::SaslCookieMechanism(mechanism, jid_.Str(), auth_token_, "oauth2");
	}
	else if (mechanism == buzz::AUTH_MECHANISM_GOOGLE_TOKEN)
	{

		LOG(LS_VERBOSE)
				<< "XmppAuth::CreateSaslMechanism - Return SaslCookieMechanism";

		return new buzz::SaslCookieMechanism(mechanism, jid_.Str(),
				auth_token_);
	}
	else if (mechanism == buzz::AUTH_MECHANISM_PLAIN)
	{

		LOG(LS_VERBOSE)
				<< "XmppAuth::CreateSaslMechanism - Return SaslPlainMechanism";

		return new buzz::SaslPlainMechanism(jid_, passwd_);
	}
	else
	{
		LOG(LS_VERBOSE) << "XmppAuth::CreateSaslMechanism - Return NULL";

		return NULL;
	}
}
