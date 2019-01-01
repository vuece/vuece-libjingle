#########
# jRTP lib  #
#########

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libjrtp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

#LOCAL_CFLAGS := $(MY_PJSIP_FLAGS)
JRTP_SRC_DIR := src

LOCAL_SRC_FILES := \
$(JRTP_SRC_DIR)/rtcpapppacket.cpp \
$(JRTP_SRC_DIR)/rtcppacket.cpp \
$(JRTP_SRC_DIR)/rtcpsrpacket.cpp  \
$(JRTP_SRC_DIR)/rtpipv4address.cpp   \
$(JRTP_SRC_DIR)/rtppollthread.cpp \
$(JRTP_SRC_DIR)/rtpsession.cpp \
$(JRTP_SRC_DIR)/rtptimeutilities.cpp \
$(JRTP_SRC_DIR)/rtcpbyepacket.cpp \
$(JRTP_SRC_DIR)/rtcprrpacket.cpp \
$(JRTP_SRC_DIR)/rtpcollisionlist.cpp \
$(JRTP_SRC_DIR)/rtpipv6address.cpp \
$(JRTP_SRC_DIR)/rtprandom.cpp \
$(JRTP_SRC_DIR)/rtpsessionparams.cpp   \
$(JRTP_SRC_DIR)/rtpudpv4transmitter.cpp \
$(JRTP_SRC_DIR)/rtcpcompoundpacketbuilder.cpp \
$(JRTP_SRC_DIR)/rtcpscheduler.cpp \
$(JRTP_SRC_DIR)/rtpdebug.cpp \
$(JRTP_SRC_DIR)/rtplibraryversion.cpp  \
$(JRTP_SRC_DIR)/rtprandomrand48.cpp \
$(JRTP_SRC_DIR)/rtpsessionsources.cpp \
$(JRTP_SRC_DIR)/rtpudpv6transmitter.cpp \
$(JRTP_SRC_DIR)/rtcpcompoundpacket.cpp  \
$(JRTP_SRC_DIR)/rtcpsdesinfo.cpp \
$(JRTP_SRC_DIR)/rtperrors.cpp \
$(JRTP_SRC_DIR)/rtppacketbuilder.cpp \
$(JRTP_SRC_DIR)/rtprandomrands.cpp \
$(JRTP_SRC_DIR)/rtpsourcedata.cpp \
$(JRTP_SRC_DIR)/rtcppacketbuilder.cpp \
$(JRTP_SRC_DIR)/rtcpsdespacket.cpp \
$(JRTP_SRC_DIR)/rtpinternalsourcedata.cpp   \
$(JRTP_SRC_DIR)/rtppacket.cpp \
$(JRTP_SRC_DIR)/rtprandomurandom.cpp \
$(JRTP_SRC_DIR)/rtpsources.cpp
		   
LOCAL_CPP_EXTENSION := .cpp

include $(BUILD_STATIC_LIBRARY)




