/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CONFIG_H_INCLUDED
	#define CONFIG_H_INCLUDED
	
	#define HDA_DAC_NUM    5      // number of stereo DAC
	#define HDA_ADC_NUM    3      // number of stereo ADC
	#define HDA_SAMPLERATE 192000 // working samplerate, Hz: 44100, 48000, 88200, 96000, 176400, 192000
	
	#define UART_ID uart1
	#define BAUD_RATE 115200
	#define UART_TX_PIN 20
	#define UART_RX_PIN 21
	#define CRLF "\r\n"
	#define uartputs(s) uart_puts(UART_ID, s)



	#ifndef ARRAYSIZE
		#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
	#endif
	
	#ifndef min
		#define min(a,b) (a) < (b) ? (a) : (b)
	#endif
	
	
#endif //CONFIG_H_INCLUDED