/*
 * TVideoSettings.h
 *
 *  Created on: Apr 10, 2010
 *      Author: Jingjing Sun
 */

#ifndef TVIDEOSETTINGS_H_
#define TVIDEOSETTINGS_H_

const int KVidSegmentSize = 1000;

/**
 * Min def. of Max byte size of a h263 coded picture in QCIF
 */
const int KH263MaxCodedSizeQCIF = 38016;

const int KEncodedBufferSize = KH263MaxCodedSizeQCIF * 5;
const int KPayloadType_H263 = 34;
const int KPayloadType_H264 = 97;
const int KInternalMsgHeaderLen = 10;
const int KVideoSamplingRate = 15;
const int KH263V2PayloadHeaderLen = 0;
//_LIT8(KMimeH263, "video/h263-2000; profile=0; level=10");
//_LIT8(KMimeH263, "video/h263-2000");
//_LIT8(KMimeH264, "video/H264; profile-level-id=42800A");
//_LIT8(KMimeH264, "video/H264");

const int KImageWidth = 176; // QCIF resolution
const int KImageHeigth = 144;

const int KImageNumPixels = KImageWidth * KImageHeigth;

const int KFrameSize = 38016;

const int KNAL_Unspecified = 0;
const int KNAL_CodeSlice = 1;
const int KNAL_DataPartitionA = 2;
const int KNAL_DataPartitionB = 3;
const int KNAL_DataPartitionC = 4;
const int KNAL_IDR = 5;
const int KNAL_SEI = 6;
const int KNAL_SPS = 7;
const int KNAL_PPS = 8;
const int KNAL_AcessUnitDelimiter = 9;
const int KNAL_EndOfSequnce = 10;
const int KNAL_EndOfStream = 11;
const int KNAL_FillerData = 12;
const int KNAL_FregmentUnitA = 28;
const int KNAL_FregmentUnitB = 29;

enum TVideoCodec
     {
     ENoCodec,
     EH263,
     EMPEG4,
     EH264
     };

enum TNALUnitType
{
    ENAL_Unspecified = 0,
    ENAL_CodedSlice,             //1
    ENAL_DataPartitionA,        //2
    ENAL_DataPartitionB,        //3
    ENAL_DataPartitionC,        //4
    ENAL_IDR,                   //5
    ENAL_SEI,                   //6
    ENAL_SPS,                   //7
    ENAL_PPS,                   //8
    ENAL_AcessUnitDelimiter,    //9
    ENAL_EndOfSequnce,          //10
    ENAL_EndOfStream,           //11
    ENAL_FillerData,             //12
    ENAL_FregmentUnitA  = 28
};


#endif /* TVIDEOSETTINGS_H_ */
