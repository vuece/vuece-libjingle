/*
 * VueceAACDecoder.cc
 *
 *  Created on: Nov 1, 2014
 *      Author: jingjing
 */

//Note - Must use extern "C"  here otherwise you will get 'undefined reference' errors
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"
}


#include "VueceLogger.h"
#include "VueceConstants.h"

#include "VueceAACDecoder.h"

VueceAACDecoder::VueceAACDecoder()
{
	VueceLogger::Debug("VueceAACDecoder - Constructor called");

	dec_data = NULL;
}


VueceAACDecoder::~VueceAACDecoder()
{
	VueceLogger::Debug("VueceAACDecoder - Destructor called");
}

void VueceAACDecoder::Uninit()
{
	VueceLogger::Debug("VUECE AAC DECODER - Uninit");

	if(dec_data != NULL)
	{
		av_free(dec_data->outbuf);
		avcodec_close(dec_data->pCodecCtx);

		free(dec_data);

		dec_data = NULL;
	}

	VueceLogger::Debug("VUECE AAC DECODER - Uninit Done");
}


bool VueceAACDecoder::Init(int sample_rate, int bit_rate, int channel_num)
{

	VueceLogger::Debug("VueceAACDecoder - Init called, sample_rate = %d, bit_rate = %d, channel_num = %d",
			sample_rate, bit_rate, channel_num);

	dec_data = (VueceAACDecData*)malloc(sizeof(VueceAACDecData));
	dec_data->pCodecCtx = NULL;
	dec_data->pCodec = NULL;
	dec_data->outbuf = NULL;
	dec_data->decoded_raw_pkt_size = 0;
	dec_data->buf_count = 0;

	VueceAACDecData* d = dec_data;

	d->outbuf =(int16_t*)av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	// Register all formats and codecs
	av_register_all();

	d->pCodec = avcodec_find_decoder(CODEC_ID_AAC);

	if(d->pCodec  == NULL)
	{
		VueceLogger::Fatal("VUECE AAC DECODER - Decoder not found.");
		return false;
	}

	VueceLogger::Debug("VUECE AAC DECODER - Init: Decoder located.");

	d->pCodecCtx = avcodec_alloc_context3(d->pCodec);

	if(d->pCodecCtx != NULL)
	{
		VueceLogger::Debug("VUECE AAC DECODER - Init: Decoder context allocated.");
	}
	else
	{
		//TODO: May be we need to do this in a gracefull way
		VueceLogger::Fatal("VUECE AAC DECODER - Init: Decoder context allocation failed! Abort.");
		return false;
	}

	d->pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;

	//set default values, they will be updated in later SET methods
	d->pCodecCtx->sample_rate = 44100;
	d->pCodecCtx->channels = 2;
	d->decoded_raw_pkt_size = 4096;

	//this is hard-coded for now
	d->pCodecCtx->bit_rate = 1411200;//64000;//1411200;//1411200;//100035;
	d->pCodecCtx->profile = FF_PROFILE_AAC_MAIN;
	d->pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	d->pCodecCtx->frame_size = 1024;

	//customize settings based on actual input
	d->pCodecCtx->sample_rate = sample_rate;
	d->pCodecCtx->bit_rate = bit_rate;
	set_num_channels(channel_num);

	VueceLogger::Debug("VUECE AAC DECODER - Opening codec, sample_rate: %d, channels: %d, bit_rate: %d",
			d->pCodecCtx->sample_rate, d->pCodecCtx->channels, d->pCodecCtx->bit_rate);

	if(avcodec_open(d->pCodecCtx, d->pCodec)<0)
	{
		VueceLogger::Fatal("VUECE AAC DECODER - Init:Cannot open AAC codec!");
		return false;
	}

	VueceLogger::Debug("VUECE AAC DECODER - Init: Decoder successfully opened, init OK");

	return true;

}

void VueceAACDecoder::set_num_channels(int channels){

	VueceAACDecData* d = dec_data;

	d->pCodecCtx->channels = channels;

	VueceLogger::Debug("VUECE AAC DECODER - set_num_channels: %d", d->pCodecCtx->channels);

	if(channels == 1)
	{
		d->decoded_raw_pkt_size = 2048;
	}
	else if(channels == 2)
	{
		d->decoded_raw_pkt_size = 4096;
	}
	else
	{
		VueceLogger::Fatal("VUECE AAC DECODER - set_num_channels: wrong channel number.");

		return;
	}

	VueceLogger::Debug("VUECE AAC DECODER - set_num_channels, decoded_raw_pkt_size is updated to: %d", d->decoded_raw_pkt_size);

	return;
}


void VueceAACDecoder::Process(VueceMemQueue* in_q, VueceMemQueue* out_q)
{

	VueceMemBulk 	*im,*om;
	int 	nbytes;
	int 	resultSize, decLen;

	VueceAACDecData *d = dec_data;

//	VueceLogger::Debug("VUECE AAC DECODER - Process: Input queue bulk count: %d", in_q->BulkCount());

	if(in_q->IsEmpty())
	{
//		VueceLogger::Debug("VUECE AAC DECODER - Process - Input queue is empty, do nothing and return");
		return;
	}

	VueceLogger::Debug("VUECE AAC DECODER - Process: Input queue bulk count: %d", in_q->BulkCount());

	while ( (im = in_q->Remove()) != NULL)
	{
		AVPacket pkt;

		nbytes = im->size_orginal;

		if (nbytes <= 0)
		{
			VueceLogger::Fatal("VUECE AAC DECODER - Process - Got a empty iput bulk, sth is wrong");
			return;
		}

		om = VueceMemQueue::AllocMemBulk(d->decoded_raw_pkt_size);

//		VueceLogger::Debug("VUECE AAC DECODER - processing data, size = %d", nbytes);

		av_init_packet(&pkt);
		pkt.data = (uint8_t *)im->data;
		pkt.size = nbytes;

		resultSize = d->decoded_raw_pkt_size;

		decLen = avcodec_decode_audio3(d->pCodecCtx, (int16_t *)om->data, &resultSize, &pkt);

		if(decLen <= 0)
		{
			VueceMemQueue::FreeMemBulk(om);
			VueceLogger::Fatal("VUECE AAC DECODER - avcodec_decode_audio3 returned a negative value: %d", decLen);
			return;
		}

		om->size_orginal = resultSize;
		om->end += resultSize;

//		VueceLogger::Debug("VUECE AAC DECODER -  Number of bytes decompressed: %d, result data size: %d ", decLen, resultSize);

		out_q->Put(om);

		d->buf_count++;

		VueceMemQueue::FreeMemBulk(im);
	}
}



