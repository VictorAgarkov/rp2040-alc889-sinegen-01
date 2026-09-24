/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
 
#ifndef _HDA_LINK_RP2040_H_INCLUDED
	#define _HDA_LINK_RP2040_H_INCLUDED
	
	

	#include "hardware/dma.h"
	#include "hardware/pio.h" 
	#include "hda.pio.h"
	#include "codec.h"
	#include "rb32.h"

	#define DMA_TXBUFF_1ST_IDX 0
	#define DMA_RXBUFF_1ST_IDX 4
	
	#define DMA_DBGPIN1 0
	#define DMA_DBGPIN2 1
	#define DMA_DBGPIN3 2
	

	int  hdal_codec_reset(void);
	void hdal_dma_init(void);
	void hdal_dma_start(void);
	
	dma_channel_hw_t * hdal_setup_DMA_tx(PIO pio, uint sm, int dma_chn, int dma_chn_next, void *buff, uint buff_size);
	dma_channel_hw_t * hdal_setup_DMA_rx(PIO pio, uint sm, int dma_chn, int dma_chn_next, void *buff, uint buff_size);
	void hdal_dma_set_DMAs_read_addr(int half, void *p_dma_data, void *p_dma_sync);
	void hdal_update_HDA_TX_buff(uint32_t *dst32, int start_stream, int stream_num, int bytes_per_stream);
	void hdal_update_dma_tx_param_by_spk_sbm(void);
	
	
	void     hdal_send_verb(uint32_t verb);
	uint32_t hdal_wait_response(void);
	uint32_t hdal_send_verb_wait_response(uint32_t verb);
	
	
	void dma_irq1_handler(void);
	void hdal_enable_hda_dma_irq(void);
	
	extern PIO HDA_pio;
	extern uint fsm_CAD;
	
	extern void *dmatx_dout_p[2];
	extern void *dmarx_din_p [2];
	
	extern volatile uint32_t HDA_din_buff [16 * 2];
	extern uint32_t          HDA_dout_buff[32 * 2];
	extern uint32_t          HDA_sync_buff_actual[32];
	extern uint32_t          HDA_sync_buff_empty [32];
	extern uint32_t          HDA_dout_buff_empty [32];
	extern volatile uint32_t g_HDA_frame_count;

	extern volatile int32_t  g_SamplesInBuff32 [HDA_MAX_SAMPLES_PER_FRAME * 2 * HDA_ADC_NUM * 2];
	extern int32_t           g_SamplesOutBuff32[HDA_MAX_SAMPLES_PER_FRAME * 2 * HDA_DAC_NUM * 2];
						     
	extern int32_t* volatile g_SamplesOutBuff32_empty;  // какую часть буфера нужно заполнить свежими сеплами

	extern volatile int      g_InputStreamNum;
	extern int32_t* volatile g_SamplesInBuff32_ready;    // какую часть буфера микрофона можно обрабатывать

						     
	extern rb32_t            queue_verb; 
	extern rb32_t            queue_resp; 
	extern rb32_t            queue_unsol;

#endif // _HDA_LINK_RP2040_H_INCLUDED
