/*
 * VueceAACDecoder.h
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

#ifndef VUECEAACDECODER_H_
#define VUECEAACDECODER_H_


#include <libavcodec/avcodec.h>

#include "VueceMemQueue.h"

typedef struct _VueceAACDecData{
	AVCodecContext  *pCodecCtx;
	AVCodec * pCodec;
	int16_t 	*outbuf;
	int decoded_raw_pkt_size;
	int buf_count;
}VueceAACDecData;


class VueceAACDecoder
{
public:
	VueceAACDecoder();
	virtual ~VueceAACDecoder();

	bool Init(int sample_rate, int bit_rate, int channel_num);
	void Process(VueceMemQueue* in_q, VueceMemQueue* out_q);
	void Uninit();

private:
	void set_num_channels(int num);
	VueceAACDecData* dec_data;
};


#endif /* VUECEAACDECODER_H_ */
