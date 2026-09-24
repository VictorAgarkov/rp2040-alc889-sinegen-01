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

	#define LEDPIN    PICO_DEFAULT_LED_PIN
	#define DBGPIN1   13  // 
	#define DBGPIN2   14  // 
	#define DBGPIN3   15  // 
	#define DBGPIN4   16  // 
	#define DBGPIN5   17  // 


	#ifndef ARRAYSIZE
		#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
	#endif
	
	#ifndef min
		#define min(a,b) (a) < (b) ? (a) : (b)
	#endif
	
	#if 1
		#define SET_PIN0(pin) gpio_put(pin, 0)
		#define SET_PIN1(pin) gpio_put(pin, 1)
		#define SET_PINT(pin) sio_hw->gpio_togl = (1 << pin)
	#else
		#define SET_PIN0(pin)
		#define SET_PIN1(pin)
		#define SET_PINT(pin)
	#endif
	
	
#endif //CONFIG_H_INCLUDED
