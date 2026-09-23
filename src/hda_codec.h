/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HDA_H_INCLUDED
#define HDA_H_INCLUDED

	#include "codec.h"
	#include "config.h"
	


	extern codec_capabilities_t codec;
	extern uint8_t g_conn_list[160]; // global connection list
	extern const uint8_t adc_bits_per_sample[8];



	typedef struct
	{
		uint8_t nid; // Node ID in path
		int8_t  sel; // selector value, if necessary
		int8_t  idx; // Node index in big array
		int8_t  res; // align to 32 bit
	}nid_path_t;

	typedef struct
	{
		int    samplerate; // samplerate, Hz
		int    base;       // base samplerate: 0 - 48 kHz, 1 - 44.1 kHz
		int    mul;        // mult. factor for codec
	}samplerate_base_mul_t;



	#define   hda_init() hdac_init_startcount(1,6)

	void      hdac_init_startcount(int start_adc, int start_dac);
	
	//void      hda_init(void);	
	int       hdac_read_codec_property();
	uint32_t  hdac_make_verb4(uint32_t node_id, uint32_t verb_id, uint32_t payload);
	uint32_t  hdac_make_verb12(uint32_t node_id, uint32_t verb_id, uint32_t payload);
	int       hdac_codec_start();
	void      hdac_codec_stop();
	int       hdac_get_node_idx_by_id(int node_id);
	int       hdac_find_node_idx_in_conn_list(int selector_nid, int src_nid);
	int       hdac_find_codec_path(int src_nid, nid_path_t *chain, int max_path_len);
	//int       hda_try_make_path(int src_id, int dst_id, nid_path_t *chain);
	
	const     samplerate_base_mul_t *hdac_find_samplerate_base_mul(int samplerate);
	int       hdac_SetAllAdcSbm(samplerate_base_mul_t const *sbm);
	int       hdac_SetAllDacSbm(samplerate_base_mul_t const *sbm);
	int       hdac_SetAllNodesSbm(samplerate_base_mul_t const *sbm, unsigned int type, int bps);
	
	
	void      print_audiofunctions_parameters(void);
	void      print_converter_stream(uint8_t nid);
	void      print_amplifier_gain(int nid, uint16_t mask);
	
	extern const samplerate_base_mul_t g_samplerate_base_mul[6];
	extern volatile const samplerate_base_mul_t  *codec_spk_SBM;
	extern volatile const samplerate_base_mul_t  *codec_mic_SBM;
	

#endif /* HDA_H_INCLUDED */
