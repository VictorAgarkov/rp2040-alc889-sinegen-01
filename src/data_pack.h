/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _DATA_PACK_H_INCLUDED
	#define _DATA_PACK_H_INCLUDED

	//#include "hda.h"
	#include "stdint.h"

	#define GET_BITS_ARIGHT_1(src,msb,width)   ((src[0] << msb) >> (32 - width))
	#define GET_BITS_ARIGHT_2(src,msb,width)  (((src[0] << msb) >> (32 - width)) | (src[1] >> (64 - width - msb)))

	#define GET_BITS_ALEFT_1(src,msb,width)   ((src[0] >> (32 - width - msb)) << (32 - width))
	#define GET_BITS_ALEFT_2(src,msb,width)   ((src[0] << msb) | ((src[1] >> (64 - width - msb)) << (32 - width)))

	#define HDABUF_WRONG_SPF     0x80000000   // samples per frame not 1, 2 or 4
	#define HDABUF_NO_INPUT_STRM 0x40000000   // first input stream in SDI data is 0
	#define HDABUF_WRONG_INP_LEN 0x20000000   // number of bytes in one stream different then expected

	#define SDO_PACK_VERB32(verb32,dst) do { dst[0] = verb32 >> 8; dst[1] = (verb32 << 24); } while(0)


	typedef union
	{
		uint16_t v16;
		struct
		{
			uint8_t samples_num ;
			uint8_t offset ;
			uint8_t stream_id   ;
		};
	}HdaIn_StreamTag_t;



	typedef void (*p_sdo_pack_24_proc)(uint32_t verb32, uint32_t *dst, uint32_t *src);

	void sdo_pack_24_1x(uint32_t verb32, uint32_t *dst, uint32_t *src);
	void sdo_pack_24_2x(uint32_t verb32, uint32_t *dst, uint32_t *src);
	void sdo_pack_24_4x(uint32_t verb32, uint32_t *dst, uint32_t *src);



	uint32_t bit32_msb_aleft(uint32_t *src, int bit, int width);
	uint32_t bit32_msb_arigth(uint32_t *src, int bit, int width);
	int      sdi_extract_16bit_4x(int32_t *dst, uint32_t *src);
	int      sdi_extract_16bit_2x(int32_t *dst, uint32_t *src);
	int      sdi_extract_16bit_1x(int32_t *dst, uint32_t *src);
	int      sdi_extract_24bit_4x(int32_t *dst, uint32_t *src);
	int      sdi_extract_24bit_2x(int32_t *dst, uint32_t *src);
	int      sdi_extract_24bit_1x(int32_t *dst, uint32_t *src);

	void     pack_32_to_24(uint32_t *dst, const uint32_t *src);
	void     unpack_24_to_32_lsb(uint32_t *dst, const uint8_t *src);
	void     unpack_16_to_32_lsb(uint32_t *dst, const uint8_t *src);
	void     unpack_24_to_32_msb(uint32_t *dst, const uint8_t *src);
	void     unpack_16_to_32_msb(uint32_t *dst, const uint8_t *src);
	// void     pack_32_to_24_s0 (uint32_t *dst, const uint32_t *src)
	// void     pack_32_to_24_s16(uint32_t *dst, const uint32_t *src)

	extern volatile HdaIn_StreamTag_t g_InputStreamTags[4];
	extern const    p_sdo_pack_24_proc sdo_pack_24_procs[4];

#endif //_DATA_PACK_H_INCLUDED
