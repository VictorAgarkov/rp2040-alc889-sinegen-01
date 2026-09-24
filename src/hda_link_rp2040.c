/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include "hda_codec.h"
#include "hda_link_rp2040.h"
#include "hardware/structs/busctrl.h"
#include "data_pack.h"


// для 500  бит нужно 16 слов  uint32
// для 1000 бит нужно 32 слова uint32
// делаем буфера двойной длины для качелей
// выравниваем для DMA ring
volatile uint32_t HDA_din_buff [16 * 2]        __attribute__((aligned(16 * 4)));
uint32_t          HDA_dout_buff[32 * 2]        __attribute__((aligned(32 * 4)));
uint32_t          HDA_sync_buff_actual[32]     __attribute__((aligned(32 * 4)));
uint32_t          HDA_sync_buff_empty [32]     __attribute__((aligned(32 * 4)));
uint32_t          HDA_dout_buff_empty [32]     __attribute__((aligned(32 * 4)));

void *dmatx_dout_p[2];
void *dmarx_din_p [2];

// базовые адреса каналов DMA
dma_channel_hw_t *p_DMA_HW_tx_data[2];
dma_channel_hw_t *p_DMA_HW_tx_sync[2];
dma_channel_hw_t *p_DMA_HW_rx_data[2];

PIO HDA_pio = pio0;
uint mask_fsm_sync = 0;  // маска - какие SM будем запускать одновременно
uint fsm_CAD;
volatile uint32_t g_HDA_frame_count   = 0;

extern int g_HDA_init_state;

void hdal_update_TX_buff(int half);
void hdal_update_RX_buff(int half);

// очереди запросов (verb) и ответов (response, unsol)
uint32_t arr_verb[32]  __attribute__((section(".scratch_x.my_buffers")));
uint32_t arr_resp[32]  __attribute__((section(".scratch_x.my_buffers")));
uint32_t arr_unsol[16] __attribute__((section(".scratch_x.my_buffers")));

volatile uint32_t verb_cnt = 0, resp_cnt = 0;

RB32_ITEM(queue_verb  __attribute__((section(".scratch_x.my_buffers"))), arr_verb);
RB32_ITEM(queue_resp  __attribute__((section(".scratch_x.my_buffers"))), arr_resp);
RB32_ITEM(queue_unsol __attribute__((section(".scratch_x.my_buffers"))), arr_unsol);

volatile int32_t  g_SamplesInBuff32 [HDA_MAX_SAMPLES_PER_FRAME * 2 * HDA_ADC_NUM * 2];
volatile int      g_SamplesInBuff32_half = 0;   // с какой половиной буфера семплов будем работать
int32_t* volatile g_SamplesInBuff32_ready = NULL; // какую часть буфера микрофона можно обрабатывать
volatile int      g_InputStreamNum;      // сколько входных потоков было выделено из потока SDI

int32_t           g_SamplesOutBuff32[HDA_MAX_SAMPLES_PER_FRAME * 2 * HDA_DAC_NUM * 2];
volatile int      g_SamplesOutBuff32_half = 0;      
int32_t* volatile g_SamplesOutBuff32_empty = NULL;    // какую часть буфера нужно заполнить свежими сеплами

volatile int      g_pass147_acc = 159;                // накопитель до 160, для режимов 44.1кГц будем отдавать 147 кадров из 160
volatile int      g_pass147_add = 0;                  // сколько будем добавлять к накопителю - 147 или 160

static p_sdo_pack_24_proc current_sdo_pack_24_proc = NULL; // процедура упаковки 32-семплов + verb в 24-бит для передачи по HDA-Link

int g_CodecAddressRequested = 0; // устанавливаем в 1, если кодек при инициализации заспросил и получил адрес


