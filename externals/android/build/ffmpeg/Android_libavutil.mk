
LOCAL_PATH:= $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/ffmpeg/
include $(CLEAR_VARS)

LOCAL_MODULE := libavutil


LOCAL_SRC_FILES = \
	libavutil/adler32.c \
	libavutil/aes.c \
	libavutil/arm/cpu.c \
	libavutil/avstring.c \
	libavutil/base64.c \
	libavutil/cpu.c \
	libavutil/crc.c \
	libavutil/des.c \
	libavutil/error.c \
	libavutil/eval.c \
	libavutil/fifo.c \
	libavutil/intfloat_readwrite.c \
	libavutil/inverse.c \
	libavutil/lfg.c \
	libavutil/lls.c \
	libavutil/log.c \
	libavutil/lzo.c \
	libavutil/mathematics.c \
	libavutil/md5.c \
	libavutil/mem.c \
	libavutil/opt.c \
	libavutil/pixdesc.c \
	libavutil/random_seed.c \
	libavutil/rational.c \
	libavutil/rc4.c \
	libavutil/sha.c \
	libavutil/tree.c \
	libavutil/utils.c \
	libavutil/dict.c \
	libavutil/imgutils.c \
	libavutil/audioconvert.c \
	libavutil/samplefmt.c

LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H

LOCAL_ARM_MODE := arm

#for including config.h:
LOCAL_C_INCLUDES += $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/android/build/ffmpeg
include $(BUILD_STATIC_LIBRARY)

