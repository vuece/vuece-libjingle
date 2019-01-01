
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

common_CFLAGS := \
		-D__ANDROID__ \
		-DANDROID \
		-DUSE_CORKSCREW

LOCAL_MODULE    := libcoffeecatch

LOCAL_C_INCLUDES := $(LOCAL_PATH)/src

CCATCH_SRC_DIR := src/

LOCAL_SRC_FILES := \
$(CCATCH_SRC_DIR)/coffeecatch.c \
$(CCATCH_SRC_DIR)/coffeejni.c

LOCAL_CFLAGS += $(common_CFLAGS)
LOCAL_CFLAGS += -funwind-tables -Wl,--no-merge-exidx-entries

include $(BUILD_STATIC_LIBRARY)




