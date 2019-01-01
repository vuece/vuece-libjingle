LOCAL_PATH:= $(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/ffmpeg/
#LOCAL_PATH:= $(call my-dir)/../../../ffmpeg
#LOCAL_PATH:= $(call my-dir)/../../../speex
#LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libavcodec

LOCAL_SRC_FILES = \
	libavcodec/allcodecs.c \
	libavcodec/aandcttab.c \
	libavcodec/arm/dsputil_arm.S.arm \
	libavcodec/arm/dsputil_armv6.S.arm \
	libavcodec/arm/dsputil_init_arm.c \
	libavcodec/arm/dsputil_init_armv5te.c \
	libavcodec/arm/dsputil_init_armv6.c \
	libavcodec/arm/dsputil_init_neon.c \
	libavcodec/arm/dsputil_init_vfp.c \
	libavcodec/arm/dsputil_neon.S.neon \
	libavcodec/arm/dsputil_vfp.S.neon \
	libavcodec/arm/fft_init_arm.c \
	libavcodec/arm/fft_neon.S.neon \
	libavcodec/arm/h264dsp_init_arm.c \
	libavcodec/arm/h264dsp_neon.S.neon \
	libavcodec/arm/h264idct_neon.S.neon \
	libavcodec/arm/h264pred_init_arm.c \
	libavcodec/arm/h264pred_neon.S.neon \
	libavcodec/arm/int_neon.S.neon \
	libavcodec/arm/jrevdct_arm.S \
	libavcodec/arm/mdct_neon.S.neon \
	libavcodec/arm/mpegvideo_arm.c \
	libavcodec/arm/mpegvideo_armv5te.c \
	libavcodec/arm/mpegvideo_armv5te_s.S \
	libavcodec/arm/mpegvideo_neon.S.neon \
	libavcodec/arm/simple_idct_arm.S \
	libavcodec/arm/simple_idct_armv5te.S \
	libavcodec/arm/simple_idct_armv6.S \
	libavcodec/arm/simple_idct_neon.S.neon \
	libavcodec/arm/fmtconvert_init_arm.c \
	libavcodec/arm/fmtconvert_neon.S \
	libavcodec/arm/fmtconvert_vfp.S \
	libavcodec/arm/h264cmc_neon.S \
	libavcodec/arm/sbrdsp_neon.S \
	libavcodec/arm/sbrdsp_init_arm.c \
	libavcodec/arm/aacpsdsp_init_arm.c \
	libavcodec/arm/aacpsdsp_neon.S \
	libavcodec/audioconvert.c \
	libavcodec/avpacket.c \
	libavcodec/bitstream.c \
	libavcodec/bitstream_filter.c \
	libavcodec/cabac.c \
	libavcodec/dsputil.c.arm \
	libavcodec/error_resilience.c \
	libavcodec/faandct.c \
	libavcodec/faanidct.c \
	libavcodec/flvdec.c \
	libavcodec/flvenc.c \
	libavcodec/fft.c \
	libavcodec/golomb.c \
	libavcodec/h263.c.arm \
	libavcodec/h263_parser.c \
	libavcodec/h263dec.c \
	libavcodec/h264.c \
	libavcodec/h264_parser.c \
	libavcodec/h264_cabac.c.arm \
	libavcodec/h264_cavlc.c.arm \
	libavcodec/h264_direct.c.arm \
	libavcodec/h264_loopfilter.c \
	libavcodec/h264_ps.c \
	libavcodec/h264_refs.c \
	libavcodec/h264_sei.c \
	libavcodec/h264dsp.c \
	libavcodec/h264idct.c \
	libavcodec/h264pred.c \
	libavcodec/imgconvert.c \
	libavcodec/intelh263dec.c \
	libavcodec/inverse.c \
	libavcodec/ituh263dec.c \
	libavcodec/ituh263enc.c \
	libavcodec/jfdctfst.c \
	libavcodec/jfdctint.c \
	libavcodec/jrevdct.c \
	libavcodec/mjpeg.c.arm \
	libavcodec/mjpegdec.c.arm \
	libavcodec/motion_est.c.arm \
	libavcodec/mpeg12data.c \
	libavcodec/mpeg4video.c.arm \
	libavcodec/mpeg4video_parser.c \
	libavcodec/mpeg4videodec.c.arm \
	libavcodec/mpeg4videoenc.c.arm \
	libavcodec/mpegvideo.c.arm \
	libavcodec/mpegvideo_enc.c.arm \
	libavcodec/opt.c \
	libavcodec/options.c \
	libavcodec/parser.c \
	libavcodec/ratecontrol.c \
	libavcodec/raw.c \
	libavcodec/resample.c \
	libavcodec/resample2.c \
	libavcodec/simple_idct.c \
	libavcodec/utils.c \
	libavcodec/pthread.c \
	libavcodec/svq3.c \
	libavcodec/raw.c \
	libavcodec/aactab.c \
	libavcodec/aacps.c \
	libavcodec/aacadtsdec.c \
	libavcodec/mdct.c \
	libavcodec/mpeg4audio.c \
	libavcodec/aac_parser.c \
	libavcodec/aac_ac3_parser.c \
	libavcodec/rawdec.c \
	libavcodec/aacsbr.c \
	libavcodec/mdct_tablegen.c \
	libavcodec/fmtconvert.c \
	libavcodec/kbdwin.c \
	libavcodec/sbrdsp.c \
	libavcodec/aacpsdsp.c \
	libavcodec/aac_adtstoasc_bsf.c \
	libavcodec/aac_ac3_parser.c \
	libavcodec/aac_tablegen.c \
	libavcodec/aaccoder.c \
	libavcodec/aacps_tablegen.c \
	libavcodec/aacps.c \
	libavcodec/aacpsdsp.c \
	libavcodec/aacpsy.c \
	libavcodec/aacenc.c \
	libavcodec/aacdec.c \
	libavcodec/latm_parser.c \
	libavcodec/psymodel.c \
	libavcodec/audio_frame_queue.c \
	libavcodec/iirfilter.c \
	libavcodec/chomp_bsf.c \
	libavcodec/dump_extradata_bsf.c \
	libavcodec/h264_mp4toannexb_bsf.c \
	libavcodec/imx_dump_header_bsf.c \
	libavcodec/mjpega_dump_header_bsf.c \
	libavcodec/mp3_header_compress_bsf.c \
	libavcodec/mp3_header_decompress_bsf.c \
	libavcodec/mpegaudiodata.c \
	libavcodec/fft_fixed.c \
	libavcodec/fft_float.c
	
LOCAL_ARM_MODE := arm

#LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H -Wa,-I$(LOCAL_PATH)/libavcodec/arm
LOCAL_CFLAGS += -DHAVE_AV_CONFIG_H

#NOTE: Compiling aacdec.c needs neon support
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_ARM_NEON  := true
LOCAL_CFLAGS += -DARCH_ARM -DHAVE_INT32_T
endif

#	$(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/faac-1.28/include \
#for including config.h:
LOCAL_C_INCLUDES += \
	$(VUECE_LIBJINGLE_EXTERNALS_LOCATION)/android/build/ffmpeg  \
	$(LOCAL_PATH)/libavcodec \
	$(LOCAL_PATH)/libavcodec/arm \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/libavutil 

#LOCAL_SHARED_LIBRARIES := libavutil libavcore
#include $(BUILD_SHARED_LIBRARY)

LOCAL_STATIC_LIBRARIES := libavutil libavcore
include $(BUILD_STATIC_LIBRARY)
