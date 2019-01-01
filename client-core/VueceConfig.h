/*
 * VueceConfig.h
 *
 *  Created on: 2015-9-27
 *      Author: Jingjing Sun
 */

#ifndef VUECECONFIG_H_
#define VUECECONFIG_H_

/*
 * Number of frames per chunk file, one frame is usually 20ms
 */
#define VUECE_AUDIO_FRAMES_PER_CHUNK 1500

/*
 * Number of chunk files that will be downloaded in each stream session
 */
#define VUECE_BUFFER_WINDOW 10

//Trigger new download when 30 seconds data left
#define VUECE_BUFWIN_THRESHOLD_SEC 30

//This is used to filter away large file if its size is above predefined value
#define VUECE_MAX_MUSIC_FILE_SIZE_MB 			20

//This is used to filter away small files if the duration is below predefined value
#define VUECE_MIN_MUSIC_DURATION_SEC         5

#define VUECE_TIMEOUT_WAIT_SESSION_RELEASED 15

#define VUECE_SESSION_MGR_TIMEOUT  20

#define VUECE_UPDATE_SERVER_URL "www.vuece.com"
#define VUECE_UPDATE_SERVER_PORT 80
#define VUECE_UPDATE_VERSION_INFO_LOCATION "/webupdate.txt"

#endif /* VUECECONFIG_H_ */