//----------------------------------------------------------------------------------------------------------
int hdal_codec_reset(void)
{
	// wait HDA codec ready
	for (int loop_en = 1; loop_en;)
	{
		void set_codec_reset_pin(int val);
		switch(g_HDA_frame_count)
		{
			case  2: gpio_put(PIN_HDA_RST, 0); break; // (+41 μs)  RST pin active
			case  8: gpio_put(PIN_HDA_RST, 1); break; // (+125 μs) RST pin deactive
			case 10:
				//  (+41 μs) заполняем буфера SYNC, пока без полезной нагрузки
				hdal_update_HDA_TX_buff(HDA_sync_buff_empty,  0,0,0);
				hdal_update_HDA_TX_buff(HDA_sync_buff_actual, 0,0,0);
			break;
			case 202: loop_en = 0; break;  // (+4ms) timeout - exit loop
			default :
				// ждём от SM признака, что кодек запросил свой адрес и он был назначен
				if(pio_sm_get_rx_fifo_level(HDA_pio, fsm_CAD))
				{
					g_CodecAddressRequested = 1;
					loop_en = 0;
				}
			break;
		}
	}	
	return g_CodecAddressRequested;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_dma_init(void)
{

	/*****************************************
	     подготовка и запуск PIO HDA-Link
		 SM0 - DOUT (tx)
		 SM1 - SYNC (tx)
		 SM2 - DIN  (rx) + end_of_frame (48kHz)
		 SM3 - CAD  (codec address pulse)
	*****************************************/
	

	// настраиваем 2 копии out_1000bit_program на 2 FSM
	uint fsm_1000[2];
	uint data_pin1000[2] = {PIN_HDA_DOUT, PIN_HDA_SYNC}; // 6, 7

	uint offset_1000 = pio_add_program(HDA_pio, &out_1000bit_program);
	for(int i = 0; i < 2; i++)
	{
		fsm_1000[i] = pio_claim_unused_sm(HDA_pio, true);
		// out_1000bit_program_init(HDA_pio, fsm_1000[i], offset_1000, clk_pin1000[i], data_pin1000[i]);
		out_1000bit_program_init(HDA_pio, fsm_1000[i], offset_1000, data_pin1000[i]);
		// длину задаём L = 1000 / 2 - 2 = 248, её передаём в fifo
		//pio_sm_put_blocking(pio, fsm_1000[i], (32+20) / 2 - 2);
		pio_sm_put_blocking(HDA_pio, fsm_1000[i], (1000) / 2 - 2);
		mask_fsm_sync |= 1 << fsm_1000[i];
	}

	// настраиваем 1 копию in_500bit
	uint fsm_500;
	uint offset_500 = pio_add_program(HDA_pio, &in_500bit_program);
	fsm_500 = pio_claim_unused_sm(HDA_pio, true);
	in_500bit_program_init(HDA_pio, fsm_500, offset_500);
	pio_sm_put_blocking(HDA_pio, fsm_500, 500  - 2);
	mask_fsm_sync |= 1 << fsm_500;

	// настраиваем програму codec_address
	uint offset_CAD = pio_add_program(HDA_pio, &codec_address_program);
	fsm_CAD    = pio_claim_unused_sm(HDA_pio, true);
	codec_address_program_init(HDA_pio, fsm_CAD, offset_CAD);
	// 3953 @ 4,    2963 @ 3
	pio_sm_put_blocking(HDA_pio, fsm_CAD, LINK_BCLK_LEN * (1000 - 10) - 7);
	mask_fsm_sync |= 1 << fsm_CAD;

	//sprintf(str, "PIO SM: TX_1000={%i, %i}, RX_500=%i, CAD=%i"CRLF, fsm_1000[0], fsm_1000[1], fsm_500, fsm_CAD);
	//uartputs(str);


	/*****************************************
	                HDA-Link DMA
	*****************************************/
	memset(HDA_dout_buff,        0, sizeof(HDA_dout_buff));
	memset(HDA_sync_buff_actual, 0, sizeof(HDA_sync_buff_actual));
	memset(HDA_sync_buff_empty,  0, sizeof(HDA_sync_buff_empty));
	memset(HDA_dout_buff_empty,  0, sizeof(HDA_dout_buff_empty));

	for(int i = 0; i < 2; i++)
	{
		// настраиваем каналы DMA таким образом:
		// каналы 0/1 - HDA_dout_buff, первая и вторая половинки
		// каналы 2/3 - HDA_sync_buff, первая и вторая половинки (содержимое статично, поэтому используется только одна половинка)
		// каналы 4/5 - HDA_din_buff,  первая и вторая половинки
		dmatx_dout_p[i] =         HDA_dout_buff + i * 32;
		dmarx_din_p [i] = (void*)(HDA_din_buff  + i * 16);

		int chn_tx_data = DMA_TXBUFF_1ST_IDX + i;
		int chn_tx_sync = chn_tx_data + 2;
		int chn_rx_data = DMA_RXBUFF_1ST_IDX + i;

		p_DMA_HW_tx_data[i] = hdal_setup_DMA_tx(HDA_pio, fsm_1000[0], chn_tx_data, chn_tx_data^1, dmatx_dout_p[i],          32);
		p_DMA_HW_tx_sync[i] = hdal_setup_DMA_tx(HDA_pio, fsm_1000[1], chn_tx_sync, chn_tx_sync^1, HDA_sync_buff_actual,     32);
		p_DMA_HW_rx_data[i] = hdal_setup_DMA_rx(HDA_pio, fsm_500,     chn_rx_data, chn_rx_data^1, dmarx_din_p[i],           16);
	}

	// set high priority to DMA
	busctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
	
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_dma_start(void)
{
	dma_start_channel_mask((1 << (DMA_TXBUFF_1ST_IDX + 0)) | (1 << (DMA_TXBUFF_1ST_IDX + 2)) | (1 << (DMA_RXBUFF_1ST_IDX + 0)) | 0);
	pio_set_sm_mask_enabled(HDA_pio, mask_fsm_sync, 1); // запускаем все машины одновременно (начинается генерация BCLK)
}
//------------------------------------------------------------------------------------------------------------------------------------------------
dma_channel_hw_t * hdal_setup_DMA_tx(PIO pio, uint sm, int dma_chn, int dma_chn_next, void *buff, uint buff_size)
{
	// --- НАСТРОЙКА КАНАЛА ---
	dma_channel_config config = dma_channel_get_default_config(dma_chn);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_32); // Передаем по 32 бита
	channel_config_set_read_increment(&config, true);            // Буфер памяти сдвигаем вперед
	channel_config_set_write_increment(&config, false);          // Адрес PIO FIFO неподвижен
	// Главная магия: по окончании работы А автоматически запускает Б
	channel_config_set_chain_to(&config, dma_chn_next);
	// Синхронизация: DMA передает слово только тогда, когда в FIFO PIO есть место
	channel_config_set_dreq(&config, pio_get_dreq(pio, sm, true)); // TX
	//channel_config_set_high_priority(&config, true);
	channel_config_set_ring (&config, 0, 5+2); // ring buff 32 words on read address

	// настраиваем, но не стартуем
	dma_channel_hw_t *dma_p = dma_channel_hw_addr(dma_chn);
	dma_p->read_addr      = (uintptr_t) buff;
	dma_p->write_addr     = (uintptr_t) &pio->txf[sm];
	dma_p->transfer_count = buff_size;
	dma_p->al1_ctrl       = channel_config_get_ctrl_value(&config);

	return dma_p;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
dma_channel_hw_t * hdal_setup_DMA_rx(PIO pio, uint sm, int dma_chn, int dma_chn_next, void *buff, uint buff_size)
{
	// --- НАСТРОЙКА КАНАЛА ---
	dma_channel_config config = dma_channel_get_default_config(dma_chn);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_32); // Передаем по 32 бита
	channel_config_set_read_increment(&config, false);           // Адрес PIO FIFO неподвижен
	channel_config_set_write_increment(&config, true);           // Буфер памяти сдвигаем вперед
	// Главная магия: по окончании работы А автоматически запускает Б
	channel_config_set_chain_to(&config, dma_chn_next);
	// Синхронизация: DMA передает слово только тогда, когда в FIFO PIO есть место
	channel_config_set_dreq(&config, pio_get_dreq(pio, sm, false));  // RX
	//channel_config_set_high_priority(&config, true);

	// МАНЕВР: Разрешаем генерировать IRQ при завершении,
	// даже если управление передается другому каналу по chain_to
	channel_config_set_irq_quiet(&config, false);
	channel_config_set_ring (&config, 1, 4+2); // ring buff 16 words on write address

	// настраиваем, но не стартуем
	dma_channel_hw_t *dma_p = dma_channel_hw_addr(dma_chn);
	dma_p->write_addr       = (uintptr_t) buff;
	dma_p->read_addr        = (uintptr_t) &pio->rxf[sm];
	dma_p->transfer_count   = buff_size;
	dma_p->al1_ctrl         = channel_config_get_ctrl_value(&config);

	// разрешаем прерывания от DMA
	// К моменту приёма фрейма от RX фрейм TX уже
	// гарантировано передан, поэтому обойдёмся одним прерыванием
	dma_channel_set_irq1_enabled(dma_chn, true);

	return dma_p;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void dma_irq1_handler(void)
{
	gpio_put(DMA_DBGPIN1, 1);
	int half = -1;

	// находим, канал, который нас вызвал, и обрабатываем его
	for(int i = 0; i < 2; i++)
	{
		int chn_idx = DMA_RXBUFF_1ST_IDX + i;
		int chn_mask = 1 << chn_idx;

		if (dma_hw->ints1 & chn_mask)
		{
			// Очищаем флаг прерывания для канала
			dma_hw->ints1 = chn_mask;
			// если в нормальном режиме - подготавливаем сигнал на вывод
			if(!g_HDA_init_state) half = i;
		}
	}
	// подготавливаем следующую порцию для
	// передачи и разгребаем принятое
	if(half >= 0)
	{
		//gpio_put(DMA_DBGPIN2, 1);
		hdal_update_TX_buff(half);
		//gpio_put(DMA_DBGPIN2, 0);

		gpio_put(DMA_DBGPIN3, 1);
		hdal_update_RX_buff(half);
		gpio_put(DMA_DBGPIN3, 0);

	}

	// увеличиваем счётчик фреймов
	g_HDA_frame_count++;

	gpio_put(DMA_DBGPIN1, 0);

//	if(!half)
//	{
//		// для второй половины делаем двойной импульс
//		asm volatile ("nop\n nop\n nop\n nop\n");
//		gpio_put(DMA_DBGPIN1, 1);
//		asm volatile ("nop\n nop\n nop\n nop\n");
//		gpio_put(DMA_DBGPIN1, 0);
//	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_dma_set_DMAs_read_addr(int half, void *p_dma_data, void *p_dma_sync)
{
	if(p_DMA_HW_tx_data[half]) p_DMA_HW_tx_data[half]->read_addr = (uintptr_t)p_dma_data;
	if(p_DMA_HW_tx_sync[half]) p_DMA_HW_tx_sync[half]->read_addr = (uintptr_t)p_dma_sync;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_verb_to_queue(uint32_t verb32)
{
	rb32_put_block(&queue_verb, verb32);
	verb_cnt++;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t hdal_wait_codec_resp(void)
{
	uint32_t resp = rb32_get_block(&queue_resp);
	return resp;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// ждём, пока
// 1. очистится очередь запросов к кодеку
// 2. кол-во запросов и ответов не сравняется,
// затем "очищаем" очередь ответов
void hdal_wait_queues_emty(void)
{
	while (!RB32_IS_EMPTY(&queue_verb));
	while (verb_cnt != resp_cnt);
	queue_resp.rd_ptr = queue_resp.wr_ptr;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_send_verb(uint32_t verb)
{
	hdal_verb_to_queue(verb);
}
//------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t hdal_wait_response(void)
{
	return hdal_wait_codec_resp();	
}
//------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t hdal_send_verb_wait_response(uint32_t verb)
{
	hdal_send_verb(verb);
	return hdal_wait_response();	
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// Подготавливаем половинку DMA буфера SDO
// half:
// 0 - первая половина HDA_dout_buff
// 1 - вторая половина HDA_dout_buff
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void hdal_update_TX_buff(int half)
{
	half = half & 1; //принудительно приводим half к 0 или 1

	// собираем буфер для отправки
	uint32_t verb32 = 0;
	rb32_try_get(&queue_verb, &verb32);
	int buff_half = g_SamplesOutBuff32_half;
	uint32_t *src = g_SamplesOutBuff32 + ARRAYSIZE(g_SamplesOutBuff32) / 2 * buff_half;

	uint32_t *p_dma_data, *p_dma_sync;

	// если семплрейт от 48кГц, заполняем буфер каждый раз.
	// если от 44.1кГц, заполняем буфер каждый 147-й раз из 160,
	// оставшиеся 13 отправляем пустые фреймы (а-ля Брезенхэм)

	g_pass147_acc += g_pass147_add; // +147 or +160
	if(g_pass147_acc >= 160)
	{
		// упаковываем данные из 32-битных отсчётов в 24-битные
		// значения для передачи кодеку
		g_pass147_acc -= 160;
		p_dma_data = dmatx_dout_p[half];
		p_dma_sync = HDA_sync_buff_actual;

		// пакуем данные и verb для передачи по HDA link
		if(current_sdo_pack_24_proc) current_sdo_pack_24_proc(verb32, p_dma_data, src);

		g_SamplesOutBuff32_empty = src; // говорим main, что можно обновить использованную половину программного буфера
		g_SamplesOutBuff32_half    = buff_half ^ 1;  // переключаемся на другую половинку качелей программного буфера
	}
	else
	{
		// отправляем пустой фрейм
		p_dma_data = HDA_dout_buff_empty;
		p_dma_sync = HDA_sync_buff_empty;
		SDO_PACK_VERB32(verb32, p_dma_data); // даже для пустого буфера делаем verb
	}

	// задаём адреса буферов источников в соотв. каналах DMA
	hdal_dma_set_DMAs_read_addr(half, p_dma_data, p_dma_sync);

}
//------------------------------------------------------------------------------------------------------------------------------------------------
// обрабатываем принятые от кодека данные
// half:
// 0 - первая половина HDA_din_buff
// 1 - вторая половина HDA_din_buff
__attribute__((noinline, section(".scratch_x.rb32_functions")))
void hdal_update_RX_buff(int half)
{
	uint32_t *data_p = dmarx_din_p[half];
	// смотрим, есть ли сообщение от кодека
	if(data_p[0] &0x80000000)
	{
		rb32_t *rb = (data_p[0] &0x40000000) ? &queue_unsol : &queue_resp;

		// комбинируем ответ
		uint32_t resp32 = (data_p[0] << 4) | (data_p[1] >> 28);
		// записываем ответ в очередь. Если очередь переполнена, ответ теряется
		rb32_try_put(rb, resp32);

		if ((data_p[0] &0x40000000) == 0)  resp_cnt++;
	}

	int samples_half = g_SamplesInBuff32_half; 
	int32_t *dst = (int32_t*)g_SamplesInBuff32 + ARRAYSIZE(g_SamplesInBuff32) / 2 * samples_half;
	

	// разгребаем аудиоданные
	int stream_cnt = 0;
	#if 0 // 1 - универсальный медленный разгребатель, 0 - на выбор узкоспециализированный рагребатель
		int bit = 36;
		
		int bit_p_sample = adc_bits_per_sample[HDA_ADC_BITS_PER_SAMPLE];
		int dst_offset = 0;
		for(;;)
		{
			int stream_ID = bit32_msb_arigth(data_p, bit, 4); bit += 4;
			if(!stream_ID) break;
			int bytes_len = bit32_msb_arigth(data_p, bit, 6); bit += 6;
			if(!bytes_len) break;

			int samples_num = bytes_len * 8 / bit_p_sample;

			g_InputStreamTags[stream_cnt].stream_id   = stream_ID;
			g_InputStreamTags[stream_cnt].samples_num = samples_num;
			g_InputStreamTags[stream_cnt].offset      = dst_offset;

			for(int s = 0; s < samples_num; s++)
			{
				dst[dst_offset++] = bit32_msb_aleft(data_p, bit + s * bit_p_sample,  bit_p_sample);
			}
			bit += bytes_len * 8;
			stream_cnt++;
		}

		
		
	#else
		#if   (HDA_ADC_BITS_PER_SAMPLE == HDA_BPS_24)
			if      (codec_mic_SBM->mul == 1) stream_cnt = sdi_extract_24bit_1x(dst, data_p);
			else if (codec_mic_SBM->mul == 2) stream_cnt = sdi_extract_24bit_2x(dst, data_p);
			else if (codec_mic_SBM->mul == 4) stream_cnt = sdi_extract_24bit_4x(dst, data_p);
			else                              stream_cnt = HDABUF_WRONG_SPF; //0x80000000
		#elif (HDA_ADC_BITS_PER_SAMPLE == HDA_BPS_16)
			if      (codec_mic_SBM->mul == 1) stream_cnt = sdi_extract_16bit_1x(dst, data_p);
			else if (codec_mic_SBM->mul == 2) stream_cnt = sdi_extract_16bit_2x(dst, data_p);
			else if (codec_mic_SBM->mul == 4) stream_cnt = sdi_extract_16bit_4x(dst, data_p);
			else                              stream_cnt = HDABUF_WRONG_SPF; //0x80000000
		#else
			#error "HDA_ADC_BITS_PER_SAMPLE may be HDA_BPS_16 or HDA_BPS_24"
		#endif
	#endif
	
	g_InputStreamNum = stream_cnt;
	if(stream_cnt & 0xff)
	{
		g_SamplesInBuff32_half = samples_half ^ 1;
		g_SamplesInBuff32_ready = dst;
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_update_HDA_TX_buff(uint32_t *dst32, int start_stream, int stream_num, int bytes_per_stream)
{
	uint8_t  *sync_p8  = (uint8_t*)dst32;
	memset(dst32, 0, 32*4);  // zeroes to sync
	dst32[31] = 0xffffffff;  // last 8 bits of frame - SYNC sequence

	int offs = 4;  // первые 40 бит (5 байт) - command stream. За 8 бит до конца нужно выдать ID первого потока.

	for(int i = 0; i < stream_num; i++)
	{
		if(offs >= 124) break;  //
		sync_p8[offs ^ 3] = 0xe0 | start_stream++;
		offs += bytes_per_stream;
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
void hdal_update_dma_tx_param_by_spk_sbm(void)
{
	samplerate_base_mul_t const *sbm = (samplerate_base_mul_t const *)codec_spk_SBM;
	
	g_pass147_add = sbm->base ? 147 : 160;
	
	unsigned int idx = sbm->mul - 1;
	if(idx < ARRAYSIZE(sdo_pack_24_procs))
	{
		// _x1, _x2 or _x4
		current_sdo_pack_24_proc = sdo_pack_24_procs[idx];	
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// разрешаем прерывания DMA IRQ от HDA_link PIO
// Вызывать нужно из того ядра, ктоторе будет их обрабатывать
void hdal_enable_hda_dma_irq(void)
{
	int dma_irq = DMA_IRQ_1;
	// Регистрируем обработчики в NVIC процессора
	irq_set_exclusive_handler(dma_irq, dma_irq1_handler);

	// Включаем прерывания
	irq_set_enabled(dma_irq, true);
}
//------------------------------------------------------------------------------------------------------------------------------------------------
