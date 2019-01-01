/*
 * VueceNativeInterface.cc
 *
 *  Created on: 2014-9-28
 *      Author: Jingjing Sun
 */

#include "VueceNativeInterface.h"
#if defined(WIN32)
#include "VueceNativeClientImplWin.h"
#elif defined(ANDROID)
#include "VueceNativeClientImplAndroid.h"
#endif

#include "talk/base/logging.h"

using namespace vuece;

VueceNativeInterface* VueceNativeInterface::CreateInstance(InitData* init_data)
{
#if defined(WIN32)
	return new VueceNativeClientImplWin(init_data);
#elif defined(ANDROID)
	return new VueceNativeClientImplAndroid(init_data);
#else
	return NULL;
#endif

	return NULL;
}

void VueceNativeInterface::LogClientState(int state)
{
	switch(state)
	{
	case CLIENT_STATE_OFFLINE:
	{
		LOG(INFO) << "VueceNativeInterface::Current client state is OFFLINE";
		break;
	}
	case CLIENT_STATE_CONNECTING:
	{
		LOG(INFO) << "VueceNativeInterface::Current client state is CONNECTING";

		break;
	}
	case CLIENT_STATE_ONLINE:
	{
		LOG(INFO) << "VueceNativeInterface::Current client state is ONLINE";

		break;
	}
	case CLIENT_STATE_DISCONNECTING:
	{
		LOG(INFO) << "VueceNativeClientImplWin::Current client state is DISCONNECTING";

		break;
	}
	default:
	{
		LOG(LS_ERROR) << "VueceNativeInterface::Unknown client state: " << state;
		break;
	}

	}
}



void VueceNativeInterface::LogClientEvent(int event)
{
	switch(event)
	{
	case CLIENT_EVENT_LOGGING_IN:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is LOGGING_IN";
		break;
	}
	case CLIENT_EVENT_LOGGING_OUT:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is LOGGING_OUT";

		break;
	}
	case CLIENT_EVENT_LOGIN_OK:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is LOGIN_OK";

		break;
	}
	case CLIENT_EVENT_LOGOUT_OK:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is LOGOUT_OK";

		break;
	}
	case CLIENT_EVENT_OPERATION_TIMEOUT:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is OPERATION_TIMEOUT";

		break;
	}
	case CLIENT_EVENT_AUTH_MISSING_PARAM:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is AUTH_MISSING_PARAM";

		break;
	}
	case CLIENT_EVENT_AUTH_ERR:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is AUTH_ERR";

		break;
	}
	case CLIENT_EVENT_NETWORK_ERR:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is NETWORK_ERR";

		break;
	}
	case CLIENT_EVENT_SYSTEM_ERR:
	{
		LOG(INFO) << "VueceNativeInterface::Current client event is SYSTEM_ERR";

		break;
	}
	case CLIENT_EVENT_NONE:
		{
			LOG(INFO) << "VueceNativeInterface::Current client event is NONE";

			break;
		}
	default:
	{
		LOG(LS_ERROR) << "VueceNativeInterface::Unknown client event: " << event;
		break;
	}

	}
}


