/*
 * Test POWER8 128 bit checksum algorithm
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "crcmodel.h"
#include "crc32_constants.h"

unsigned int
crc32_vpmsum(unsigned int crc, unsigned char *p, unsigned long len);

static unsigned int verify_crc(unsigned int crc, unsigned char *p,
			       unsigned long len)
{
	cm_t cm_t = { 0, };
	int i;

	cm_t.cm_width = 32;
	cm_t.cm_poly  = CRC;
	/* XXX should reflect this */
	cm_t.cm_init  = crc;
#ifdef REFLECT
	cm_t.cm_refin = TRUE;
	cm_t.cm_refot = TRUE;
#else
	cm_t.cm_refin = FALSE;
	cm_t.cm_refot = FALSE;
#endif
#ifdef CRC_XOR
	cm_t.cm_init ^= 0xffffffff;
	cm_t.cm_xorot = 0xffffffff;
#else
	cm_t.cm_xorot = 0x0;
#endif
	cm_ini(&cm_t);

	for (i = 0; i < len; i++)
		cm_nxt(&cm_t, p[i]);

	return cm_crc(&cm_t);
}

int main(int argc, char *argv[])
{
	unsigned long length;
	unsigned long i;
	unsigned int initial_value;
	unsigned char *data;
	unsigned long crc, verify;

	if (argc != 3) {
		fprintf(stderr, "Usage: crc32_test initial_value length\n");
		exit(1);
	}

	initial_value = strtoul(argv[1], NULL, 0);
	length = strtoul(argv[2], NULL, 0);

	data = memalign(16, length);
	if (!data) {
		perror("memalign");
		exit(1);
	}

	srandom(1);
	for (i = 0; i < length; i++)
		data[i] = random() & 0xff;

	crc = crc32_vpmsum(initial_value, data, length);
	verify = verify_crc(initial_value, data, length);

	if (crc != verify)
		printf("FAILURE: got 0x%08lx expected 0x%08lx\n", crc, verify);
	else
		printf("%08lx\n", crc);

	return 0;
}
