##lib swcale###################
LOCAL_PATH:= $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/ffmpeg/
include $(CLEAR_VARS)

LOCAL_MODULE := libavformat

LOCAL_SRC_FILES = \
	libavformat/allformats.c \
	libavformat/aacdec.c \
	libavformat/utils.c \
	libavformat/cutils.c \
	libavformat/metadata.c \
	libavformat/metadata_compat.c \
	libavformat/seek.c \
	libavformat/aviobuf.c \
	libavformat/avio.c \
	libavformat/options.c \
	libavformat/id3v1.c \
	libavformat/id3v2.c \
	libavformat/rawdec.c \
	libavformat/riff.c \
	libavformat/file.c \
	libavformat/avidec.c \
	libavformat/flvdec.c \
	libavformat/flvenc.c \
	libavformat/h264dec.c \
	libavformat/rawenc.c \
	libavformat/riff.c \
	libavformat/avc.c
	
	
LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H

LOCAL_ARM_MODE := arm

#for including config.h:
LOCAL_C_INCLUDES += $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/android/build/ffmpeg  \
					$(LOCAL_PATH)/libavformat \
					$(LOCAL_PATH)/libavcodec \
					$(LOCAL_PATH)/libavutil \
					$(LOCAL_PATH)/../

LOCAL_STATIC_LIBRARIES := libavutil libavcodec

include $(BUILD_STATIC_LIBRARY)

