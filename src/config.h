/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CONFIG_H_INCLUDED
	#define CONFIG_H_INCLUDED
	
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