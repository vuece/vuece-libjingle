#########
# jRTP lib  #
#########

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libjthread

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

#LOCAL_CFLAGS := $(MY_PJSIP_FLAGS)
JTHRD_SRC_DIR := src/pthread

LOCAL_SRC_FILES := \
$(JTHRD_SRC_DIR)/jmutex.cpp \
$(JTHRD_SRC_DIR)/jthread.cpp

		   
LOCAL_CPP_EXTENSION := .cpp

include $(BUILD_STATIC_LIBRARY)




