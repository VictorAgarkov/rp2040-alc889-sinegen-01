/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "hda_codec.h"
#include "hda_link_rp2040.h"


#define PRINT_EN 0x00 //(4 | 8)


#if (PRINT_EN)
	#include "hardware/uart.h"
	
	extern char str[256];
	
	char * get_bit_str(char *s, uint32_t val, int msb, int lsb, bool valid);
	char * get_bit_strz(char *s, uint32_t val, int msb, int lsb, bool valid, char zero);
	int get_32bit_field(uint32_t val, int msb, int lsb);
	
	
	const char *widget_types[] =
	{
		"audio out", // 0
		"audio in", // 1
		"mixer", // 2
		"selector", // 3
		"pin complex", // 4
		"power widget", // 5
		"volume knob", // 6
		"reserved", // 7 (7 - 14)
		"vendor defined" // 8 (15)
	};
	

	const char *stream_types[] =
	{
		"PCM",
		"Non-PCM"
	};

	const char *mute_types[] =
	{
		"Unmute",
		"Mute"
	};

	const char *base_samplerates[] =
	{
		"48.0",
		"44.1"
	};

	const char *vref_types[] =
	{
		"Hi-Z",
		"50%",
		"GND",
		"--",
		"80%",
		"100%"
	};


	const uint8_t adc_samplerate_MULT[] =
	{
		1, 2, 3, 4, 0, 0, 0, 0
	};

	
	
#endif




const uint8_t adc_bits_per_sample[8] = 
{
	8, 16, 20, 24, 32, 0, 0, 0
};

const samplerate_base_mul_t g_samplerate_base_mul[6] = 
{
	//freq, base, mul
	{ 44100, 1, 1},
	{ 48000, 0, 1},
	{ 88200, 1, 2},
	{ 96000, 0, 2},
	{176400, 1, 4},
	{192000, 0, 4},
};

// source-to-destination pathes
// default nodes
const codec_path_t codec_path_list[] =
{
	{0x02, 0x14, HDA_DAC_BITS_PER_SAMPLE},  // DAC5   -> FRONT   [D]
#if HDA_DAC_NUM >= 2
	{0x03, 0x15, HDA_DAC_BITS_PER_SAMPLE},  // DAC4   -> SURR    [A]
#if HDA_DAC_NUM >= 3
	{0x04, 0x16, HDA_DAC_BITS_PER_SAMPLE},  // DAC3   -> CEN/LFE [G]
#if HDA_DAC_NUM >= 4
	{0x05, 0x17, HDA_DAC_BITS_PER_SAMPLE},  // DAC2   -> SIDESUR [H]
#if HDA_DAC_NUM >= 5
	{0x25, 0x1b, HDA_DAC_BITS_PER_SAMPLE},  // DAC1   -> LINE2   [E]
#endif
#endif
#endif
#endif
	
	{0x18, 0x09, HDA_DAC_BITS_PER_SAMPLE},  // MIC1  [B] -> ADC1
#if HDA_ADC_NUM >= 2
	{0x19, 0x08, HDA_DAC_BITS_PER_SAMPLE},  // MIC2  [F] -> ADC2
#if HDA_ADC_NUM >= 3
	{0x1a, 0x07, HDA_DAC_BITS_PER_SAMPLE},  // LINE1 [C] -> ADC3
#endif
#endif
};

/*
// ALC889 proto board conn.
const codec_path_t codec_path_list[] =
{
	{0x03, 0x1a, HDA_DAC_BITS_PER_SAMPLE},  // DAC4        -> LINE1 [C] 4
#if HDA_DAC_NUM >= 2
	{0x05, 0x19, HDA_DAC_BITS_PER_SAMPLE},  // DAC2        -> MIC2  [F] 2
#if HDA_DAC_NUM >= 3
	{0x04, 0x18, HDA_DAC_BITS_PER_SAMPLE},  // DAC3        -> MIC1  [B] 3
#if HDA_DAC_NUM >= 4
	{0x25, 0x1b, HDA_DAC_BITS_PER_SAMPLE},  // DAC1        -> LINE2 [E] 1
#if HDA_DAC_NUM >= 5
	{0x02, 0x14, HDA_DAC_BITS_PER_SAMPLE},  // DAC5        -> FRONT [D] 5
#endif
#endif
#endif
#endif
	
	{0x16, 0x09, HDA_ADC_BITS_PER_SAMPLE},  // CEN/LFE [G] -> ADC1 1
#if HDA_ADC_NUM >= 2
	{0x15, 0x08, HDA_ADC_BITS_PER_SAMPLE},  // SURR    [A] -> ADC2 2
#if HDA_ADC_NUM >= 3
	{0x17, 0x07, HDA_ADC_BITS_PER_SAMPLE},  // SIDESUR [H] -> ADC3 3
#endif
#endif
	//{0x0a, 0x06, -1},  // impossible path (for debug)
	//{0x17, 0x15, -1},  // pin to pin (for debug)
};
*/


codec_capabilities_t codec;
uint8_t g_conn_list[160]; // global connection list


void     verb_to_queue(uint32_t verb32);
uint32_t wait_codec_resp(void);
void hdac_print_codec_capabilities(void);
void hdal_wait_queues_emty(void);

int print_verb_en = 0;
int pin_unsol_count = 0;  // счётчик нод, которе могут отправлять unsolicited response

volatile const samplerate_base_mul_t  *codec_mic_SBM =   &g_samplerate_base_mul[1]; // default 48kHz
volatile const samplerate_base_mul_t  *codec_spk_SBM =   &g_samplerate_base_mul[1]; // default 48kHz


//----------------------------------------------------------------------------------------------------------
// инициализируем кодек
// start_adc - с какого номера начнуться номера потоков АЦП
// start_dac - с какого номера начнуться номера потоков ЦАП
void hdac_init_startcount(int start_adc, int start_dac)
{
	memset(&codec, 0, sizeof(codec));
	codec.stream_DAC_start = start_dac;
	codec.stream_ADC_start = start_adc;

	hdac_read_codec_property();
	hdac_print_codec_capabilities();
	hdac_codec_start();
	hdal_wait_queues_emty();
	print_audiofunctions_parameters();
}

