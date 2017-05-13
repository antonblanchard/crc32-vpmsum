/*
 * Test POWER8 128 bit checksum algorithms.
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

char crc_data[] __attribute__((aligned(16))) = {
	0x9e, 0xd3, 0x20, 0xcc, 0x34, 0x1a, 0x50, 0x64,
	0x5c, 0x73, 0x22, 0x26, 0x5c, 0x32, 0x0d, 0xad,
};

static void doit(p_cm_t p_cm, char *str)
{
	int i;

	for (i = 0; i < sizeof(crc_data)/sizeof(crc_data[0]); i++)
		cm_nxt(p_cm, crc_data[i]);

	printf("%-25s = 0x%08lx\n", str, cm_crc(p_cm));
}

#ifdef __powerpc__
unsigned long __attribute__ ((aligned (32)))
final_fold(void* __restrict__ data);
unsigned long __attribute__ ((aligned (32)))
final_fold_reflected(void* __restrict__ data);

static void do_final_fold(void)
{
	unsigned int crc;

	printf("final_fold\n");

	crc = final_fold(crc_data);
	printf("%-25s = 0x%08x\n", "Base", crc);

	crc = final_fold_reflected(crc_data);
	printf("%-25s = 0x%08x\n", "Reflected", crc);

	printf("\n");
}
#else
static void do_final_fold(void) { }
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

	cm_t.cm_init  = 0x0;
	cm_t.cm_xorot = 0x0;
	cm_t.cm_refin = TRUE;
	cm_t.cm_refot = TRUE;
	cm_ini(&cm_t);
	doit(&cm_t, "Reflected");
}

int main(void)
{
	do_final_fold();
	do_crc();

	return 0;
}
