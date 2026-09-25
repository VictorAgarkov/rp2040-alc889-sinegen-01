/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * 🎧
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "RP2040.h"
#include "rb32.h"
//#include "rb8.h"
#include "hda_link_rp2040.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "hda_codec.h"
#include "data_pack.h"
//#include "codec.h"




#define MIC_CH_NUM 6


uint32_t sine_freq = 1222.2 / 48000 * 0x100000000ULL;
uint32_t sine_phase = 0;

// Прототип функции из шаблона
int32_t get_sine32_linear(uint32_t angle);
void uart_routine(void);
void unsolicided_routine(void);

void add_event(uint32_t val);

int last_nbytes = 0;


uint32_t arr_uart_events[512];
RB32_ITEM(queue_uart_events, arr_uart_events);

volatile uint32_t last_fdebug_err         = 0;

uint32_t          g_HDA_init_state    = 1;
volatile int      g_core1_ready       = 0; // флаг готовности второго ядра (оно сделало все инициализации и теперь крутится в вечном цикле)

char str[256];

uint32_t last_ADC_summ = 0;

//------------------------------------------------------------------------------------------------------------------------------------------------
void nibble2char(char *dst, uint8_t nibble)
{
	*dst = (nibble <= 9) ? nibble + '0' : nibble - 10 + 'a';
}
//------------------------------------------------------------------------------------------------------------------------------------------------
int get_32bit_field(uint32_t val, int msb, int lsb)
{
	int width = msb - lsb + 1;
	int mask = (1U << width) - 1;
	return (val >> lsb) & mask;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
char * get_bit_strz(char *s, uint32_t val, int msb, int lsb, bool valid, char zero)
{
	int b = get_32bit_field(val, msb, lsb);
	int w = msb - lsb + 1;
	int l;

	if(w < 3)
	{
		if(valid)
		{
			if(b) sprintf(s, "%i ", b);
			else
			{
				s[0] = zero;
				s[1] = ' ';
				s[2] = 0;
			}
		}
		else
		{
			memcpy(s, "? ", 3);
		}
		l = 2;
	}
	else
	{
		if(valid)
		{
			if(b) sprintf(s, "%2i ", b);
			else
			{
				s[0] = zero;
				s[1] = zero;
				s[2] = ' ';
				s[3] = 0;
			}
		}
		else
		{
			memcpy(s, " ? ", 3);
		}
		l = 3;
	}
	return s + l;
}
//------------------------------------------------------------------------------------------------------------------------------------------------
char * get_bit_str(char *s, uint32_t val, int msb, int lsb, bool valid)
{
	return get_bit_strz(s, val, msb, lsb, valid, '-');
};
//------------------------------------------------------------------------------------------------------------------------------------------------
void add_event(uint32_t val)
{
	rb32_try_put(&queue_uart_events, val);
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// вся обработка прерываний от DMA, связанным с PIO HDA-Link,
// выполняется на ядре №1.
// обработчик взаимодействует с main через очереди
// queue_verb, queue_resp, queue_unsol
// __attribute__((noinline, section(".time_critical.rb32_functions")))
void core1_routine(void)
{
	
	hdal_enable_hda_dma_irq();

	g_core1_ready = 1; // сообщаем в main об окончании инициализации

	// теперь ядро 1 будет в фоне обрабатывать прерывания
	// от HDA-Link, а в перерывах работать с UART

	for(;;)
	{
		uart_routine();
		unsolicided_routine();

		uint32_t fdb = HDA_pio->fdebug;
		gpio_put(DBGPIN5, fdb != 0);  // set pin, if some bit in FDEBUG rized
		if(fdb)
		{
			last_fdebug_err = fdb;
			HDA_pio->fdebug = fdb; // reset errors
			uartputs("DeSYNC"CRLF);
		}

		uint32_t flevel = HDA_pio->flevel;
		int lvl1 = (flevel >> 0) & 0x0f;
		int lvl2 = (flevel >> 8) & 0x0f;

		//gpio_put(DBGPIN4, lvl1 < 7 || lvl2 < 7);
		SET_PINT(DBGPIN4);
		//__wfi();
	}

}
//------------------------------------------------------------------------------------------------------------------------------------------------
// сюда мы попадаем, когда например срабатывает
// jack sense у нод, где это разрешено
void unsolicided_routine(void)
{
	uint32_t unsol;
	if(rb32_try_get(&queue_unsol, &unsol))
	{
		sprintf(str, "Unsol. resp. 0x%08x"CRLF, unsol);
		uartputs(str);
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// int8_t di_stat[16*32];
// int di_stat_cnt = 0;

// __attribute__((noinline, section(".time_critical.rb32_functions")))
void uart_routine(void)
{
	// UART command parsing
	// реагируем на команды от терминала
	
	extern int print_verb_en;
	extern int last_required_samplerate;
	extern int spk_rb_target_fill;
	
	if(uart_is_readable(UART_ID))
	{
		uint8_t received_char = uart_getc(UART_ID);
		if(received_char == '1')
		{
			uartputs(CRLF);

		}
		else if(received_char == '2')
		{
			// содержимое буфера семплов спикера
			uartputs("----------------"CRLF"Dump g_SamplesOutBuff32[]:");
			for(int i = 0; i < ARRAYSIZE(g_SamplesOutBuff32); i++)
			{
				if(!(i & 7)) uartputs(CRLF);
				uint32_t v = g_SamplesOutBuff32[i];
				if(v)
				{
					sprintf(str, "%08x ", v);
					uartputs(str);
				}
				else uartputs("-------- ");
			}
			uartputs(CRLF);
			
		}
		else if(received_char == '3')
		{
			uartputs("----------------"CRLF"Dump g_SamplesInBuff32[0..16]:");
			for(int i = 0; i < ARRAYSIZE(g_SamplesInBuff32); i++)
			{
				if(!(i & 3)) uartputs(CRLF);
				sprintf(str, "%08x ", g_SamplesInBuff32[i]);
				uartputs(str);
			}
			uartputs(CRLF);
		}
		else if(received_char == '4')
		{
//			// по приёму символа '4' от UART будем считать и выводить статистику об
//			// изменённых битах в потоке, принятом от кодека по SDIN.
//			// Один символ = одно сравнение изменений и одлин раз вывести.
//			// Статичные биты выводятся как '0' или '1', изменённые как '-'.
//			sprintf(str, "--------------------"CRLF"HDA_din_buff[0..16] stat (%i):"CRLF, di_stat_cnt);
//			uartputs(str);
//			for(int i = 0; i < 16; i++)
//			{
//				uint32_t di_val = HDA_din_buff[i];
//				char *ss = str;
//				int8_t *p8 = di_stat + i * 32;
//				sprintf(str, "%2i: ", i);
//				uartputs(str);
//				for(int b = 0; b < 32; b++)
//				{
//					int bit = (di_val >> 31) & 1;
//					if(di_stat_cnt)
//					{
//						if(*p8 != bit) *p8 = '-' - '0';
//					}
//					else
//					{
//						*p8 = bit;
//					}
//
//					*(ss++) = *p8 + '0';
//					if((b & 3) == 3) *(ss++) = ' ';
//					p8++;
//					di_val <<= 1;
//				}
//				*(ss++) = 0;
//				uartputs(str);
//				uartputs(CRLF);
//			}
//			di_stat_cnt++;
//			
//			sprintf(str, "g_InputStreamNum = %08x"CRLF, g_InputStreamNum); uartputs(str);
			
		}
		else if(received_char == '5')
		{
			// uartputs("----------------"CRLF"Dump HDA_din_buff[0..16]:"CRLF);
			// for(int i = 0; i < ARRAYSIZE(HDA_din_buff) / 2 / 4; i++)
			// {
				// volatile uint32_t *p32 = HDA_din_buff + i * 4;
				// sprintf(str, "%2i: 0x%08x, 0x%08x, 0x%08x, 0x%08x,"CRLF, i * 4, p32[0], p32[1], p32[2], p32[3]);
				// uartputs(str);
			// }
		}
		else if(received_char == '6')
		{
			sprintf(str, "last_ADC_summ = %08x"CRLF, last_ADC_summ); uartputs(str);
		}
		else if(received_char == '7')
		{
			uartputs("HDA_dout_buff dump:");
			for(int i = 0; i < ARRAYSIZE(HDA_dout_buff); i++)
			{
				if(!(i & 7)) uartputs(CRLF);
				uint32_t v = HDA_dout_buff[i];
				if(v)
				{
					sprintf(str, "%08x ", v);
					uartputs(str);
				}
				else uartputs("-------- ");
			}
			uartputs(CRLF);
		}
		else if(received_char == '8')
		{
		}
		else if(received_char == '9')
		{

		}
		else if(received_char == '0')
		{
		}
		else if(received_char == 'v') print_verb_en = 0;
		else if(received_char == 'V') print_verb_en = 1;
		else if(received_char == '-')
		{
			uartputs("-------------------"CRLF);
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// делаем очередной семпл синуса для вывода на ЦАПы (10 каналов)
// каждый со своей частотой.
// Каждое значение - в отдельный int32, упаковка в поток для
// кодека будет на следующем этапе
void make_next_sine(int32_t * dst)
{
	static uint32_t gen_phase[10];
	static uint32_t gen_freq[10] =
	{
		// частоту задаём от 0.0  до ~0.49999
		0.004583333333  * 0x100000000ULL,
		0.006875000000  * 0x100000000ULL,
		0.010312500000  * 0x100000000ULL,
		0.015468750000  * 0x100000000ULL,
		0.023203125000  * 0x100000000ULL,
		0.034804687500  * 0x100000000ULL,
		0.052207031250  * 0x100000000ULL,
		0.078310546875  * 0x100000000ULL,
		
		0.333  * 0x100000000ULL,
		0.400  * 0x100000000ULL,
	};

	int32_t v32;

	// проходимся в циклах по:
	// int dac -> 5 стереоканалам (потокам)
	// int ss  -> семплам внутри потока (1 @ 44/48; 2 @ 88/96; 4 @ 176/192kHz)
	// int rl  -> 2 каналам (LR) внутри стереопотока
	for(int dac = 0; dac < codec.path_DAC_conv_count; dac++)  // 1
	{
		for(int ss = 0; ss < codec_spk_SBM->mul; ss++)          // 2
		{
			for(int rl = 0; rl < 2; rl++)     // 3
			{
				int gen_idx = dac * 2 + rl;

				gen_phase[gen_idx] += gen_freq[gen_idx];  // different frequency for each channel
				v32 = get_sine32_linear(gen_phase[gen_idx]);

				*(dst++) = v32;
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------
// анализируем уровень сигнала на входе АЦП.
// если он больше порога - светик мигает.
// если меньше - светик горит постоянно.

void analize_ADC_samples(int32_t* src)
{
	static uint32_t summ = 0;

	int idx_to = codec.path_ADC_conv_count * codec_mic_SBM->mul * 2;
	
	for(int i = 0; i < idx_to; i++)
	{
		int32_t v = src[i] >> 16;
		summ += (v > 0) ? v : -v;
	}
	
	if((g_HDA_frame_count & 0x000007ff) == 0)
	{
		int led = 1;
		if(g_HDA_frame_count & 0x00000800)
		{
			if(summ > 0x00100000) led = 0;
			last_ADC_summ = summ;
			summ = 0;
		}

		// зажигаем/тушим светик
		gpio_put(LEDPIN, led);	
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------

int main()
{
	/*****************************************
	              System clock & all
	*****************************************/
	set_sys_clock_pll(24000000 * LINK_BCLK_LEN*2 * 6, 3, 2);
	stdio_init_all();

	// ХАК ДЛЯ ОТЛАДКИ: Разрешаем таймеру бежать во время брейкпоинтов
	timer_hw->dbgpause = 0;

	/*****************************************
	                   Debug pins
	*****************************************/
	// настраиваем  OUT PIN
	uint out_pins_idx[] = {DBGPIN1, DBGPIN2, DBGPIN3, DBGPIN4, DBGPIN5, PIN_HDA_RST, LEDPIN};
	uint out_pins_val[] = {      0,       0,       0,       0,       0,           1,      0};
	for(int i = 0; i < ARRAYSIZE(out_pins_idx); i++)
	{
		gpio_init(out_pins_idx[i]);
		gpio_set_dir(out_pins_idx[i], GPIO_OUT);
		gpio_put(out_pins_idx[i], out_pins_val[i]);
	}

	/*****************************************
	                   UART
	*****************************************/
	uart_init(UART_ID, BAUD_RATE); // Настройка порта UART
	// Назначение пинов на функции UART
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
	while(uart_is_readable(UART_ID)) (void)uart_getc(UART_ID); // опустошаем приёмник
	uartputs("--\r\n");

	hdal_dma_init();

	/*****************************************
	                Core #1 start
	*****************************************/
	// print_SP("Core0");

	multicore_reset_core1();
	sleep_ms(10);
	multicore_launch_core1(core1_routine); //запускаем второе ядро
	while(!g_core1_ready); // ждём, пока второе ядро инициализируется
	uartputs("- core 1 start -"CRLF);


	/*****************************************
	          Start HDA-Link PIO & DMA
	*****************************************/
	sleep_ms(30); // power-on pause
	hdal_dma_start();	
	if(hdal_codec_reset()) uartputs("CAD"CRLF);

	// кодеку назначен адрес - дальше запускаем рабочую рутину
	g_HDA_init_state = 0; // процедура сброс окончена - можно начинать выводить полезный сигнал
	uartputs("<");
	hda_init();
	uartputs(">"CRLF);

	hdal_release_sm_CAD(); // release unused SM

	// пересоздаём буфер SYNC сигнала с учётом номеров и размеров аудиопоков ЦАП
	int DAC_stream_count = HDA_SINGLE_STREAM ? 1 : codec.path_DAC_conv_count;
	hdal_update_HDA_TX_buff(HDA_sync_buff_actual,  codec.stream_DAC_start, DAC_stream_count, 6 * codec_spk_SBM->mul);
	hdal_update_dma_tx_param_by_spk_sbm();

	/*****************************************
	                 audio init
	*****************************************/
	samplerate_base_mul_t const *sbm = hdac_find_samplerate_base_mul(HDA_SAMPLERATE);
	if(sbm)
	{
		codec_mic_SBM = codec_spk_SBM = sbm;
		hdal_update_dma_tx_param_by_spk_sbm(); // задаём параметры передачи DMA
		hdac_SetAllDacSbm(sbm);  // заводим ЦАПы кодека на нужной частоте
		hdac_SetAllAdcSbm(sbm);  // заводим АЦПы кодека на нужной частоте
		// инициируем нужную для этой частоты/кол-ва каналов синхропоследовательность
		hdal_update_HDA_TX_buff(HDA_sync_buff_actual,  codec.stream_DAC_start, DAC_stream_count, 6 * codec_spk_SBM->mul);
	}


	/*****************************************
	                Main loop
	*****************************************/
    for (uint32_t cc = 0;; cc++)
	{
		int32_t current_time_ms = to_ms_since_boot(get_absolute_time());

		if(g_SamplesOutBuff32_empty)
		{
			// буфер ЦАП пуст - готовим очередную порцию семплов
			int32_t *dst = (int32_t*)g_SamplesOutBuff32_empty;
			g_SamplesOutBuff32_empty = NULL;

			// генерим выход
			make_next_sine(dst); 
		}
		
		if(g_SamplesInBuff32_ready)
		{
			// здесь нужно обработать принятые от АЦП данные
			int32_t *src = (int32_t*)g_SamplesInBuff32_ready;
			g_SamplesInBuff32_ready = NULL;
			analize_ADC_samples(src);			
		}

		//SET_PINT(DBGPIN4); // debug pin flip-flop
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------