//----------------------------------------------------------------------------------------------------------
// bit     value
// 31:28   Codec Address (CAD)
// 27:20   Node id (NID)
// 19:16   4-bit Verb ID
// 15: 0   Payload
uint32_t hdac_make_verb4(uint32_t node_id, uint32_t verb_id, uint32_t payload)
{
	#if (PRINT_EN & 4)
		if(print_verb_en)
		{
			sprintf(str, "Make verb 4 : nid = %02x, verb = %01x, load = %04x"CRLF, (int)node_id, (int)verb_id, (int)payload);
			uartputs(str);
		}
	#endif // PRINT_EN & 4
	
	return ((HDA_CODEC_ADDRESS & 0x0000000f) << 28) | ((node_id & 0x000000ff) << 20) | ((verb_id & 0x0000000f) << 16) | (payload & 0x0000ffff);
}
//#define hdac_make_verb4(node_id,verb_id,payload) ((HDA_CODEC_ADDRESS & 0x0000000f) << 28) | ((node_id & 0x000000ff) << 20) | ((verb_id & 0x0000000f) << 16) | (payload & 0x0000ffff)
//----------------------------------------------------------------------------------------------------------
// bit     value
// 31:28   Codec Address (CAD)
// 27:20   Node id (NID)
// 19: 8   12-bit Verb ID
//  7: 0   Payload
uint32_t hdac_make_verb12(uint32_t node_id, uint32_t verb_id, uint32_t payload)
{
	#if (PRINT_EN & 4)
		if(print_verb_en)
		{
			sprintf(str, "Make verb 12: nid = %02x, verb = %03x, load = %02x"CRLF, (int)node_id, (int)verb_id, (int)payload);
			uartputs(str);
		}
	#endif // PRINT_EN & 4
	return ((HDA_CODEC_ADDRESS & 0x0000000f) << 28) | ((node_id & 0x000000ff) << 20) | ((verb_id & 0x00000fff) << 8) | (payload & 0x000000ff);
}
//#define hdac_make_verb12(node_id,verb_id,payload) ((HDA_CODEC_ADDRESS & 0x0000000f) << 28) | ((node_id & 0x000000ff) << 20) | ((verb_id & 0x00000fff) << 8) | (payload & 0x000000ff)
//----------------------------------------------------------------------------------------------------------
#if (PRINT_EN)
	const char *hdac_get_widgettype_name_by_type(int widget_type)
	{
		const char *ct = widget_types[7]; // reserved
		if(widget_type <= 7) ct = widget_types[widget_type];
		else if(widget_type == 15) ct = widget_types[8];  // vendor defined
		return ct;
	}
	//----------------------------------------------------------------------------------------------------------
	const char *hdac_get_widgettype_name_by_nid(int nid)
	{
		int idx = hdac_get_node_idx_by_id(nid);
		if(idx < 0) return "";
		
		return hdac_get_widgettype_name_by_type(codec.nodes_param[idx].widget_type);	
	}
