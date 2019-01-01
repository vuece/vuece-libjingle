/*
 * vuecevideosize.h
 *
 *  Created on: Jun 3, 2012
 *      Author: Vuece
 */

#ifndef VUECEVIDEOSIZE_H_
#define VUECEVIDEOSIZE_H_

//Regarding call signaling: http://code.google.com/apis/talk/call_signaling.html#Video
/**
 * Another note is that the Google Talk client may change the resolution
 * it sends during the call. If there is not enough bandwidth to maintain
 *  sufficient quality (QP), the client may reduce resolution from
 *  640x400 -> 480x300 -> 320x200 -> 240x150 -> 160x100, as needed.
 */
#define MS_VIDEO_SIZE_QCIF_GTALK1_W 160
#define MS_VIDEO_SIZE_QCIF_GTALK1_H 114

#define MS_VIDEO_SIZE_QCIF_GTALK2_W 320
#define MS_VIDEO_SIZE_QCIF_GTALK2_H 200

#define MS_VIDEO_SIZE_QCIF_GTALK3_W 640
#define MS_VIDEO_SIZE_QCIF_GTALK3_H 400

//the size used by vtok
#define MS_VIDEO_SIZE_QCIF_GTALK4_W 180
#define MS_VIDEO_SIZE_QCIF_GTALK4_H 240

#define MS_VIDEO_SIZE_QCIF_GTALK5_W 240
#define MS_VIDEO_SIZE_QCIF_GTALK5_H 150

#define MS_VIDEO_SIZE_QCIF_GTALK6_W 240
#define MS_VIDEO_SIZE_QCIF_GTALK6_H 180

#define MS_VIDEO_SIZE_QCIF_GTALK7_W 160
#define MS_VIDEO_SIZE_QCIF_GTALK7_H 100

#define MS_VIDEO_SIZE_QCIF_GTALK8_W 480
#define MS_VIDEO_SIZE_QCIF_GTALK8_H 300

#define MS_VIDEO_SIZE_QVGA_W 320
#define MS_VIDEO_SIZE_QVGA_H 240

#define MS_VIDEO_SIZE_QCIF_GTALK1 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK1_W,MS_VIDEO_SIZE_QCIF_GTALK1_H}
#define MS_VIDEO_SIZE_QCIF_GTALK2 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK2_W,MS_VIDEO_SIZE_QCIF_GTALK2_H}
#define MS_VIDEO_SIZE_QCIF_GTALK3 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK3_W,MS_VIDEO_SIZE_QCIF_GTALK3_H}
#define MS_VIDEO_SIZE_QCIF_GTALK4 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK4_W,MS_VIDEO_SIZE_QCIF_GTALK4_H}
#define MS_VIDEO_SIZE_QCIF_GTALK5 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK5_W,MS_VIDEO_SIZE_QCIF_GTALK5_H}
#define MS_VIDEO_SIZE_QCIF_GTALK6 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK6_W,MS_VIDEO_SIZE_QCIF_GTALK6_H}
#define MS_VIDEO_SIZE_QCIF_GTALK7 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK7_W,MS_VIDEO_SIZE_QCIF_GTALK7_H}
#define MS_VIDEO_SIZE_QCIF_GTALK8 (MSVideoSize){MS_VIDEO_SIZE_QCIF_GTALK8_W,MS_VIDEO_SIZE_QCIF_GTALK8_H}



#endif /* VUECEVIDEOSIZE_H_ */
