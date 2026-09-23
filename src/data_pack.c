/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/*
	Convert data from/to
	HDA buffers
*/

#include "data_pack.h"

volatile HdaIn_StreamTag_t g_InputStreamTags[4];

const p_sdo_pack_24_proc sdo_pack_24_procs[4] = 
{
	sdo_pack_24_1x,
	sdo_pack_24_2x,
	0,
	sdo_pack_24_4x	
};

//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void sdo_pack_24_1x(uint32_t verb32, uint32_t *dst, uint32_t *src)
{
	//                                                                                 //       собираем слова для DMA буфера:
	//                                                                                 //    +--------+--------+--------+--------+
	//                                                                                 //    |  MSB   |        |        |  LSB   |
	//  упаковка семпла (0) и глагола в слова (0..1)                                   //    +--------+--------+--------+--------+
	// dst[0] =  verb32 >> 8;                 // reserved 8 bits (zero) + 24 verb data    // 0  |    0   |  vrb3  |  vrb2  |  vrb1  |
	// dst[1] = (verb32 << 24);// verb remain                                             // 1  |  vrb0  |  S0-3  |  S0-2  |     0  |

	SDO_PACK_VERB32(verb32,dst);
	dst[1] |= (src[0] >> 8);//                              word[0]                   // 1  |   OR   |   OR   |   OR   |  S0-1  |

	// преобразуем оставшиеся 32-семплы в 24 формат
	int idx_i = 1;
	int idx_o = 2;

	// упавковка 4 семплов (1..4) в слова (2..4)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 2  |   S1-3 |   S1-2 |   S1-1 |   S2-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 3  |   S2-2 |   S2-1 |   S3-3 |   S3-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 4  |   S3-1 |   S4-3 |   S4-2 |   S4-1 |
	idx_i += 4;

	// упавковка 4 семплов (5..8) в слова (5..7)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 5  |   S5-3 |   S5-2 |   S5-1 |   S6-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 6  |   S6-2 |   S6-1 |   S7-3 |   S7-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 7  |   S7-1 |   S8-3 |   S8-2 |   S8-1 |
	idx_i += 4;

	// упаковка семпла (9)   в слово (8)
	dst[idx_o++] = ( src[idx_i + 0] & 0xffffff00);                                    // 8  |   S9-3 |   S9-2 |   S9-1 |    0   |

}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void sdo_pack_24_2x(uint32_t verb32, uint32_t *dst, uint32_t *src)
{
	//                                                                                 //       собираем слова для DMA буфера:
	//                                                                                 //    +--------+--------+--------+--------+
	//                                                                                 //    |  MSB   |        |        |  LSB   |
	//  упаковка семпла (0) и глагола в слова (0..1)                                   //    +--------+--------+--------+--------+
	// dst[0] =  verb32 >> 8;                 // reserved 8 bits (zero) + 24 verb data    // 0  |    0   |  vrb3  |  vrb2  |  vrb1  |
	// dst[1] = (verb32 << 24) | (src[0] >> 8);// verb remain + word[0]                   // 1  |  vrb0  |  S0-3  |  S0-2  |  S0-1  |

	SDO_PACK_VERB32(verb32,dst);
	dst[1] |= (src[0] >> 8);//                              word[0]                   // 1  |   OR   |   OR   |   OR   |  S0-1  |

	// преобразуем оставшиеся 32-семплы в 24 формат
	int idx_i = 1;
	int idx_o = 2;

	// упавковка 4 семплов (1..4) в слова (2..4)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 2  |   S1-3 |   S1-2 |   S1-1 |   S2-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 3  |   S2-2 |   S2-1 |   S3-3 |   S3-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 4  |   S3-1 |   S4-3 |   S4-2 |   S4-1 |
	idx_i += 4;

	// упавковка 4 семплов (5..8) в слова (5..7)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 5  |   S5-3 |   S5-2 |   S5-1 |   S6-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 6  |   S6-2 |   S6-1 |   S7-3 |   S7-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 7  |   S7-1 |   S8-3 |   S8-2 |   S8-1 |
	idx_i += 4;

	// упавковка 4 семплов (9..12) в слова (8..10)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 8  |   S9-3 |   S9-2 |   S9-1 |  S10-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 9  |  S10-2 |  S10-1 |  S11-3 |  S11-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 10 |  S11-1 |  S12-3 |  S12-2 |  S12-1 |
	idx_i += 4;

	// упавковка 4 семплов (13..16) в слова (11..13)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 11 |  S13-3 |  S13-2 |  S13-1 |  S14-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 12 |  S14-2 |  S14-1 |  S15-3 |  S15-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 13 |  S15-1 |  S16-3 |  S16-2 |  S16-1 |
	idx_i += 4;

	// упавковка 3 семплов (17..19) в слова (14..16)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 14 |  S17-3 |  S17-2 |  S17-1 |  S18-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 15 |  S18-2 |  S18-1 |  S19-3 |  S19-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24);                                     // 16 |  S19-1 |      0 |      0 |      0 |
	idx_i += 4;

}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void sdo_pack_24_4x(uint32_t verb32, uint32_t *dst, uint32_t *src)
{
	//                                                                                 //       собираем слова для DMA буфера:
	//                                                                                 //    +--------+--------+--------+--------+
	//                                                                                 //    |  MSB   |        |        |  LSB   |
	//  упаковка семпла (0) и глагола в слова (0..1)                                   //    +--------+--------+--------+--------+
	// dst[0] =  verb32 >> 8;                 // reserved 8 bits (zero) + 24 verb data    // 0  |    0   |  vrb3  |  vrb2  |  vrb1  |
	// dst[1] = (verb32 << 24) | (src[0] >> 8);// verb remain + word[0]                   // 1  |  vrb0  |  S0-3  |  S0-2  |  S0-1  |

	SDO_PACK_VERB32(verb32,dst);
	dst[1] |= (src[0] >> 8);//                              word[0]                   // 1  |   OR   |   OR   |   OR   |  S0-1  |

	// преобразуем оставшиеся 32-семплы в 24 формат
	int idx_i = 1;
	int idx_o = 2;

	// упавковка 4 семплов (1..4) в слова (2..4)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 2  |   S1-3 |   S1-2 |   S1-1 |   S2-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 3  |   S2-2 |   S2-1 |   S3-3 |   S3-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 4  |   S3-1 |   S4-3 |   S4-2 |   S4-1 |
	idx_i += 4;

	// упавковка 4 семплов (5..8) в слова (5..7)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 5  |   S5-3 |   S5-2 |   S5-1 |   S6-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 6  |   S6-2 |   S6-1 |   S7-3 |   S7-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 7  |   S7-1 |   S8-3 |   S8-2 |   S8-1 |
	idx_i += 4;

	// упавковка 4 семплов (9..12) в слова (8..10)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 8  |   S9-3 |   S9-2 |   S9-1 |  S10-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 9  |  S10-2 |  S10-1 |  S11-3 |  S11-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 10 |  S11-1 |  S12-3 |  S12-2 |  S12-1 |
	idx_i += 4;

	// упавковка 4 семплов (13..16) в слова (11..13)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 11 |  S13-3 |  S13-2 |  S13-1 |  S14-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 12 |  S14-2 |  S14-1 |  S15-3 |  S15-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 13 |  S15-1 |  S16-3 |  S16-2 |  S16-1 |
	idx_i += 4;

	// упавковка 4 семплов (17..20) в слова (14..16)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 14 |  S17-3 |  S17-2 |  S17-1 |  S18-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 15 |  S18-2 |  S18-1 |  S19-3 |  S19-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 16 |  S19-1 |  S20-3 |  S20-2 |  S20-1 |
	idx_i += 4;

	// упавковка 4 семплов (21..24) в слова (17..19)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 17 |  S21-3 |  S21-2 |  S21-1 |  S22-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 18 |  S22-2 |  S22-1 |  S23-3 |  S23-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 19 |  S23-1 |  S24-3 |  S24-2 |  S24-1 |
	idx_i += 4;

	// упавковка 4 семплов (25..28) в слова (20..22)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 20 |  S25-3 |  S25-2 |  S25-1 |  S26-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 21 |  S26-2 |  S26-1 |  S27-3 |  S27-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 22 |  S27-1 |  S28-3 |  S28-2 |  S28-1 |
	idx_i += 4;

	// упавковка 4 семплов (29..32) в слова (23..25)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 23 |  S29-3 |  S29-2 |  S29-1 |  S30-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 24 |  S30-2 |  S30-1 |  S31-3 |  S31-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 25 |  S31-1 |  S32-3 |  S32-2 |  S32-1 |
	idx_i += 4;

	// упавковка 4 семплов (33..36) в слова (26..29)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 26 |  S33-3 |  S33-2 |  S33-1 |  S34-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 27 |  S34-2 |  S34-1 |  S35-3 |  S35-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 28 |  S35-1 |  S36-3 |  S36-2 |  S36-1 |
	idx_i += 4;

	// упавковка 3 семплов (37..39) в слова (30..31)
	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 29 |  S37-3 |  S37-2 |  S37-1 |  S38-3 |
	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 30 |  S38-2 |  S38-1 |  S39-3 |  S39-2 |
	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)                             ;        // 31 |  S39-1 |      0 |      0 |      0 |
	//idx_i += 4;

}
//------------------------------------------------------------------------------------------------------------------------------------------------
// __attribute__((noinline, section(".scratch_x.rb32_functions")))
// void create_TX_buff(uint32_t verb32, uint32_t *dst, uint32_t *src)
// {
// 	//                                                                                 //       собираем слова для DMA буфера:
// 	//                                                                                 //    +--------+--------+--------+--------+
// 	//                                                                                 //    |  MSB   |        |        |  LSB   |
// 	//  упаковка семпла (0) и глагола в слова (0..1)                                   //    +--------+--------+--------+--------+
// 	dst[0] =  verb32 >> 8;                 // reserved 8 bits (zero) + 24 verb data    // 0  |    0   |  vrb3  |  vrb2  |  vrb1  |
// 	dst[1] = (verb32 << 24) | (src[0] >> 8);// verb remain + word[0]                   // 1  |  vrb0  |  S0-3  |  S0-2  |  S0-1  |
//
// 	// преобразуем оставшиеся 32-семплы в 24 формат
// 	int idx_i = 1;
// 	int idx_o = 2;
//
// 	// упавковка 4 семплов (1..4) в слова (2..4)
// 	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 2  |   S1-3 |   S1-2 |   S1-1 |   S2-3 |
// 	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 3  |   S2-2 |   S2-1 |   S3-3 |   S3-2 |
// 	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 4  |   S3-1 |   S4-3 |   S4-2 |   S4-1 |
// 	idx_i += 4;
//
// 	// упавковка 4 семплов (5..8) в слова (5..7)
// 	dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 5  |   S5-3 |   S5-2 |   S5-1 |   S6-3 |
// 	dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 6  |   S6-2 |   S6-1 |   S7-3 |   S7-2 |
// 	dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 7  |   S7-1 |   S8-3 |   S8-2 |   S8-1 |
// 	idx_i += 4;
//
//
// 	if (spk_mul == 1)
// 	{
// 		// упаковка семпла (9)   в слово (8)
// 		dst[idx_o++] = ( src[idx_i + 0] & 0xffffff00);                                   // 8  |   S9-3 |   S9-2 |   S9-1 |    0   |
// 	}
// 	else   //  (spk_mul != 1)
// 	{
// 		// упавковка 4 семплов (9..12) в слова (8..10)
// 		dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 8  |   S9-3 |   S9-2 |   S9-1 |  S10-3 |
// 		dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 9  |  S10-2 |  S10-1 |  S11-3 |  S11-2 |
// 		dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 10 |  S11-1 |  S12-3 |  S12-2 |  S12-1 |
// 		idx_i += 4;
//
// 		// упавковка 4 семплов (13..16) в слова (11..13)
// 		dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 11 |  S13-3 |  S13-2 |  S13-1 |  S14-3 |
// 		dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 12 |  S14-2 |  S14-1 |  S15-3 |  S15-2 |
// 		dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 13 |  S15-1 |  S16-3 |  S16-2 |  S16-1 |
// 		idx_i += 4;
//
// 		if (spk_mul == 2)
// 		{
//
// 			// упавковка 3 семплов (17..19) в слова (14..16)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 14 |  S17-3 |  S17-2 |  S17-1 |  S18-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 15 |  S18-2 |  S18-1 |  S19-3 |  S19-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24);                                    // 16 |  S19-1 |      0 |      0 |      0 |
// 			idx_i += 4;
// 		}
// 		else // (spk_mul != 2)
// 		{
// 			// упавковка 4 семплов (17..20) в слова (14..16)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 14 |  S17-3 |  S17-2 |  S17-1 |  S18-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 15 |  S18-2 |  S18-1 |  S19-3 |  S19-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 16 |  S19-1 |  S20-3 |  S20-2 |  S20-1 |
// 			idx_i += 4;
//
// 			// упавковка 4 семплов (21..24) в слова (17..19)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 17 |  S21-3 |  S21-2 |  S21-1 |  S22-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 18 |  S22-2 |  S22-1 |  S23-3 |  S23-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 19 |  S23-1 |  S24-3 |  S24-2 |  S24-1 |
// 			idx_i += 4;
//
// 			// упавковка 4 семплов (25..28) в слова (20..22)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 20 |  S25-3 |  S25-2 |  S25-1 |  S26-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 21 |  S26-2 |  S26-1 |  S27-3 |  S27-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 22 |  S27-1 |  S28-3 |  S28-2 |  S28-1 |
// 			idx_i += 4;
//
// 			// упавковка 4 семплов (29..32) в слова (23..25)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 23 |  S29-3 |  S29-2 |  S29-1 |  S30-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 24 |  S30-2 |  S30-1 |  S31-3 |  S31-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 25 |  S31-1 |  S32-3 |  S32-2 |  S32-1 |
// 			idx_i += 4;
//
// 			// упавковка 4 семплов (33..36) в слова (26..29)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 26 |  S33-3 |  S33-2 |  S33-1 |  S34-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 27 |  S34-2 |  S34-1 |  S35-3 |  S35-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)      | (src[idx_i + 3] >>  8);       // 28 |  S35-1 |  S36-3 |  S36-2 |  S36-1 |
// 			idx_i += 4;
//
// 			// упавковка 3 семплов (37..39) в слова (30..31)
// 			dst[idx_o++] = ((src[idx_i + 0] >> 8) <<  8)      | (src[idx_i + 1] >> 24);       // 29 |  S37-3 |  S37-2 |  S37-1 |  S38-3 |
// 			dst[idx_o++] = ((src[idx_i + 1] >> 8) << 16)      | (src[idx_i + 2] >> 16);       // 30 |  S38-2 |  S38-1 |  S39-3 |  S39-2 |
// 			dst[idx_o++] = ((src[idx_i + 2] >> 8) << 24)                             ;       // 31 |  S39-1 |      0 |      0 |      0 |
// 			//idx_i += 4;
//
// 		} //(spk_mul == 2)
//
// 	}  //  (spk_mul == 1)
// }
//------------------------------------------------------------------------------------------------------------------------------------------------
/*
	bit32_msb_aleft
	bit32_msb_arigth

	Из массива uint32_t src[] выдёргиваем нужное количество (width) бит,
	начиная с номера bit.
	Данные в битовом массиве находятся в формате MSB.
	Результат выровнен влево (bit32_msb_aleft)
	или вправо (bit32_msb_arigth)
*/