#endif 
//----------------------------------------------------------------------------------------------------------
int hdac_read_codec_property()
{
	int k;
	int ret; 
	uint32_t resp, resp2;
	
	hdal_send_verb(hdac_make_verb12(0, 0xf00, 0)); // verb - Get vendor & device ID
	hdal_send_verb(hdac_make_verb12(0, 0xf00, 2)); // verb - Get revision ID	
	hdal_send_verb(hdac_make_verb12(0, 0xf00, 4)); // verb - Subourdinate node count
	
	resp = hdal_wait_response();
	resp2 = hdal_wait_response();
	
	codec.vid = resp >> 16;
	codec.did = resp;
	#if (PRINT_EN & 1)
	#endif // PRINT_EN

	#if (PRINT_EN & 1)
		sprintf(str, "Codec ID - %04x:%04x"CRLF"Revision ID - %08x"CRLF, (int)codec.vid, (int)codec.did, (int)resp2);
		uartputs(str);
	#endif // PRINT_EN

	// get number of function groups in the codec
	resp = hdal_wait_response();
	//hdac_make_verb12(g_send_buff + 1, 0, );  
	//hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
	codec.root_node_count = resp;
	codec.root_node_start = resp >> 16;
	#if (PRINT_EN & 1)
		sprintf(str, "Codec root nodes - %i, start - %i"CRLF, (int)codec.root_node_count, (int)codec.root_node_start);
		uartputs(str);
	#endif // PRINT_EN
	
	for(int i = codec.root_node_start; i < codec.root_node_start + codec.root_node_count; i++)
	//for(i = codec.root_node_start; i < codec.root_node_start + codec.root_node_count;)
	{
		hdal_send_verb(hdac_make_verb12(i, 0xf00, 5)); // verb - node function group type
		hdal_send_verb(hdac_make_verb12(i, 0xf00, 4));//  verb - Subourdinate node count
		hdal_send_verb(hdac_make_verb12(i, 0xf05, 0)); //  verb - get power state

		resp = hdal_wait_response(); // verb - node function group type
		codec.FG_nodes_UnSol = (resp >> 8) & 1; //g_rcv_buff[4];     // 8
		codec.FG_nodes_type  = (resp >> 0) & 0xff; //g_rcv_buff[6];  // 7-0

		resp = hdal_wait_response();//  verb - Subourdinate node count
		codec.FG_node_start = (resp >> 16) & 0xff; //g_rcv_buff[4]; // 23-16
		codec.FG_node_count = (resp >>  0) & 0xff; //g_rcv_buff[6]; // 7-0
		
		#if (PRINT_EN & 1)
			sprintf(str, "Codec FG nodes - %i, start - %i, type - %02xh"CRLF, (int)codec.FG_node_count, (int)codec.FG_node_start, (int)codec.FG_nodes_type);
			uartputs(str);
		#endif // PRINT_EN

		resp = hdal_wait_response(); //  verb - get power state

		#if (PRINT_EN & 1)
			sprintf(str, "Actual power state=D%i", (int)(resp >> 4) & 3);
			uartputs(str);
			sprintf(str, "; Set power state=D%i"CRLF, (int)(resp >> 0) & 3);
			uartputs(str);
			for(int j = 0; j < 198; j++) uartputs("-"); 
			uartputs(CRLF);
			
			uartputs("NID|      F00:09          D P D C U P F A O I S  |        F00:0C        |    F00:0d    |    F00:12    |        F1C:00            | F00:0E |                  F02:xx                           |  F01 |"CRLF);
			uartputs("   |      Node type (##)    a 9 8 7 6 4 3 2 1 0  | ref 7 6 5 4 3 2 1 0  |  M ss ns of  |  M ss ns of  |  C Lo DD CT Cl Mi DA Sq  |        |  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0  |  :00 |"CRLF);
	
			for(int j = 0; j < 198; j++) uartputs("-"); 
			uartputs(CRLF);
		#endif // PRINT_EN

		if(codec.FG_nodes_type == 1)  // if nodes is audio function
		//if(false)  // no routine
		{
			int conn_list_offset = 0;
			for(int nid = codec.FG_node_start, idx = 0; nid < codec.FG_node_start + codec.FG_node_count; nid++, idx++)
			//for(j = 26, idx = 0; ;)
			{
				{
					const char *ct;
					char *cp;
					int widget_type, valid = 1;

					if(idx >= sizeof(codec.nodes_param) / sizeof(codec.nodes_param[0])) break;

					hda_AfgNode_param_t *pnode = &codec.nodes_param[idx];

					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 9));// verb - audio widget capabilites
					//hdac_make_verb12(g_send_buff + 1, 0, j, 0xf00, 9);  
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);


					pnode->nid                = nid;                 // node ID
					pnode->widget_type        = (resp >> 20) & 0x0f; //g_rcv_buff[4] >> 4;      // 23:20
					pnode->widget_delay       = (resp >> 16) & 0x0f; //g_rcv_buff[4] & 0x0f;    // 19:16
					pnode->power_ctl          = (resp >> 10) & 1;    //(g_rcv_buff[5] >> 2) & 1;   // 10
					pnode->digital            = (resp >>  9) & 1;    //(g_rcv_buff[5] >> 1) & 1;     // 9
					pnode->conn_list_qr       = (resp >>  8) & 1;    //(g_rcv_buff[5] >> 0) & 1;   // 8
					pnode->unsol_cap          = (resp >>  7) & 1;    //(g_rcv_buff[6] >> 7) & 1;   // 7
					pnode->processing_widget  = (resp >>  6) & 1;    //(g_rcv_buff[6] >> 6) & 1; // 6
					pnode->format_override    = (resp >>  4) & 1;    //(g_rcv_buff[6] >> 4) & 1;   // 4
					pnode->AMP_param_override = (resp >>  3) & 1;    //(g_rcv_buff[6] >> 3) & 1; // 3
					pnode->AMP_out_present    = (resp >>  2) & 1;    //(g_rcv_buff[6] >> 2) & 1; // 2
					pnode->AMP_in_present     = (resp >>  1) & 1;    //(g_rcv_buff[6] >> 1) & 1;  // 1
					pnode->stereo             = (resp >>  0) & 1;    //(g_rcv_buff[6] >> 0) & 1;      // 0

					widget_type = pnode->widget_type;
					if(pnode->digital == 0)
					{
						int channels_in_node = (pnode->stereo) ? 2 : 1; // 
						// аналоговый вход/выход
						// для них назначаем номера потоков и смещения внутри них
						if(widget_type == 0)
						{
							// DAC audio out ( 0)
							
							codec.codec_DAC_conv_count++;
							codec.codec_DAC_chnn_count += channels_in_node;
						}
						else if(widget_type == 1)
						{
							// ADC audio in ( 1)
							codec.codec_ADC_conv_count++;
							codec.codec_ADC_chnn_count += channels_in_node;
						}
					}

					#if (PRINT_EN & 1)

						valid = true;
						
						ct = hdac_get_widgettype_name_by_type(widget_type);


						sprintf(str, "%02x |%15s (%2i)  ", nid, ct, widget_type);
						cp = str + strlen(str);
	
							// printf("Nd:%02xh type = %s"CRLF, nid, ct);
	
						sprintf(cp, "%01x ", (int)get_32bit_field(resp, 19, 16));
						cp += strlen(cp);  //cp = get_bit_str(cp, resp, 19, 16, valid);
	
						cp = get_bit_str(cp, resp, 10, 10, valid);
						cp = get_bit_str(cp, resp, 9, 9, valid);
						cp = get_bit_str(cp, resp, 8, 8, valid);
						cp = get_bit_str(cp, resp, 7, 7, valid);
						cp = get_bit_str(cp, resp, 6, 6, valid);
						cp = get_bit_str(cp, resp, 4, 4, valid);
						cp = get_bit_str(cp, resp, 3, 3, valid);
						cp = get_bit_str(cp, resp, 2, 2, valid);
						cp = get_bit_str(cp, resp, 1, 1, valid);
						cp = get_bit_str(cp, resp, 0, 0, valid);
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN


					// valid = false;
					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 0x0c)); // verb - pin capabilities
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf00, 0x0c); 
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff); 
					pnode->pin_VREF_cap       = (resp >>  8) & 0xff; //(g_rcv_buff[5] >> 0);           // bit 15:8
					pnode->pin_LR_swap        = (resp >>  7) & 1;    //(g_rcv_buff[4] >> 7) & 1;       // bit 7
					pnode->pin_balanced       = (resp >>  6) & 1;    //(g_rcv_buff[4] >> 6) & 1;       // bit 6
					pnode->pin_in_cap         = (resp >>  5) & 1;    //(g_rcv_buff[4] >> 5) & 1;       // bit 5
					pnode->pin_out_cap        = (resp >>  4) & 1;    //(g_rcv_buff[4] >> 4) & 1;       // bit 4
					pnode->pin_headphone_drv  = (resp >>  3) & 1;    //(g_rcv_buff[4] >> 3) & 1;       // bit 3
					pnode->pin_presence_det   = (resp >>  2) & 1;    //(g_rcv_buff[4] >> 2) & 1;       // bit 2
					pnode->pin_trigger_rq     = (resp >>  1) & 1;    //(g_rcv_buff[4] >> 1) & 1;       // bit 1
					pnode->pin_impedance_sens = (resp >>  0) & 1;    //(g_rcv_buff[4] >> 0) & 1;       // bit 0

					#if (PRINT_EN & 1)
						sprintf(str, "%02x ", (int)get_32bit_field(resp, 15, 8));
						cp = str + strlen(str);  
	
						cp = get_bit_str(cp, resp, 7, 7, valid);
						cp = get_bit_str(cp, resp, 6, 6, valid);
						cp = get_bit_str(cp, resp, 5, 5, valid);
						cp = get_bit_str(cp, resp, 4, 4, valid);
						cp = get_bit_str(cp, resp, 3, 3, valid);
						cp = get_bit_str(cp, resp, 2, 2, valid);
						cp = get_bit_str(cp, resp, 1, 1, valid);
						cp = get_bit_str(cp, resp, 0, 0, valid);
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN

					// valid = false;
					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 0x0d));// verb - input amplifier parameters
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf00, 0x0d);  
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff); 
					pnode->in_AMP_mute        = (resp >>  31) & 1;       //(g_rcv_buff[3] >> 7) & 1;         // 31
					pnode->in_AMP_step_size   = (resp >>  16) & 0x7f;    //(g_rcv_buff[4] >> 0) & 0x7f;       // 22:16
					pnode->in_AMP_step_number = (resp >>   8) & 0x7f;    //(g_rcv_buff[5] >> 0) & 0x7f;      // 14:8
					pnode->in_AMP_offset_0db  = (resp >>   0) & 0x7f;    //(g_rcv_buff[6] >> 0) & 0x7f;    // 6:0

					#if (PRINT_EN & 1)
						cp = str;
						cp = get_bit_str(cp, resp, 31, 31, valid);         // 31
						cp = get_bit_str(cp, resp, 22, 16, valid);         // 22:16
						cp = get_bit_str(cp, resp, 14, 8,  valid);         // 14:8
						cp = get_bit_str(cp, resp,  6, 0,  valid);         // 6:0
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN

					// valid = false;
					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 0x12));  // verb - output amplifier parameters
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf00, 0x12);
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
					pnode->out_AMP_mute        = (resp >>  31) & 1;       //(g_rcv_buff[3] >> 7) & 1;      // 31
					pnode->out_AMP_step_size   = (resp >>  16) & 0x7f;    //(g_rcv_buff[4] >> 0) & 0x7f;   // 22:16
					pnode->out_AMP_step_number = (resp >>   8) & 0x7f;    //(g_rcv_buff[5] >> 0) & 0x7f;   // 14:8
					pnode->out_AMP_offset_0db  = (resp >>   0) & 0x7f;    //(g_rcv_buff[6] >> 0) & 0x7f;   // 6:0    

					#if (PRINT_EN & 1)
						cp = str;
						cp = get_bit_str(cp, resp, 31, 31, valid);              
						cp = get_bit_str(cp, resp, 22, 16, valid);              
						cp = get_bit_str(cp, resp, 14,  8, valid);               
						cp = get_bit_str(cp, resp,  6,  0, valid);                
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN




					// F00:13 - не нужно (Volume knob capabilities: response for NID 0x21 (volume control knob))
					//resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 0x13));  // verb - volume knob capabilities
					////hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf00, 0x13);
					////ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
					//pnode->vol_knob_delta    = (resp >>  7) & 1;       //(g_rcv_buff[6] >> 7) & 1;      // 7
					//pnode->vol_knob_numsteps = (resp >>  0) & 0x7f;    //(g_rcv_buff[6] >> 0) & 0x7f;   // 6:0
					//
					//if(PRINT_EN & 1)
					//{
					//	cp = str;
					//	cp = get_bit_str(cp, resp + 3, 7, 7, valid);        
					//	cp = get_bit_str(cp, resp + 3, 6, 0, valid);        
					//	uartputs(str);
					//	uartputs(" |  ");
					//}

					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf1c, 0));  // verb - get config default
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf1c, 0); 
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);

					#if (PRINT_EN & 1)
						char port_coonectivity[4] = {'P', 'N', 'H', 'x', };
						char *colors[16] = {"-- ", "Bk ", "Gy ", "Bl ", "Gn ", "Rd ", "Or ", "Ye ", "Pu ", " 9 ", "10 ", "11 ", "12 ", "13 ", "Wh ", "Ot "};
						cp = str;
						*(cp++) = port_coonectivity[(resp >> 30) & 3]; *(cp++) = ' '; 
						//cp = get_bit_str(cp, resp, 31, 30, valid);         //
						cp = get_bit_str(cp, resp, 29, 24, valid);         //
						cp = get_bit_str(cp, resp, 23, 20, valid);         //
						cp = get_bit_str(cp, resp, 19, 16, valid);         //
						//cp = get_bit_str(cp, resp, 15, 12, valid);         //
						memcpy(cp, colors[(resp >> 12) & 15], 3); cp += 3;
						cp = get_bit_str(cp, resp, 11,  8, valid);         //
						cp = get_bit_str(cp, resp,  7,  4, valid);         //
						cp = get_bit_str(cp, resp,  3,  0, valid);         //
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN

					// valid = false;
					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf00, 0x0e));  // verb - connect list length
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf00, 0x0e); (resp >>  ) & 1;    //
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
					pnode->conn_list_short    = (resp >>  7) & 1;       //(g_rcv_buff[6] >> 7) & 1;     // 7
					pnode->conn_list_length   = (resp >>  0) & 0x7f;    //(g_rcv_buff[6] >> 0) & 0x7f;  // 6:0
					pnode->conn_list_start    = conn_list_offset;

					#if (PRINT_EN & 1)
						cp = str;
						cp = get_bit_str(cp, resp, 7, 7, valid);         // 7
						cp = get_bit_str(cp, resp, 6, 0, valid);        // 6:0
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN

					if(pnode->conn_list_short == 0)
					{
						// 8-bit connection entry
						//
						// g_conn_list[conn_list_offset]
						for(k = 3; k >= 0;  k--)
						{
							int ii = k * 4;
							int iii;
							int cll = pnode->conn_list_length;
							//memset(&pnode->conn_list[ii], 0, 4);
							if(cll > ii)
							{
								// have conn in this range

								// valid = false;
								resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf02, ii)); // verb - get connection list entry 15-12, 11-8, 7-4, 3-0
								//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf02, ii);  
								//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
								for(iii = 0; iii < 4; iii++)
								{
									//pnode->conn_list[ii + iii]    = (g_rcv_buff[6 - iii] >> 0);
									if((ii + iii) < pnode->conn_list_length)
									{
										int list_idx = ii + iii + conn_list_offset;
										if(list_idx > sizeof(g_conn_list) / sizeof(g_conn_list[0]))
										{
											#if (PRINT_EN & 1)
												uartputs(CRLF CRLF "!ERROR connection list too small for codec connections!!"CRLF CRLF);
											#endif // PRINT_EN
											return 2;
										}
										int shift = iii * 8;
										g_conn_list[list_idx]    = (resp >>  shift) & 0xff;    //(g_rcv_buff[6 - iii] >> 0);
									}
								}
							}
						}
					}
					else
					{
						// 16-bit connection entry
					}

					#if (PRINT_EN & 1)
						for(k = 15; k >= 0; k--)
						{
							if(k < pnode->conn_list_length)
							{
								int val = g_conn_list[k + conn_list_offset] ;
								sprintf(str, "%02x ", val);
								uartputs(str);
							}
							else
							{
								uartputs("-- ");
							}
						}
					#endif // PRINT_EN


					conn_list_offset += pnode->conn_list_length;  // update offset




					//valid = false;
					resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf01, 0));  // verb - get connection select control
					//hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf01, 0); 
					//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);

					#if (PRINT_EN & 1)
						uartputs(" |  ");
						int v = resp & 0xff;
						if(pnode->conn_list_length) sprintf(str, "%02x ", v);
						else                                        sprintf(str, "-- ");
						uartputs(str);
						uartputs(" |  ");
					#endif // PRINT_EN


					// valid = false;
					// F0E:00 - не нужно (Get digital converter control 2: response for NID 06, 10 - SPDIF out, 0a - SPDIF in) - всегда отвечает 0000
					//resp = hdal_send_verb_wait_response(hdac_make_verb12(nid, 0xf0e, 0)); // verb - get digital converter control 1
					////hdac_make_verb12(g_send_buff + 1, 0, nid, 0xf0e, 0); 
					////ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
					//if(PRINT_EN & 1)
					//{
					//	uint32_t v = resp & 0xffff;
					//	if(v) sprintf(str, "%04x ", (int)resp & 0xffff);
					//	else  sprintf(str, "---- ");
					//	uartputs(str);
					//	uartputs(" |  ");
					//}


					#if (PRINT_EN & 1)
						uartputs(CRLF);
					#endif // PRINT_EN


				}
			}
			
			#if (PRINT_EN & 1)
				for(int j = 0; j < 198; j++) uartputs("-");
				uartputs(CRLF);
			#endif // PRINT_EN
		}
	}
	return 0;
}

