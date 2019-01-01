#include "talk/base/logging.h"

#include "VueceDevVidUtils.h"
#include "TVideoSettings.h"

int VueceDevVidUtils::ReadNextNAL(int aPos, int aDataLen, uint8_t* aVidData, int * aCurrentPos) {
	int numOfZeros = 0;
	//    int aPos = aCurrentPos;
	bool aFound(false);
	int aNalType(-1);

	// Find next PSC
	while (aPos < aDataLen && !aFound) {
		if (aVidData[aPos] == 0x00) {
			numOfZeros++;
		} else if (aVidData[aPos] == 0x01 && numOfZeros == 3) {
			aFound = true;
		} else {
			numOfZeros = 0;
		}
		aPos++;
	}

	if (aFound) {
		//the value is extracted from the last 5 bits: 00011111

		aNalType = 0x1f & aVidData[aPos];

		LOG(LS_VERBOSE) << "VueceDevVidUtils:Found a NAL unit:Type: " << aNalType;
	}

	*aCurrentPos = aPos;

	return aNalType;
}

bool VueceDevVidUtils::IsNextFewBitsStartCode(uint8_t* data, int pos, int numBits) {
	if (numBits == 24) {
		if (data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x01) {
			return true;
		} else {
			return false;
		}
	}

	if (numBits == 32) {

		if (data[pos] == 0x00 && data[pos + 1] == 0x00 && data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
			return true;
		} else {
			return false;
		}

	}

	// this should not be reached!
	return false;
}

int VueceDevVidUtils::AnalyzeAnnexBBitStream(uint8_t* aVidData, int len, bool &hasCodeSlice, bool &hasSPS, bool &hasIDR,
		bool &hasPPS, bool &hasSEI) {

	int aBufLen = KH263MaxCodedSizeQCIF;
	bool aFound = false;
	int aPos = 0;
	bool aEndReached = false;

	aPos = 0;
	int aNalType = -1;
	int aPreNalPos = 0;

	LOG(LS_VERBOSE) << "AnalyzeAnnexBBitStream";

	while(aPos + 24 < len)
	{
			LOG(LS_VERBOSE) << "AnalyzeAnnexBBitStream:Reading next NAL unit...";

			while(!IsNextFewBitsStartCode(aVidData, aPos, 24)
					&& !IsNextFewBitsStartCode(aVidData, aPos, 32))
			{
				aPos++;

				if(aPos + 24 >= len)
				{
					aEndReached = true;
					break;
				}

				//leading_zero_8bits
				if(aVidData[aPos] != 0x00)
				{
//					LOG(LS_ERROR) << "This should be a leading zero byte! pos = " << aPos;
					//return -1;
				}


			}

			if(aEndReached)
			{
				break;
			}

			LOG(LS_VERBOSE) << "Found start code, pos = " << aPos;

			if(!IsNextFewBitsStartCode(aVidData, aPos, 24))
			{
				aPos++;
				//zero_byte
				if(aVidData[aPos] != 0x00)
				{
					LOG(LS_ERROR) << "This should be a leading zero_byte after non-24bit start code! pos = " << aPos;
					return -1;
				}
			}

			// next must be start_code_prefix_one_3bytes
			if(!IsNextFewBitsStartCode(aVidData, aPos, 24))
			{
				LOG(LS_ERROR) << "This MUST BE a start code! pos = " << aPos;
				return -1;
			}

			aPos+=3;

			aNalType = 0x1f & aVidData[aPos];

			LOG(LS_VERBOSE) << "Found a NAL unit:Type: " << aNalType << " at position: " << aPos;

			aPos++;


			switch (aNalType) {
			case ENAL_CodedSlice: {
				LOG(LS_VERBOSE) << "ENAL_CodeSlice";
				hasCodeSlice = true;
				aFound = true;
				break;
			}
			case ENAL_SPS: {
				LOG(LS_VERBOSE) << "ENAL_SPS:" << aPos;
				hasSPS = true;
				break;
			}
			case ENAL_IDR: {
				LOG(LS_VERBOSE) << "ENAL_IDR:" << aPos;
				hasIDR = true;
				aFound = true;
				break;
			}
			case ENAL_SEI: {
				LOG(LS_VERBOSE) << "ENAL_SEI: " << aPos;
				hasSEI = true;
				break;
			}
			case ENAL_PPS: {
				LOG(LS_VERBOSE) << "ENAL_PPS: " << aPos;
				hasPPS = true;
				break;
			}
			default: {
				LOG(LS_VERBOSE) << "NAL Type ignored, unknown NAL type: " << aNalType;
				break;
			}
			}

			LOG(LS_VERBOSE) << "Nal size: " << (aPos - aPreNalPos);
			aPreNalPos = aPos;
	}




	return -1;


}

