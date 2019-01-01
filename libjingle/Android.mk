

LOCAL_PATH:= $(call my-dir)

# We need to build this for both the device (as a shared library)
# and the host (as a static library for tools to use).

# for logging
LOCAL_LDLIBS := -llog

common_CFLAGS := \
		-DLINUX \
		-DANDROID \
		-DPOSIX \
		-DXML_STATIC \
		-DFEATURE_ENABLE_SSL \
		-DSSL_USE_OPENSSL \
		-DHAVE_OPENSSL_SSL_H \
		-DOPENSSL_NO_X509 \
		-DEXPAT_RELATIVE_PATH \
		-D__STDC_CONSTANT_MACROS \
		-DHAVE_PTHREAD=1 \
		-DHAVE_MMX=1 \
		-D__SSE__ \
		-DARCH_X86=1 \
		-DVUECE_APP_ROLE_HUB_CLIENT \
		-DLOGGING=1
		
#		-DFEATURE_ENABLE_VOICEMAIL \

common_C_INCLUDES := \
	talk \
	talk/base \
	talk/session/phone/vuece/ \
	talk/session/fileshare/ \
	$(LOCAL_PATH)/../externals/expat \
	$(LOCAL_PATH)/../externals/jthread/src \
	$(LOCAL_PATH)/../externals/android/openssl/include \
	$(LOCAL_PATH)/../externals/libsrtp/crypto/include \
	$(LOCAL_PATH)/../externals/libsrtp/include \
	$(LOCAL_PATH)/../externals/android/build/ffmpeg/ \
	$(LOCAL_PATH)/../externals/ffmpeg/ \
	$(LOCAL_PATH)/../externals/ffmpeg/libavutil \
	$(LOCAL_PATH)/../externals/ffmpeg/libavcodec \
	$(LOCAL_PATH)/../externals/ffmpeg/libavformat \
	$(LOCAL_PATH)/../externals/ffmpeg/libswscale \
	$(VUECE_LIBJINGLE_LOCATION)/client-core \
	$(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/coffeecatch/src
	
	
common_COPY_HEADERS_TO := libjingle
 
include $(CLEAR_VARS)

MY_JINGLE_SRC_FILES_BASE := \
talk/base/stream.cc \
talk/base/asyncfile.cc \
talk/base/asynchttprequest.cc \
talk/base/asyncpacketsocket.cc \
talk/base/asyncsocket.cc \
talk/base/asynctcpsocket.cc \
talk/base/asyncudpsocket.cc \
talk/base/autodetectproxy.cc \
talk/base/base64.cc \
talk/base/bytebuffer.cc \
talk/base/checks.cc \
talk/base/common.cc \
talk/base/diskcache.cc \
talk/base/event.cc \
talk/base/fileutils.cc \
talk/base/firewallsocketserver.cc \
talk/base/flags.cc \
talk/base/helpers.cc \
talk/base/host.cc \
talk/base/httpbase.cc \
talk/base/httpclient.cc \
talk/base/httpserver.cc \
talk/base/httpcommon.cc \
talk/base/httprequest.cc \
talk/base/logging.cc \
talk/base/md5c.c \
talk/base/messagehandler.cc \
talk/base/messagequeue.cc \
talk/base/nethelpers.cc \
talk/base/network.cc \
talk/base/openssladapter.cc \
talk/base/pathutils.cc \
talk/base/physicalsocketserver.cc \
talk/base/proxydetect.cc \
talk/base/proxyinfo.cc \
talk/base/ratetracker.cc \
talk/base/signalthread.cc \
talk/base/socketadapters.cc \
talk/base/socketaddress.cc \
talk/base/socketaddresspair.cc \
talk/base/socketpool.cc \
talk/base/socketstream.cc \
talk/base/ssladapter.cc \
talk/base/sslsocketfactory.cc \
talk/base/stringdigest.cc \
talk/base/stringencode.cc \
talk/base/stringutils.cc \
talk/base/task.cc \
talk/base/taskparent.cc \
talk/base/taskrunner.cc \
talk/base/thread.cc \
talk/base/timeutils.cc \
talk/base/urlencode.cc \
talk/base/unixfilesystem.cc \
talk/base/streamutils.cc \
talk/base/tarstream.cc \

MY_JINGLE_SRC_FILES_P2P := \
talk/p2p/base/p2pconstants.cc \
talk/p2p/base/p2ptransport.cc \
talk/p2p/base/p2ptransportchannel.cc \
talk/p2p/base/parsing.cc \
talk/p2p/base/port.cc \
talk/p2p/base/pseudotcp.cc \
talk/p2p/base/relayport.cc \
talk/p2p/base/relayserver.cc \
talk/p2p/base/rawtransport.cc \
talk/p2p/base/rawtransportchannel.cc \
talk/p2p/base/session.cc \
talk/p2p/base/sessiondescription.cc \
talk/p2p/base/sessionmanager.cc \
talk/p2p/base/sessionmessages.cc \
talk/p2p/base/stun.cc \
talk/p2p/base/stunport.cc \
talk/p2p/base/stunrequest.cc \
talk/p2p/base/stunserver.cc \
talk/p2p/base/tcpport.cc \
talk/p2p/base/transport.cc \
talk/p2p/base/transportchannel.cc \
talk/p2p/base/transportchannelproxy.cc \
talk/p2p/base/udpport.cc \
talk/p2p/client/basicportallocator.cc \
talk/p2p/client/httpportallocator.cc \
talk/p2p/client/socketmonitor.cc 

MY_JINGLE_SRC_FILES_PHONE_BASE := \
talk/session/phone/vuece/VueceDevVidUtils.cc \
talk/session/phone/VuecePhoneEngine.cc \
talk/session/phone/devicemanager_android_dummy.cc \
talk/session/phone/audiomonitor.cc \
talk/session/phone/call.cc \
talk/session/phone/channel.cc \
talk/session/phone/channelmanager.cc \
talk/session/phone/codec.cc \
talk/session/phone/mediaengine.cc \
talk/session/phone/mediamonitor.cc \
talk/session/phone/mediasessionclient.cc \
talk/session/phone/rtcpmuxfilter.cc \
talk/session/phone/soundclip.cc \
talk/session/phone/srtpfilter.cc

MY_JINGLE_SRC_FILES_TUNNEL := \
talk/session/tunnel/pseudotcpchannel.cc \
talk/session/tunnel/tunnelsessionclient.cc \
talk/session/tunnel/securetunnelsessionclient.cc

MY_JINGLE_SRC_FILES_XMLLITE := \
talk/xmllite/qname.cc \
talk/xmllite/xmlbuilder.cc \
talk/xmllite/xmlconstants.cc \
talk/xmllite/xmlelement.cc \
talk/xmllite/xmlnsstack.cc \
talk/xmllite/xmlparser.cc \
talk/xmllite/xmlprinter.cc 

MY_JINGLE_SRC_FILES_XMPP := \
talk/xmpp/constants.cc \
talk/xmpp/jid.cc \
talk/xmpp/ratelimitmanager.cc \
talk/xmpp/saslmechanism.cc \
talk/xmpp/xmppclient.cc \
talk/xmpp/xmppengineimpl.cc \
talk/xmpp/xmppengineimpl_iq.cc \
talk/xmpp/xmpplogintask.cc \
talk/xmpp/xmppstanzaparser.cc \
talk/xmpp/xmpptask.cc \
talk/xmpp/xmpppump.cc \
talk/xmpp/xmppauth.cc \
talk/xmpp/xmppsocket.cc \
talk/xmpp/xmppthread.cc

MY_JINGLE_SRC_FILE_SHARE := \
talk/session/fileshare/VueceMediaStreamSession.cc \
talk/session/fileshare/VueceMediaStreamSessionClient.cc \
talk/session/fileshare/VueceHttpServerThread.cc \
talk/session/fileshare/VueceMediaStream.cc \
talk/session/fileshare/VueceShareCommon.cc \
talk/session/fileshare/VueceStreamPlayer.cc \
talk/session/fileshare/VueceNetworkPlayerFsm.cc \
talk/session/fileshare/VueceStreamPlayerMonitorThread2.cc \
talk/session/fileshare/VueceMediaDataBumperFsm.cc \
talk/session/fileshare/VueceMemQueue.cc \
talk/session/fileshare/VueceAACDecoder.cc \
talk/session/fileshare/VueceAudioWriter.cc \
talk/session/fileshare/VueceStreamEngine.cc \
talk/session/fileshare/VueceJni.cc

LOCAL_CFLAGS += $(common_CFLAGS)
LOCAL_CFLAGS += -funwind-tables -Wl,--no-merge-exidx-entries

#LOCAL_CFLAGS += -DHAVE_ILBC=1
LOCAL_STATIC_LIBRARIES := libexpat

#ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -DHAVE_AMR
LOCAL_STATIC_LIBRARIES += \
libavcodec \
libavcore \
libavutil \
libswscale \
libavformat
#endif

LOCAL_C_INCLUDES += $(common_C_INCLUDES)

LOCAL_SRC_FILES := $(MY_JINGLE_SRC_FILES_BASE)

ifeq ($(ENABLE_PHONE_ENGINE), 1)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_PHONE_BASE)
endif


LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_BASE)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_XMPP)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_XMLLITE)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_TUNNEL)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILES_P2P)
LOCAL_SRC_FILES += $(MY_JINGLE_SRC_FILE_SHARE)

LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE:= libjingle
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := $(common_COPY_HEADERS_TO)
LOCAL_COPY_HEADERS := $(common_COPY_HEADERS)

LOCAL_CFLAGS += -funwind-tables -Wl,--no-merge-exidx-entries

$(info ---------- Vuece Build Enviroment(libjingle) ------------)
$(info TARGET_ARCH_ABI = $(TARGET_ARCH_ABI))
$(info ----------)
$(info LOCAL_LDLIBS = $(LOCAL_LDLIBS))
$(info ----------)
$(info LOCAL_CFLAGS = $(LOCAL_CFLAGS))
$(info ----------)
$(info LOCAL_LDFLAGS = $(LOCAL_LDFLAGS))
$(info ----------)
$(info LOCAL_STATIC_LIBRARIES = $(LOCAL_STATIC_LIBRARIES))

include $(BUILD_STATIC_LIBRARY)