//----------------------------------------------------------------------------------------------------------
/*
										       
											       

*/
void hdac_print_codec_capabilities(void)
{
	// for(int i = 0; i < codec.FG_node_count; i++)
	// {
		// hda_AfgNode_param_t *pp =  codec.nodes_param + i;
		// sprintf
		// (
			// str, 
			// "{%3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i ,",
			// (int) pp->index                       ,
			// (int) pp->widget_type                 ,    // bit 23:20
			// (int) pp->widget_delay                ,    // bit 19:16
			// (int) pp->power_ctl                   ,    // bit 10
			// (int) pp->digital                     ,    // bit 9
			// (int) pp->conn_list_qr                ,    // bit 8
			// (int) pp->unsol_cap                   ,    // bit 7
			// (int) pp->processing_widget           ,    // bit 6
			// (int) pp->format_override             ,    // bit 4
			// (int) pp->AMP_param_override          ,    // bit 3
			// (int) pp->AMP_out_present             ,    // bit 2
			// (int) pp->AMP_in_present              ,    // bit 1
			// (int) pp->stereo                      ,    // bit 0
			// (int) pp->pin_VREF_cap                ,    // bit 15:8
			// (int) pp->pin_LR_swap                 ,    // bit 7
			// (int) pp->pin_balanced                     // bit 6 
		// );
		// uartputs(str);
		
		// sprintf
		// (
			// str, 
			// "%3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i, %3i},"CRLF,
			// (int) pp->pin_in_cap                  ,    // bit 5
			// (int) pp->pin_out_cap                 ,    // bit 4
			// (int) pp->pin_headphone_drv           ,    // bit 3;
			// (int) pp->pin_presence_det            ,    // bit 2
			// (int) pp->pin_trigger_rq              ,    // bit 1
			// (int) pp->pin_impedance_sens          ,    // bit 0
			// (int) pp->in_AMP_mute                 ,    // 31
			// (int) pp->in_AMP_step_size            ,    // 22:16
			// (int) pp->in_AMP_step_number          ,    // 14:8
			// (int) pp->in_AMP_offset_0db           ,    // 6:0
			// (int) pp->out_AMP_mute                ,    // 31
			// (int) pp->out_AMP_step_size           ,    // 22:16
			// (int) pp->out_AMP_step_number         ,    // 14:8
			// (int) pp->out_AMP_offset_0db          ,    // 6:0
			// (int) pp-> conn_list_short            ,    // 7
			// (int) pp-> conn_list_length           ,    // 6:0			
			// (int) pp-> conn_list_start                 // 			
		// );
		// uartputs(str);
	// }
	
	
	// uartputs("codec_capabilities_t codec ="CRLF"{"CRLF);
	
	// sprintf(str, ".vid              = %i,"CRLF, codec.vid    );  uartputs(str);
	// sprintf(str, ".did              = %i,"CRLF, codec.did    );  uartputs(str);
	// sprintf(str, ".root_node_start  = %i,"CRLF, codec.root_node_start    );  uartputs(str);
	// sprintf(str, ".root_node_count  = %i,"CRLF, codec.root_node_count    );  uartputs(str);
	// sprintf(str, ".FG_nodes_type    = %i,"CRLF, codec.FG_nodes_type    );  uartputs(str);
	// sprintf(str, ".FG_nodes_UnSol   = %i,"CRLF, codec.FG_nodes_UnSol    );  uartputs(str);
	// sprintf(str, ".FG_node_start    = %i,"CRLF, codec.FG_node_start    );  uartputs(str);
	// sprintf(str, ".FG_node_count    = %i,"CRLF, codec.FG_node_count    );  uartputs(str);
	
	// uartputs(  ".nodes_param ="CRLF"{" CRLF CRLF CRLF CRLF"}"CRLF"};");
	
	// uartputs(   CRLF CRLF CRLF CRLF"uint8_t g_conn_list[160] ="CRLF"{");
	
	// for(int i = 0; i < ARRAYSIZE(g_conn_list); i++)
	// {
		// if((i &15) == 0) uartputs(CRLF);
		// sprintf(str, "%i, ", (int)g_conn_list[i]);  
		// uartputs(str);
	// }
	
	// uartputs(CRLF"};");
	
}

