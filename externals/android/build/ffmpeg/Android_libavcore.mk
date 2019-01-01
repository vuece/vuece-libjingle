
LOCAL_PATH:= $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/ffmpeg/
include $(CLEAR_VARS)

LOCAL_MODULE := libavcore


LOCAL_SRC_FILES := \
	libavcore/parseutils.c \
#	libavcore/utils.c 

#	libavcore/samplefmt.c \
#	libavcore/audioconvert.c


LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H



#for including config.h:
LOCAL_C_INCLUDES += $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/android/build/ffmpeg  $(LOCAL_PATH)/

LOCAL_STATIC_LIBRARIES := libavutil

include $(BUILD_STATIC_LIBRARY)

