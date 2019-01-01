/*
 * VueceNetworkPlayerFsm.h
 *
 *  Created on: Jan 27, 2015
 *      Author: jingjing
 */

#ifndef VUECE_NETWORK_PLAYER_FSM_H
#define VUECE_NETWORK_PLAYER_FSM_H

#include <iostream>
#include <string>

#include "VueceNativeInterface.h"
#include "VueceConstants.h"

namespace cricket {
	class VueceMediaStreamSessionClient;
}

class VueceNetworkPlayerFsm {
public:
	static void Init();
	static void UnInit();
	static void RegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client);
	static void UnRegisterUpperLayerNotificationListener(cricket::VueceMediaStreamSessionClient* client);
	static vuece::NetworkPlayerState GetNetworkPlayerState(void);
	static void LogNetworkPlayerState(vuece::NetworkPlayerState state);
	static void GetNetworkPlayerEventString(vuece::NetworkPlayerEvent e, char* buf);
	static void GetNetworkPlayerStateString(vuece::NetworkPlayerState s, char* buf);
	static void LogStateTranstion(vuece::NetworkPlayerState pre_state, vuece::NetworkPlayerState new_state);
	static void FireNetworkPlayerStateChangeNotification(vuece::NetworkPlayerEvent e, vuece::NetworkPlayerState s);
	static void FireNotification(VueceStreamPlayerNotificaiontMsg* notification);
	static void SetNetworkPlayerState(vuece::NetworkPlayerState state);
};

#endif /* VUECESTREAMPLAYER_H_ */