//----------------------------------------------------------------------------------------------------------
// node   - node ID
// stream - converter stream ID 1..15
// samplerate_div - division factor: 1,2, 4 (48, 96, 192 kHz)
// samplerate_mul - multipl. factor: 1,2, 4 (48, 96, 192 kHz)
// bps - bits per sample: enum HDA_BITS_PER_SAMPLE_e;
// channels_mask - bit[0] - left, bit[1] - right channel
int hdac_configure_converter(hda_AfgNode_param_t *pnode, uint8_t base, uint8_t samplerate_mul, uint8_t bps)
{
	uint32_t c_mult, c_bps, c_chns; // codec params
	uint32_t payload;
	int ret = 0;

	c_mult = samplerate_mul - 1;

	c_bps  = bps;
	c_chns = 1;

	// set converter format
	payload = (           0  << 15)  //  15    Stream Type (TYPE).0: PCM, 1: Non-PCM
	        | ((base   & 1)  << 14)  //  14    Sample Base Rate (BASE).0: 48kHz, 1: 44.1kHz
	        | ((c_mult & 7)  << 11)  //  13:11 Sample Base Rate Multiple (MULT).000b=*1, 001b=*2, 010b=*3, 011b=*4, 100b-111b: Reserved
	        | ((0      & 7)  <<  8)  //  10:8  Sample Base Rate Divisor (not supported). 000b=/1, 001b=/2, 010b=/3011b=/4, 100b=/5, 101b=/6110b=/7, 111b=/8
			| (     0        <<  7)  //  7     Reserved. Read as 0.
			| ((c_bps  & 7)  <<  4)  //  6:4   Bits per Sample (BITS).000b: 8 bits, 001b: 16 bits, 010b: 20 bits011b: 24 bits, 100b: 32 bits, 101b~111b: Reserved
			| ((c_chns  &15) <<  0); //  3:0   Number of Channels.0: 1 channel, 1: 2 channels, 2: 3 channels, ...... 15: 16 channels
	hdal_send_verb(hdac_make_verb4(pnode->nid, 0x2, payload));    // verb - set converter format
	//hdac_make_verb4(g_send_buff + 1, 0, node, 0x2, payload);
	//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
	//if(ret < 0) return ret;

	// enable channel
	payload = ((pnode->stream_ID     & 15) << 4)  // [7:4] - stream
	        | ((pnode->stream_offset & 15) << 0); // [1:0] - channel
	hdal_send_verb(hdac_make_verb12(pnode->nid, 0x706, payload));  // verb - set converter stream
	//hdac_make_verb12(g_send_buff + 1, 0, node, 0x706, payload);
	//ret = hdal_send_verb_wait_response(g_send_buff, g_rcv_buff);
	#if (PRINT_EN & 0x10)
		if(print_verb_en) sprintf
		(
			str, "Configure node = %02x, mul=%i, base=%i, bps=%i, chns=%i, stream=%i, offset=%i"CRLF,
			(int)pnode->nid, (int)c_mult, (int)base, (int)c_bps, (int)c_chns, (int)pnode->stream_ID, (int)pnode->stream_offset
		);
		uartputs(str);
	#endif // PRINT_EN & 4


	return ret;
}
//----------------------------------------------------------------------------------------------------------
//uint32_t hda_make_converter_format()
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
void hdac_set_node_selector(int nid, int sel)
{
	hdal_send_verb(hdac_make_verb12(nid, 0x701, sel));  // verb - set connection select
}
//----------------------------------------------------------------------------------------------------------

