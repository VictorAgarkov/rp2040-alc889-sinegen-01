/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CODEC_H_INCLUDED
#define CODEC_H_INCLUDED

	#include <stdint.h>
	#include "config.h"

	#define HDA_CODEC_ADDRESS             0
	#define HDA_MAX_SAMPLES_PER_FRAME     4    // 1 = 48kHz, 2 = 96kHz, 4 = 192kHz
	#define HDA_DAC_BITS_PER_SAMPLE       HDA_BPS_24   // 24 only
	#define HDA_DAC_NUM_MAX               5
	#define HDA_ADC_NUM_MAX               3

	#define GET_CONVERTER_BITS(HDA_BPS) ( \
		((HDA_BPS) == 0) ? 8 :  \
		((HDA_BPS) == 1) ? 16 : \
		((HDA_BPS) == 2) ? 20 : \
		((HDA_BPS) == 3) ? 24 : \
		((HDA_BPS) == 4) ? 32 : 0)

	#define HDA_BPS_08 0
	#define HDA_BPS_16 1
	#define HDA_BPS_20 2
	#define HDA_BPS_24 3
	#define HDA_BPS_32 4

	#ifndef HDA_DAC_NUM
		#define HDA_DAC_NUM HDA_DAC_NUM_MAX  
	#endif

	#ifndef HDA_ADC_NUM
		#define HDA_ADC_NUM HDA_ADC_NUM_MAX
	#endif
	
	#ifndef HDA_ADC_MAX_FREQ
		#define HDA_ADC_MAX_FREQ 192000
	#endif
	
	// 6 каналов АЦП при 24бит/192кГц не помещаются в 464 бита линии SDI,
	// поэтому принудительно понижаем разрядность до 16 бит.
	#if HDA_ADC_NUM > 2 && HDA_ADC_MAX_FREQ > 96000
		#define HDA_ADC_BITS_PER_SAMPLE     HDA_BPS_16
	#else
		#define HDA_ADC_BITS_PER_SAMPLE     HDA_BPS_24
	#endif
	


	// HDA_SINGLE_STREAM - была попытка сделать, так, что бы на все коодеки был один
	// поток (а не 5), что бы упростить порядок следовани семплов:
	// 0 - на каждый ЦАП/АЦП свой поток, смещение=0, 1 - по одному потоку на ЦАП/АЦП, смещения инкрементируются
	// Выяснилось, что у ALC889 эта штука работает только на скорости 48кГц (когда порядок следования
	// семплов и так нормальный), а на 96кГц нормально работает только кодек со смещением 0, остальные
	// переводятся в режим 48кГц (похоже на баг самого кодека).
	// Ниже в указано, какой № семпл в потоке откликается в каком канале и с какой скоростью:
	// №    кан.   скорость
	// 0    0L     96кГц
	// 1    0R     96кГц
	// 2    0L     96кГц
	// 3    0R     96кГц
	// 4    1L     48кГц
	// 5    1R     48кГц
	// 6    2L     48кГц
	// 7    2R     48кГц
	// 8    3L     48кГц
	// 9    3R     48кГц
	// 10   4L     48кГц
	// 11   4R     48кГц
	// 12    x      --
	// На скорости 192кГц ситуация похожая: семплы 0-7 отправляюися в канал 0,
	// остальные каналы берут по 2 семпла.
	// Для работы на 96 и 192кГц HDA_SINGLE_STREAM должен быть равен 0
	//
	#define HDA_SINGLE_STREAM     0

	#if 0  // 1 - compact fields, 0 - 8-bit fields
		#define BIT1 1
		#define BIT4 4
		#define BIT7 7
		#define BIT8 8
	#else
		#define BIT1 8
		#define BIT4 8
		#define BIT7 8
		#define BIT8 8
	#endif


	typedef struct
	{
		uint8_t nid                         :BIT8;    //   0  Node ID

		// answer at f00:09
		uint8_t widget_type                 :BIT4;    //   1  bit 23:20
		uint8_t widget_delay                :BIT4;    //   2  bit 19:16
		uint8_t power_ctl                   :BIT1;    //   3  bit 10
		uint8_t digital                     :BIT1;    //   4  bit 9
		uint8_t conn_list_qr                :BIT1;    //   5  bit 8
		uint8_t unsol_cap                   :BIT1;    //   6  bit 7
		uint8_t processing_widget           :BIT1;    //   7  bit 6
		uint8_t format_override             :BIT1;    //   8  bit 4
		uint8_t AMP_param_override          :BIT1;    //   9  bit 3
		uint8_t AMP_out_present             :BIT1;    //  10  bit 2
		uint8_t AMP_in_present              :BIT1;    //  11  bit 1
		uint8_t stereo                      :BIT1;    //  12  bit 0

		//answer at f00:0c
		uint8_t pin_VREF_cap                :BIT8;    //  13  bit 15:8
		uint8_t pin_LR_swap                 :BIT1;    //  14  bit 7
		uint8_t pin_balanced                :BIT1;    //  15  bit 6
		uint8_t pin_in_cap                  :BIT1;    //  16  bit 5
		uint8_t pin_out_cap                 :BIT1;    //  17  bit 4
		uint8_t pin_headphone_drv           :BIT1;    //  18  bit 3;
		uint8_t pin_presence_det            :BIT1;    //  19  bit 2
		uint8_t pin_trigger_rq              :BIT1;    //  20  bit 1
		uint8_t pin_impedance_sens          :BIT1;    //  21  bit 0

		//answer at f00:0d - input amplifier parameters
		uint8_t in_AMP_mute                 :BIT1;    //  22  31
		uint8_t in_AMP_step_size            :BIT8;    //  23  22:16
		uint8_t in_AMP_step_number          :BIT8;    //  24  14:8
		uint8_t in_AMP_offset_0db           :BIT8;    //  25  6:0


		//answer at f00:12 - output amplifier parameters
		uint8_t out_AMP_mute                :BIT1;    //  26  31
		uint8_t out_AMP_step_size           :BIT8;    //  27  22:16
		uint8_t out_AMP_step_number         :BIT8;    //  28  14:8
		uint8_t out_AMP_offset_0db          :BIT8;    //  29  6:0

		//answer at f00:0e - connection list length
		uint8_t  conn_list_short            :BIT1;    //  30  7
		uint8_t  conn_list_length           :BIT7;    //  31  6:0
		uint16_t conn_list_start            :BIT8;    //  32  start (offset) list of this node in global connection list

		// converter stream parameters
		uint8_t  stream_ID                  :BIT4;    //  33  Stream ID (1...15)
		uint8_t  stream_offset              :BIT4;    //  34  int stream offset (0...15)

		uint8_t  converter_enabled          :BIT1;    //  35  converter (ADC/DAC) in path and enabled
//		//answer at f02:xx - connection list content
//		uint8_t conn_list[16];

		//answer at f00:13 - volume knob capabilities
		//uint8_t vol_knob_delta              :BIT1;    // 7
		//uint8_t vol_knob_numsteps           :BIT7;    // 6:0


	}hda_AfgNode_param_t;


	typedef struct
	{
		uint16_t            vid, did;
		uint8_t             root_node_start;
		uint8_t             root_node_count;

		uint8_t             FG_nodes_type;
		uint8_t             FG_nodes_UnSol;
		uint8_t             FG_node_start;
		uint8_t             FG_node_count;

		// DAC stream parameters
		uint8_t  stream_DAC_start;
		uint8_t  codec_DAC_conv_count;
		uint8_t  codec_DAC_chnn_count;
		uint8_t  path_DAC_conv_count;
		uint8_t  path_DAC_chnn_count;

		// ADC stream parameters
		uint8_t  stream_ADC_start;
		uint8_t  codec_ADC_conv_count;
		uint8_t  codec_ADC_chnn_count;
		uint8_t  path_ADC_conv_count;
		uint8_t  path_ADC_chnn_count;


		// pins count
		uint8_t  pins_input_count;
		uint8_t  pins_output_count;

		// nodes parameters
		hda_AfgNode_param_t nodes_param[64];

	}codec_capabilities_t;

	typedef struct
	{
		uint8_t from, to;
		uint8_t bps;
	} codec_path_t;


#endif /* CODEC_H_INCLUDED */
