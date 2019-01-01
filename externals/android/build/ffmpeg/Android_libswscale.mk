##lib swcale###################
LOCAL_PATH:= $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/ffmpeg/libswscale/
include $(CLEAR_VARS)

LOCAL_MODULE := libswscale

LOCAL_SRC_FILES = \
	options.c \
	rgb2rgb.c \
	swscale.c \
	swscale_unscaled.c \
	utils.c \
	yuv2rgb.c \
	output.c \
	input.c 

LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H

LOCAL_ARM_MODE := arm

#for including config.h:
LOCAL_C_INCLUDES += $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/android/build/ffmpeg  \
					$(LOCAL_PATH)/ \
					$(LOCAL_PATH)/../

LOCAL_STATIC_LIBRARIES := libavutil

include $(BUILD_STATIC_LIBRARY)

