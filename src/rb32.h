/*
 * Copyright (c) 2026, Victor Agarkov
 *            victoragarkov@gmail.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef RB32_H_INCLUDED
	#define RB32_H_INCLUDED
	
	#include <stdint.h>
	#include <stdlib.h>
	
	#ifndef ARRAYSIZE
		#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
	#endif
	
	typedef struct
	{
		uint32_t* const  buff;
		const     size_t len;
		volatile  size_t wr_ptr;
		volatile  size_t rd_ptr;
	} rb32_t;

	#define RB32_ITEM(name,arr) rb32_t name = {.buff = arr, .len = ARRAYSIZE(arr), .wr_ptr = 0, .rd_ptr = 0};
	

	#define RB32_IS_EMPTY(a) ((a)->wr_ptr == (a)->rd_ptr)

	void     rb32_put_block (rb32_t *rb, uint32_t src);
	int      rb32_try_put   (rb32_t *rb, uint32_t src);
	uint32_t rb32_get_block (rb32_t *rb);
	int      rb32_try_get   (rb32_t *rb, uint32_t *dst);
	
#endif