void hdac_set_amp_volume(int nid, int what_amp, int sel, int volume, int mute)
{
	//volume = 127;
	uint32_t payload = ((what_amp & 15) << 12) | ((sel & 15) << 8) | ((mute & 1) << 7) | ((volume & 0x7f) << 0);
	hdal_send_verb(hdac_make_verb4(nid, 0x3, payload));  // verb - set amplifier gain
}
//----------------------------------------------------------------------------------------------------------
void hdac_set_pin_mode(int nid, int as_in, int hp_amp_en, int vref_en)
{
	uint32_t payload;
	if(as_in) payload = (1 << 5) | ((vref_en & 7) << 0);
	else      payload = ((hp_amp_en & 1) << 7) | (1 << 6) | ((vref_en & 7) << 0);
	hdal_send_verb(hdac_make_verb12(nid, 0x707, payload));  // verb - set pin widget control

	// enable Jack Detect
	payload = (1 << 7) | ((++pin_unsol_count) & 15);
	hdal_send_verb(hdac_make_verb12(nid, 0x708, payload));  // verb - set unsolicited response control
}

//----------------------------------------------------------------------------------------------------------
void hdac_set_power_state(int nid, int pwr_state)
{
	pwr_state &= 3;
	uint32_t payload = (pwr_state << 4) | pwr_state;
	hdal_send_verb(hdac_make_verb12(nid, 0x705, payload));  // verb - set pin widget control
}
//----------------------------------------------------------------------------------------------------------
// конфигурируем ноды, входящие в состав chain длиной links_num
void hdac_make_path(nid_path_t *chain, int links_num, int bits)
{
	for(int i = 0; i < links_num; i++)
	{
//		printf("Node %i:", i);
		int idx = chain[i].idx;
		
		if(idx < 0) continue;
		
		int nid = chain[i].nid;
		int sel = chain[i].sel;

		hda_AfgNode_param_t *pnode = codec.nodes_param + idx;
		int wt = pnode->widget_type;

		hdac_set_power_state(nid, 0);

		int mute_cap = pnode->in_AMP_mute | pnode->out_AMP_mute;
		int volume = pnode->in_AMP_offset_0db | pnode->out_AMP_offset_0db;   // 0 dB
		//int volume = pnode->in_AMP_step_number | pnode->out_AMP_step_number;  // maximum amplify
		

		if(mute_cap || volume)
		{
			// если у ноды в нужном направлении есть мьют или громкость - настраиваем
			int ssel = 0;
			
			if(wt == 2) // if mixer - select necessary amp/mute
			{
				if(sel >= 0) ssel = sel;
			}			
			hdac_set_amp_volume(nid, 0b1111, ssel, volume, 0);
		}

		if(wt == 0 || wt == 1)
		{
			// ADC, DAC
			int conv_channels = pnode->stereo ? 2 : 1;
			if(wt == 0)
			{

				#if HDA_SINGLE_STREAM
					pnode->stream_ID     = codec.stream_DAC_start;   // один и тот же номер потока на все конвертеры
					pnode->stream_offset = codec.path_DAC_chnn_count;  // смещение конвера внутри потока инкрементируется
				#else
					pnode->stream_ID     = codec.stream_DAC_start + codec.path_DAC_conv_count; // номера потоков инкрементируются
					pnode->stream_offset = 0;                        // без смещения внутри потока
				#endif
				codec.path_DAC_chnn_count += conv_channels;
				codec.path_DAC_conv_count++;
			}
			else
			{
				#if HDA_SINGLE_STREAM
					pnode->stream_ID     = codec.stream_ADC_start;   // один и тот же номер потока на все конвертеры
					pnode->stream_offset = codec.path_ADC_chnn_count;  // смещение конвера внутри потока инкрементируется
				#else
					pnode->stream_ID     = codec.stream_ADC_start + codec.path_ADC_conv_count; // номера потоков инкрементируются
					pnode->stream_offset = 0;                        // без смещения внутри потока
				#endif
				codec.path_ADC_chnn_count += conv_channels;
				codec.path_ADC_conv_count++;
			}
			
			hdac_configure_converter(pnode, 0, 1, bits);
			pnode->converter_enabled = 1;
		}
		else if(wt == 4) // pin complex
		{
			// если конечная нода (№0 в chain) это pin complex (4),
			// то предполагаем, что вся цепь работает на выход
			// (as_in = 0), в остальных случаях - как вход (as_in = 1)
			hdac_set_pin_mode(nid, !!i, 0, 0);
		}

		if(pnode->conn_list_length > 1 && sel >= 0)
		{
			// если есть селектор - настраиваем его
			hdac_set_node_selector(nid, sel);
		}
//		printf(CRLF);

	}
}
//----------------------------------------------------------------------------------------------------------
void hdac_readback_path(nid_path_t *chain, int links_num)
{
	hdal_wait_queues_emty();
	hdal_send_verb_wait_response(hdac_make_verb12(0, 0xf00, 0));
	uint32_t verbs[] = {0xf01, 0xf05, 0xf06};
	for(int i = 0; i < links_num; i++)
	{
		int nid = chain[i].nid;
		for(int ii = 0; ii < ARRAYSIZE(verbs); ii++)
		{
			hdal_send_verb_wait_response(hdac_make_verb12(nid, verbs[ii], 0));
		}
	}
}
//----------------------------------------------------------------------------------------------------------
void hdac_print_path(nid_path_t *chain, int links_num)
{
	#if (PRINT_EN & 8)
		sprintf(str, "sizeof(nid_path_t) = %i"CRLF, sizeof(nid_path_t));
		uartputs(str);
		for(int i = links_num - 1; i >= 0; i--)
		{
				nid_path_t *pp = chain + i;
				uint32_t u32 = *(uint32_t*)pp;
				sprintf(str, "%i: NID = %02x, sel = %2i, idx = %i  (0x%08x)"CRLF, i, pp->nid, pp->sel, pp->idx, u32);
				uartputs(str);
		}
	#endif // PRINT_EN
}
//----------------------------------------------------------------------------------------------------------
//int hda_codec_set_force()
//{
//	uint32_t verbs[][3] = 
//	{
//		// 1. Настройка физического разъема (Pin Widget 17h)
//		{0x17, 0x705, 0x00   }, // Питание D0
//		{0x17, 0x701, 0x03   }, // Выбираем вход микшера (индекс 3)
//		{0x17, 0x707, 0x40   }, // Включаем выход наушников (HP En)
//		{0x17, 0x3,   0xb07f }, // Снимаем Mute с ВЫХОДА разъема (b07f убирает Mute)
//		// 2. Настройка промежуточного сумматора (Mixer Widget 0Fh)
//		{0x0f, 0x705, 0x00   }, // Питание D0
//		{0x0f, 0x701, 0x00   }, // Выбор входа
//		{0x0f, 0x3,   0xb07f }, // Снимаем Mute с ВЫХОДА микшера
//		{0x0f, 0x3,   0x707f }, // Критично: Снимаем Mute со ВХОДА №0 (впускаем звук от DAC 05)
//		// 3. Настройка цифрового конвертера (DAC Widget 05h)
//		{0x05, 0x705, 0x00   }, // Питание D0
//		{0x05, 0x2,   0x0031 }, // Формат потока
//		{0x05, 0x706, 0x60   }, // Привязка к Stream ID = 6
//		{0x05, 0x3,   0xb07f }, // Снимаем Mute с ВЫХОДА конвертера
//	};
//	
//	for(int i = 0; i < ARRAYSIZE(verbs); i++)
//	{
//		uint32_t verb = verbs[i][1] > 15 ? hdac_make_verb12(verbs[i][0], verbs[i][1], verbs[i][2]) : hdac_make_verb4(verbs[i][0], verbs[i][1], verbs[i][2]);
//		hdal_send_verb(verb);		
//	}
//	
//}
//
//
//----------------------------------------------------------------------------------------------------------