int VueceDevVidUtils::GetCodedDateLength(uint8_t* aVidData, bool &hasCodeSlice, bool &hasSPS, bool &hasIDR,
		bool &hasPPS, bool &hasSEI) {

	int aBufLen = KH263MaxCodedSizeQCIF;

	bool aFound = false;
	int aPos = 0;

	//	if ((aBuf->iData.Length() > 0)) {
	if (true) {
		// starting bytes: 00 00 00 01
		//            aPos = KScLength; //skip first 3 byte (PSC)
		aPos = 0;

		int aNALType = -1; //ReadNextNAL(aPosPtr, aBufLen, aVidData);
		//read the NAL header type
		//            aPos++;

		//            int aNalType = 0x1f & aVidData[aPos];
		//            LOG(LS_VERBOSE) << "KNALType_SPS"));
		int aCurrentPos = aPos;

		while (aPos < aBufLen) {
			aNALType = ReadNextNAL(aPos, aBufLen, aVidData, &aCurrentPos);

			aPos = aCurrentPos;
			switch (aNALType) {
			case ENAL_CodedSlice: {
				//                        LOG(LS_VERBOSE) << "ENAL_CodeSlice: %d"), aCurrentPos);
				hasCodeSlice = true;
				aFound = true;
				break;
			}
			case ENAL_SPS: {
				LOG(LS_VERBOSE) << "ENAL_SPS:" << aCurrentPos;
				hasSPS = true;
				break;
			}
			case ENAL_IDR: {
				LOG(LS_VERBOSE) << "ENAL_IDR:" << aCurrentPos;
				hasIDR = true;
				aFound = true;
				break;
			}
			case ENAL_SEI: {
				LOG(LS_VERBOSE) << "ENAL_SEI: " << aCurrentPos;
				hasSEI = true;
				break;
			}
			case ENAL_PPS: {
				LOG(LS_VERBOSE) << "ENAL_PPS: " << aCurrentPos;
				hasPPS = true;
				break;
			}
			default: {
				LOG(LS_VERBOSE) << "NAL Type ignored, unknown NAL type: " << aNALType;
				break;
			}
			}

			if (aFound) {
				break;
			}
		}

		if (!aFound) {
			LOG(LS_VERBOSE) << "Slice or IDR Not Found!";
			return -1;
		}

	} else {
		//                iInputEnd = true;
		return -1;
	}

	return aPos;
}

int VueceDevVidUtils::getNALType(uint8_t header) {
	return (int) header & 0x1f; //00011111
}

int VueceDevVidUtils::getNALRefIdc(uint8_t header) {
	return (int) ((header >> 5) & 0x03); //01100000
}

int VueceDevVidUtils::getSliceHeader_FirstMBInSlice(uint8_t header) {
	return (int) ((header >> 6) & 0x03); //11000000
}
int VueceDevVidUtils::getSliceHeader_SliceType(uint8_t header) {
	return (int) ((header >> 4) & 0x03); //00110000
}
int VueceDevVidUtils::getSliceHeader_ParamSetId(uint8_t header) {
	return (int) ((header >> 2) & 0x03); //00001100
}

int VueceDevVidUtils::getSliceHeader_FrameNumber(uint8_t header) {
	return (int) header & 0x03; //00000011
}

bool VueceDevVidUtils::isFragmentationStart(uint8_t header) {
	// When set to one, the Start bit indicates the start of a fragmented
	// NAL unit.  When the following FU payload is not the start of a
	// fragmented NAL unit payload, the Start bit is set to zero.

	int v = header & 0x80; //10000000, i.e. 128 in decimal
	if (v == 128) {
		return true;
	}

	return false;
}

bool VueceDevVidUtils::isFragmentationEnd(uint8_t header) {
	// When set to one, the End bit indicates the end of a fragmented NAL
	// unit, i.e., the last byte of the payload is also the last byte of
	// the fragmented NAL unit.  When the following FU payload is not the
	// last fragment of a fragmented NAL unit, the End bit is set to zero.

	int v = header & 0x40; //1000000

	if (v == 64) {
		return true;
	}

	return false;
}

void VueceDevVidUtils::printNalType(int type) {
	switch (type) {
	case 0:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Unspecified (0)";
		break;
	case 1:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Coded slice";
		break;
	case 2:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Data partition A";
		break;
	case 3:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Data partition B";
		break;
	case 4:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Data partition C";
		break;
	case 5:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: IDR(Instantaneous Decoding Refresh) picture";
		break;
	case 6:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: SEI";
		break;
	case 7:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: SPS";
		break;
	case 8:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: PPS";
		break;
	case 9:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Access Unit Delimiter";
		break;
	case 10:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: End of Sequences";
		break;
	case 11:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: End of Stream";
		break;
	case 12:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Filter Data";
	default:
		LOG(LS_VERBOSE) << "------------ NAL Unit Type: Extended or Undefined: " << type;
		break;
	}
}