uint32_t bit32_msb_aleft(uint32_t *src, int bit, int width)
{
	int msb  = bit & 0x1f;
	int idx0 = bit >> 5;
	int w32m = 32 - width;
	if(msb + width > 32)
	{
		int idx1 = idx0 + 1;
		return (src[idx0] << msb) | ((src[idx1] >> (32 + w32m - msb)) << w32m);
	}
	else
	{
		return   (src[idx0] >> (w32m - msb)) << (w32m);
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t bit32_msb_arigth(uint32_t *src, int bit, int width)
{
	int msb  = bit & 0x1f;
	int idx0 = bit >> 5;
	int w32m = 32 - width;

	if(msb + width > 32)
	{
		int idx1 = idx0 + 1;
		return ((src[idx0] << msb) >> (w32m))
		     | (src[idx1] >> (32 + w32m - msb));
	}
	else
	{
		return   ((src[idx0] << msb)  >> (w32m));
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_16bit_4x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;
	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 16) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 8;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_1((src + 1), 14, 16);
	dst[1] = GET_BITS_ALEFT_2((src + 1), 30, 16);
	dst[2] = GET_BITS_ALEFT_1((src + 2), 14, 16);
	dst[3] = GET_BITS_ALEFT_2((src + 2), 30, 16);
	dst[4] = GET_BITS_ALEFT_1((src + 3), 14, 16);
	dst[5] = GET_BITS_ALEFT_2((src + 3), 30, 16);
	dst[6] = GET_BITS_ALEFT_1((src + 4), 14, 16);
	dst[7] = GET_BITS_ALEFT_2((src + 4), 30, 16);

	stream_ID = GET_BITS_ARIGHT_1((src + 5), 14, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 5), 18, 6);
	if(bytes_len != 16) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 8;
	g_InputStreamTags[1].offset      = 8;

	dst[8] = GET_BITS_ALEFT_2((src + 5), 24, 16);
	dst[9] = GET_BITS_ALEFT_1((src + 6), 8, 16);
	dst[10] = GET_BITS_ALEFT_2((src + 6), 24, 16);
	dst[11] = GET_BITS_ALEFT_1((src + 7), 8, 16);
	dst[12] = GET_BITS_ALEFT_2((src + 7), 24, 16);
	dst[13] = GET_BITS_ALEFT_1((src + 8), 8, 16);
	dst[14] = GET_BITS_ALEFT_2((src + 8), 24, 16);
	dst[15] = GET_BITS_ALEFT_1((src + 9), 8, 16);

	stream_ID = GET_BITS_ARIGHT_1((src + 9), 24, 4);
	if(!stream_ID) return 2;
	bytes_len = GET_BITS_ARIGHT_2((src + 9), 28, 6);
	if(bytes_len != 16) return 2;

	g_InputStreamTags[2].stream_id   = stream_ID;
	g_InputStreamTags[2].samples_num = 8;
	g_InputStreamTags[2].offset      = 16;

	dst[16] = GET_BITS_ALEFT_1((src + 10), 2, 16);
	dst[17] = GET_BITS_ALEFT_2((src + 10), 18, 16);
	dst[18] = GET_BITS_ALEFT_1((src + 11), 2, 16);
	dst[19] = GET_BITS_ALEFT_2((src + 11), 18, 16);
	dst[20] = GET_BITS_ALEFT_1((src + 12), 2, 16);
	dst[21] = GET_BITS_ALEFT_2((src + 12), 18, 16);
	dst[22] = GET_BITS_ALEFT_1((src + 13), 2, 16);
	dst[23] = GET_BITS_ALEFT_2((src + 13), 18, 16);
	return 3;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_16bit_2x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;

	// stream 0
	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 8) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 4;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_1((src + 1), 14, 16);
	dst[1] = GET_BITS_ALEFT_2((src + 1), 30, 16);
	dst[2] = GET_BITS_ALEFT_1((src + 2), 14, 16);
	dst[3] = GET_BITS_ALEFT_2((src + 2), 30, 16);

	// stream 1
	stream_ID = GET_BITS_ARIGHT_1((src + 3), 14, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 3), 18, 6);
	if(bytes_len != 8) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 4;
	g_InputStreamTags[1].offset      = 4;

	dst[4] = GET_BITS_ALEFT_2((src + 3), 24, 16);
	dst[5] = GET_BITS_ALEFT_1((src + 4), 8, 16);
	dst[6] = GET_BITS_ALEFT_2((src + 4), 24, 16);
	dst[7] = GET_BITS_ALEFT_1((src + 5), 8, 16);

	// stream 2
	stream_ID = GET_BITS_ARIGHT_1((src + 5), 24, 4);
	if(!stream_ID) return 2;
	bytes_len = GET_BITS_ARIGHT_2((src + 5), 28, 6);
	if(bytes_len != 8) return 2;

	g_InputStreamTags[2].stream_id   = stream_ID;
	g_InputStreamTags[2].samples_num = 4;
	g_InputStreamTags[2].offset      = 8;

	dst[8] = GET_BITS_ALEFT_1((src + 6), 2, 16);
	dst[9] = GET_BITS_ALEFT_2((src + 6), 18, 16);
	dst[10] = GET_BITS_ALEFT_1((src + 7), 2, 16);
	dst[11] = GET_BITS_ALEFT_2((src + 7), 18, 16);
	return 3;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_16bit_1x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;
	// stream 0
	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 4) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 2;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_1((src + 1), 14, 16);
	dst[1] = GET_BITS_ALEFT_2((src + 1), 30, 16);
	// stream 1
	stream_ID = GET_BITS_ARIGHT_1((src + 2), 14, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 2), 18, 6);
	if(bytes_len != 4) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 2;
	g_InputStreamTags[1].offset      = 2;

	dst[2] = GET_BITS_ALEFT_2((src + 2), 24, 16);
	dst[3] = GET_BITS_ALEFT_1((src + 3), 8, 16);
	// stream 2
	stream_ID = GET_BITS_ARIGHT_1((src + 3), 24, 4);
	if(!stream_ID) return 2;
	bytes_len = GET_BITS_ARIGHT_2((src + 3), 28, 6);
	if(bytes_len != 4) return 2;

	g_InputStreamTags[2].stream_id   = stream_ID;
	g_InputStreamTags[2].samples_num = 2;
	g_InputStreamTags[2].offset      = 4;

	dst[4] = GET_BITS_ALEFT_1((src + 4), 2, 16);
	dst[5] = GET_BITS_ALEFT_2((src + 4), 18, 16);
	return 3;

}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_24bit_4x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;
	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 24) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 8;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_2((src + 1), 14, 24);
	dst[1] = GET_BITS_ALEFT_1((src + 2),  6, 24);
	dst[2] = GET_BITS_ALEFT_2((src + 2), 30, 24);
	dst[3] = GET_BITS_ALEFT_2((src + 3), 22, 24);
	dst[4] = GET_BITS_ALEFT_2((src + 4), 14, 24);
	dst[5] = GET_BITS_ALEFT_1((src + 5),  6, 24);
	dst[6] = GET_BITS_ALEFT_2((src + 5), 30, 24);
	dst[7] = GET_BITS_ALEFT_2((src + 6), 22, 24);

	stream_ID = GET_BITS_ARIGHT_1((src + 7), 14, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 7), 18, 6);
	if(bytes_len != 24) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 8;
	g_InputStreamTags[1].offset      = 8;

	dst[8]  = GET_BITS_ALEFT_2((src + 7),  24, 24);
	dst[9]  = GET_BITS_ALEFT_2((src + 8),  16, 24);
	dst[10] = GET_BITS_ALEFT_1((src + 9),   8, 24);
	dst[11] = GET_BITS_ALEFT_1((src + 10),  0, 24);
	dst[12] = GET_BITS_ALEFT_2((src + 10), 24, 24);
	dst[13] = GET_BITS_ALEFT_2((src + 11), 16, 24);
	dst[14] = GET_BITS_ALEFT_1((src + 12),  8, 24);
	dst[15] = GET_BITS_ALEFT_1((src + 13),  0, 24);
	return 2;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_24bit_2x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;
	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 12) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 4;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_2((src + 1), 14, 24);
	dst[1] = GET_BITS_ALEFT_1((src + 2), 6, 24);
	dst[2] = GET_BITS_ALEFT_2((src + 2), 30, 24);
	dst[3] = GET_BITS_ALEFT_2((src + 3), 22, 24);

	stream_ID = GET_BITS_ARIGHT_1((src + 4), 14, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 4), 18, 6);
	if(bytes_len != 12) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 4;
	g_InputStreamTags[1].offset      = 4;

	dst[4] = GET_BITS_ALEFT_2((src + 4), 24, 24);
	dst[5] = GET_BITS_ALEFT_2((src + 5), 16, 24);
	dst[6] = GET_BITS_ALEFT_1((src + 6), 8, 24);
	dst[7] = GET_BITS_ALEFT_1((src + 7), 0, 24);

	stream_ID = GET_BITS_ARIGHT_1((src + 7), 24, 4);
	if(!stream_ID) return 2;
	bytes_len = GET_BITS_ARIGHT_2((src + 7), 28, 6);
	if(bytes_len != 12) return 2;

	g_InputStreamTags[2].stream_id   = stream_ID;
	g_InputStreamTags[2].samples_num = 4;
	g_InputStreamTags[2].offset      = 8;

	dst[8] = GET_BITS_ALEFT_1((src + 8), 2, 24);
	dst[9] = GET_BITS_ALEFT_2((src + 8), 26, 24);
	dst[10] = GET_BITS_ALEFT_2((src + 9), 18, 24);
	dst[11] = GET_BITS_ALEFT_2((src + 10), 10, 24);
	return 3;

}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".time_critical.rb32_functions")))
int sdi_extract_24bit_1x(int32_t *dst, uint32_t *src)
{
	int stream_ID, bytes_len;

	stream_ID = GET_BITS_ARIGHT_1((src + 1), 4, 4);
	if(!stream_ID) return HDABUF_NO_INPUT_STRM;
	bytes_len = GET_BITS_ARIGHT_1((src + 1), 8, 6);
	if(bytes_len != 6) return HDABUF_WRONG_INP_LEN;

	g_InputStreamTags[0].stream_id   = stream_ID;
	g_InputStreamTags[0].samples_num = 2;
	g_InputStreamTags[0].offset      = 0;

	dst[0] = GET_BITS_ALEFT_2((src + 1), 14, 24);
	dst[1] = GET_BITS_ALEFT_1((src + 2), 6, 24);
	stream_ID = GET_BITS_ARIGHT_2((src + 2), 30, 4);
	if(!stream_ID) return 1;
	bytes_len = GET_BITS_ARIGHT_1((src + 3), 2, 6);
	if(bytes_len != 6) return 1;

	g_InputStreamTags[1].stream_id   = stream_ID;
	g_InputStreamTags[1].samples_num = 2;
	g_InputStreamTags[1].offset      = 2;

	dst[2] = GET_BITS_ALEFT_1((src + 3), 8, 24);
	dst[3] = GET_BITS_ALEFT_1((src + 4), 0, 24);

	stream_ID = GET_BITS_ARIGHT_1((src + 4), 24, 4);
	if(!stream_ID) return 2;
	bytes_len = GET_BITS_ARIGHT_2((src + 4), 28, 6);
	if(bytes_len != 6) return 2;

	g_InputStreamTags[2].stream_id   = stream_ID;
	g_InputStreamTags[2].samples_num = 2;
	g_InputStreamTags[2].offset      = 4;

	dst[4] = GET_BITS_ALEFT_1((src + 5), 2, 24);
	dst[5] = GET_BITS_ALEFT_2((src + 5), 26, 24);
	return 3;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void pack_32_to_24(uint32_t *dst, const uint32_t *src)
{
	// упавковка 4 семплов в 3 слова
	dst[0] = ((src[0] >> 8))  | ((src[1] >> 8)  << 24);    // | S1-1 | S0-3 | S0-2 | S0-1 |
	dst[1] = ((src[1] >> 16)) | ((src[2] >> 8)  << 16);    // | S2-2 | S2-1 | S1-3 | S1-2 |
	dst[2] = ((src[2] >> 24)) | ((src[3] >> 8)  <<  8);    // | S3-3 | S3-2 | S3-1 | S2-3 |
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void unpack_24_to_32_lsb(uint32_t *dst, const uint8_t *src)
{
	// распаковка 6 байт в 2 слова
	dst[0] = (src[0] << 24) | (src[1] << 16) | (src[2] << 8);     // | S0 | S1 | S2 | 0 |
	dst[1] = (src[3] << 24) | (src[4] << 16) | (src[5] << 8);     // | S3 | S4 | S5 | 0 |
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void unpack_16_to_32_lsb(uint32_t *dst, const uint8_t *src)
{
	// распаковка 4 байт в 2 слова
	dst[0] = (src[0] << 24) | (src[1] << 16);                      // | S0 | S1 | 0 | 0 |
	dst[1] = (src[2] << 24) | (src[3] << 16);                      // | S2 | S3 | 0 | 0 |
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void unpack_24_to_32_msb(uint32_t *dst, const uint8_t *src)
{
	// распаковка 6 байт в 2 слова
	dst[0] = (src[2] << 24) | (src[1] << 16) | (src[0] << 8);     // | S0 | S1 | S2 | 0 |
	dst[1] = (src[5] << 24) | (src[4] << 16) | (src[3] << 8);     // | S3 | S4 | S5 | 0 |
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void unpack_16_to_32_msb(uint32_t *dst, const uint8_t *src)
{
	// распаковка 4 байт в 2 слова
	dst[0] = (src[1] << 24) | (src[0] << 16);                      // | S0 | S1 | 0 | 0 |
	dst[1] = (src[3] << 24) | (src[2] << 16);                      // | S2 | S3 | 0 | 0 |
}
//------------------------------------------------------------------------------------------------------------------------------------------------
//void pack_32_to_24_s0(uint32_t *dst, const uint32_t *src)
//{
//	// упавковка 2 семплов в 1.5 слова со сдвигом на 0 байт
//	dst[0] = ((src[0] >> 8))  | ((src[1] >> 8)  << 24);    // | S1-1 | S0-3 | S0-2 | S0-1 |
//	dst[1] = ((src[1] >> 16));                             // |  0   |  0   | S1-3 | S1-2 |
//}
////------------------------------------------------------------------------------------------------------------------------------------------------
//void pack_32_to_24_s16(uint32_t *dst, const uint32_t *src)
//{
//	// упавковка 2 семплов в 1.5 слова со сдвигом на 2 байта
//	dst[1] |=                    ((src[0] >> 8)  << 16);    // | S0-2 | S0-1 |  or  |  or  |
//	dst[2]  = ((src[0] >> 24)) | ((src[1] >> 8)  <<  8);    // | S1-3 | S1-2 | S1-1 | S0-3 |
//}
//------------------------------------------------------------------------------------------------------------------------------------------------