int hdac_codec_start()
{
	nid_path_t chain[4];

	uint8_t chn_mask[4] = {0, 1, 3, 3};
	
	pin_unsol_count = 0;

	for(int i = 0; i < ARRAYSIZE(codec_path_list); i++)
	{
		int from  = codec_path_list[i].from;
		int to    = codec_path_list[i].to;
		int bits  = codec_path_list[i].bps;

		chain[0].nid = to;
		int links_num = hdac_find_codec_path(from, chain, ARRAYSIZE(chain) - 1);

		if(links_num)
		{
			int  stream_id = 0;

			//printf(CRLF"Path %i"CRLF, i);
			hdac_make_path(chain, links_num + 1, bits);
			//hdac_print_path(chain, links_num + 1);
			//hdac_readback_path(chain, links_num + 1);
		}
		else
		{
			#if (PRINT_EN & 0x1000)
				sprintf(str, " -> !NOT FOUND! -> %02x (%s)"CRLF, to, hdac_get_widgettype_name_by_nid(to));
				uartputs(str);
			#endif
		}
	}
	#if (PRINT_EN & 0x1000)
		sprintf(str, "Audio in/out nodes:"CRLF"ADC from %2i, count %2i"CRLF"DAC from %2i, count %2i"CRLF, codec.stream_ADC_start, codec.path_ADC_conv_count, codec.stream_DAC_start, codec.path_DAC_conv_count);
		uartputs(str);
	#endif

	//hda_codec_set_force();

	return 0;
}
//----------------------------------------------------------------------------------------------------------
void hdac_codec_stop()
{
}
//----------------------------------------------------------------------------------------------------------
// return:
// 0... - index of node
// -1 - node not found
int hdac_get_node_idx_by_id(int node_id)
{
	int start = codec.FG_node_start;
	int cnt   = codec.FG_node_count;
	if(node_id >= start && node_id < start + cnt) return node_id - start;
	return -1;
}
//----------------------------------------------------------------------------------------------------------
int hdac_find_node_idx_in_conn_list(int selector_nid, int src_nid)
{
	//return:
	// >= 0 - index of 'node_id' in list
	// < 0 - not found
	uint8_t *conn_list;
	int i, num;

	i = hdac_get_node_idx_by_id(selector_nid);
	if(i < 0) return -1;

	conn_list = g_conn_list + codec.nodes_param[i].conn_list_start;
	num = codec.nodes_param[i].conn_list_length;
	// search
	for(i = 0; i < num; i++)
	{
		if(conn_list[i] == src_nid) return i;
	}
	return -1;
}
//----------------------------------------------------------------------------------------------------------
// пытаемся найти путь от ноды src_nid до
// той, которая лежит в chain[0];
// Если путь найден, возвращает сколько нод, кроме src_nid,
// участвует в маршруте, а в chain оказывается маршрут в
// обратном порядке (от назначения к источнику).
// Если маршрут не найден - возвращает 0.

