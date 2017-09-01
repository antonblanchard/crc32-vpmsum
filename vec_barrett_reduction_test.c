/*
 * Test POWER8 Barrett reduction algorithms.
 *
 * Copyright (C) 2015 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of either:
 *
 *  a) the GNU General Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your option)
 *     any later version, or
 *  b) the Apache License, Version 2.0
 */
#include <stdio.h>
#include <string.h>
#include "crcmodel.h"

/* CRC without the top bit */
#define CRC	0x04C11DB7

char crc_data[] __attribute__((aligned(4))) = { 0x9e, 0xd3, 0x20, 0xcc, };
/*
 * The CRC appends 32bits of 0s to the end of the message, so replicate
 * that for our Barrett reduction.
 */
char data[] __attribute__((aligned(8))) = { 0x9e, 0xd3, 0x20, 0xcc, 0, 0,
											0, 0, };

static void doit(p_cm_t p_cm, char *str)
{
	int i;

	for (i = 0; i < sizeof(crc_data)/sizeof(crc_data[0]); i++)
		cm_nxt(p_cm, crc_data[i]);

	printf("%-25s = 0x%08lx\n", str, cm_crc(p_cm));
}

#ifdef __powerpc__

unsigned long __attribute__ ((aligned (32)))
barrett_reduction(unsigned long data);

unsigned long __attribute__ ((aligned (32)))
barrett_reduction_reflected(unsigned long data);

static void do_barrett(void)
{
	unsigned long val;
	unsigned long crc;

	printf("Barrett reduction\n");

	memcpy(&val, data, sizeof(data));
#ifdef __LITTLE_ENDIAN__
	val = __builtin_bswap64(val);
#endif
	crc = barrett_reduction(val);
	printf("%-25s = 0x%08lx\n", "Base", crc);

	crc = barrett_reduction(val ^ 0xffffffff00000000UL);
	/* Since it returns a 64 bit integrer mask the result as it retuns
		0xffffffff on the upper bits.
	 */
	printf("%-25s = 0x%08lx\n", "Inverted", ~crc & 0x00000000ffffffffUL);

	memcpy(&val, data, sizeof(data));
#ifndef __LITTLE_ENDIAN__
	val = __builtin_bswap64(val);
#endif
	crc = barrett_reduction_reflected(val);
	printf("%-25s = 0x%08lx\n", "Reflected", crc);
	crc = barrett_reduction_reflected(val ^ 0x00000000ffffffffUL);
	printf("%-25s = 0x%08lx\n", "Reflected and Inverted", ~crc &
		0x00000000ffffffffUL);

	printf("\n");
}
#else
static void do_barrett(void) { }
#endif

static void do_crc(void)
{
	cm_t cm_t = { 0, };

	printf("CRC comparision\n");
	cm_t.cm_width = 32;
	cm_t.cm_poly  = CRC;
	cm_t.cm_init  = 0x0;
	cm_t.cm_refin = FALSE;
	cm_t.cm_refot = FALSE;
	cm_t.cm_xorot = 0x0;
	cm_ini(&cm_t);
	doit(&cm_t, "Base");

	cm_t.cm_init  = 0xffffffff;
	cm_t.cm_xorot = 0xffffffff;
	cm_ini(&cm_t);
	doit(&cm_t, "Inverted");

	cm_t.cm_init  = 0x0;
	cm_t.cm_xorot = 0x0;
	cm_t.cm_refin = TRUE;
	cm_t.cm_refot = TRUE;
	cm_ini(&cm_t);
	doit(&cm_t, "Reflected");

	cm_t.cm_init  = 0xffffffff;
	cm_t.cm_xorot = 0xffffffff;
	cm_ini(&cm_t);
	doit(&cm_t, "Reflected and Inverted");
}

int main(void)
{
	do_barrett();
	do_crc();

	return 0;
}
