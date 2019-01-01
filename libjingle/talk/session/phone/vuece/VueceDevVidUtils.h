#ifndef VUECEDEVVIDUTILS_H_
#define VUECEDEVVIDUTILS_H_

extern "C" {
#include "stdint.h"
}

class VueceDevVidUtils {
public:

	//    static int GetDataLengthL(const TDesC8& aMsg);
	static int GetCodedDateLength(uint8_t* aVidData, bool &hasCodeSlice,
			bool &hasSPS, bool &hasIDR, bool &hasPPS, bool &hasSEI);
	static int ReadNextNAL(int aStartPos, int aDataLen, uint8_t* aVidData,
			int * aCurrentPos);
	static int  getNALType(uint8_t header);
	static int  getNALRefIdc(uint8_t header);
	static int  getSliceHeader_FrameNumber(uint8_t header);
	static int  getSliceHeader_FirstMBInSlice(uint8_t header);
	static int  getSliceHeader_SliceType(uint8_t header);
	static int  getSliceHeader_ParamSetId(uint8_t header);

	static bool isFragmentationStart(uint8_t header);
    static bool isFragmentationEnd(uint8_t header);

    static void printNalType(int type);

    static int AnalyzeAnnexBBitStream(uint8_t* aVidData, int len, bool &hasCodeSlice,
    		bool &hasSPS, bool &hasIDR, bool &hasPPS, bool &hasSEI);

    static bool IsNextFewBitsStartCode(uint8_t* aVidData, int pos, int numBits);
};

#endif /* VUECEDEVVIDUTILS_H_ */