int hdac_find_codec_path(int src_nid, nid_path_t *chain, int max_path_len) 
{
	if(!max_path_len)return 0;

	int dst_idx = hdac_get_node_idx_by_id(chain[0].nid);
	if(dst_idx < 0)return 0;

	chain->idx = dst_idx;

	int conn_list_length = codec.nodes_param[dst_idx].conn_list_length;

	if(!conn_list_length)return 0;

	int conn_list_start = codec.nodes_param[dst_idx].conn_list_start;

	nid_path_t *chain_next = chain + 1;

	chain_next->idx = -1;
	chain_next->sel = -1;

	for(int i = 0; i < conn_list_length; i++)
	{
		int list_idx = i + conn_list_start;
		int nid = g_conn_list[list_idx];

		chain_next->nid = nid;

		chain->sel = i;

		if(nid == src_nid)
		{
			// path found!!!
			chain_next->idx = hdac_get_node_idx_by_id(src_nid);
			return 1;
		}

		int res = hdac_find_codec_path(src_nid, chain + 1, max_path_len - 1);

		if(res)
		{
			return res + 1;
		}
	}
	return 0;
}
//----------------------------------------------------------------------------------------------------------
void print_audiofunctions_parameters(void)
{
	#if (PRINT_EN & 2)
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		//                               print ADC parameters
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		uint32_t resp = hdal_send_verb_wait_response(hdac_make_verb12(0, 0xf00, 00));  // verb - get amplifier gain
		sprintf(str, "VID:DID = %08x"CRLF, resp);
		uartputs(str);

		for(int ii = 0; ii < codec.FG_node_count;  ii++)
		{
			uint8_t wtype = codec.nodes_param[ii].widget_type;
			if(wtype == 1 || wtype == 2) // "audio in" - 1, "mixer" - 2
			{
				for(int j = 0; j < 207; j++) uartputs("-");
				uartputs(CRLF);
				int nid = codec.nodes_param[ii].nid;
				sprintf(str, "%12s %02x: ", widget_types[wtype], nid);
				uartputs(str);						

				if(wtype == 1)
				{
					uint32_t resp = hdal_send_verb_wait_response(hdac_make_verb4(nid, 0xa, 0)); //  verb - get converter format

					sprintf(str, "%8s, ", stream_types[(resp >> 15) & 1]);
					uartputs(str);

					sprintf(str, "Base freq=%s, ", base_samplerates[(resp >> 14) & 1]);
					uartputs(str);

					sprintf(str, "Mult=%i, ", (int)adc_samplerate_MULT[(resp >> 11) & 7]);
					uartputs(str);

					sprintf(str, "Div=%i, ", ((int)(resp >> 8) & 7) + 1);
					uartputs(str);

					sprintf(str, "B/s=%i, ", (int)adc_bits_per_sample[(resp >> 4) & 7]);
					uartputs(str);

					sprintf(str, "CHNs=%i, ", ((int)(resp >> 0) & 15) + 1);
					uartputs(str);
				}
				
				print_converter_stream(nid);
				// read and print amplifier gain for
				print_amplifier_gain(nid, 0xffff);
				uartputs(CRLF);
			}
		}
	#endif // PRINT_EN
}
//----------------------------------------------------------------------------------------------------------
void print_converter_stream(uint8_t nid)
{
	#if (PRINT_EN & 2)
		int idx;
		uint32_t resp;

		idx = hdac_get_node_idx_by_id(nid);
		if(idx < 0) return;

		resp = hdal_send_verb_wait_response(hdac_make_verb12(codec.nodes_param[idx].nid, 0xf06, 0));  // verb - get converter stream

		sprintf(str, "Stream=%i, ", (int)(resp >> 4) & 15);
		uartputs(str);

		sprintf(str, "Chn=%i ", (int)(resp >> 0) & 15);
		uartputs(str);
	#endif // PRINT_EN
}
//----------------------------------------------------------------------------------------------------------
void print_amplifier_gain(int nid, uint16_t mask)
{
	#if (PRINT_EN & 2) 
		int idx, lr, inout, srcidx, ret;
		int conn_list_start, conn_list_length;
		uint32_t resp;

		idx = hdac_get_node_idx_by_id(nid);
		if(idx < 0) return;
		sprintf(str, "Node %02x (idx=%i), %s, Amplifier gain:"CRLF, nid, idx, widget_types[codec.nodes_param[idx].widget_type]);
		uartputs(str);
		conn_list_start  = codec.nodes_param[idx].conn_list_start;
		conn_list_length = codec.nodes_param[idx].conn_list_length;
		for(srcidx = 0; srcidx < conn_list_length; srcidx++)
		{
			if((mask & 1) != 0)
			{
				int src_nid = g_conn_list[conn_list_start + srcidx];
				sprintf(str, "  %2i: Src_node=%02x :"CRLF, srcidx, src_nid);
				uartputs(str);
				for(inout = 0; inout < 2; inout++)
				{
					sprintf(str, "    %4s: ", inout ? "Out" : "In");
					uartputs(str);
					for(lr = 0; lr < 2; lr++)
					{
						int payload = (inout << 15) | (lr << 13) | (srcidx);
						resp = hdal_send_verb_wait_response(hdac_make_verb4(codec.nodes_param[idx].nid, 0xb, payload));  // verb - get amplifier gain

						sprintf(str, "%5s - %6s, ", lr ? "Left" : "Right", mute_types[(int)(resp >> 7) & 1]);
						uartputs(str);

						sprintf(str, "Gain=%2i; ", (int)(resp >> 0) & 0x7f);
						uartputs(str);

						sprintf(str, "  (resp=%08x)", resp);
						uartputs(str);
					}
					uartputs(CRLF);
				}
			}
			mask >>= 1;
		}
	#endif // PRINT_EN
}
//----------------------------------------------------------------------------------------------------------

samplerate_base_mul_t const *hdac_find_samplerate_base_mul(int samplerate)
{

	for(int i = 0; i < ARRAYSIZE(g_samplerate_base_mul); i++)
	{
		if(g_samplerate_base_mul[i].samplerate == samplerate) return &g_samplerate_base_mul[i];
	}
	return NULL;
}
//----------------------------------------------------------------------------------------------------------
// Шерстим список нод, ищем ЦАП (type = 0: Audio Output) или АЦП (type = 1: Audio Input)
// Для тех, которые нашли, устанавливаем значения base, div и bps.
// 
int hdac_SetAllNodesSbm(samplerate_base_mul_t const *sbm, unsigned int type, int bps)
{
	if(!sbm || type > 1) return 1;
	
	for(int i = 0; i < codec.FG_node_count; i++)
	{
		hda_AfgNode_param_t *pnode = &codec.nodes_param[i];
		if(pnode->widget_type == type && pnode->digital == 0 && pnode->converter_enabled)
		{
			hdac_configure_converter(pnode, sbm->base, sbm->mul, bps);
		}
	}
	return 0;
}
//----------------------------------------------------------------------------------------------------------
int hdac_SetAllAdcSbm(samplerate_base_mul_t const *sbm)
{
	return hdac_SetAllNodesSbm(sbm, 1, HDA_ADC_BITS_PER_SAMPLE);	
}
//----------------------------------------------------------------------------------------------------------
int hdac_SetAllDacSbm(samplerate_base_mul_t const *sbm)
{
	return hdac_SetAllNodesSbm(sbm, 0, HDA_DAC_BITS_PER_SAMPLE);	
}
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
