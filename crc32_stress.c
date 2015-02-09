/*
 * Test POWER8 128 bit checksum algorithm
 *
 * Copyright (C) 2015 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "crcmodel.h"
#include "crc32_constants.h"

#define MAX_CRC_LENGTH (128*1024)

unsigned int crc32_vpmsum(unsigned int crc, unsigned char *p,
			  unsigned long len);

static unsigned int verify_crc(unsigned int crc, unsigned char *p,
			       unsigned long len)
{
	cm_t cm_t = { 0, };
	int i;

	cm_t.cm_width = 32;
	cm_t.cm_poly  = CRC;
	cm_t.cm_init  = crc;
#ifdef REFLECT
	cm_t.cm_refin = TRUE;
	cm_t.cm_refot = TRUE;
#else
	cm_t.cm_refin = FALSE;
	cm_t.cm_refot = FALSE;
#endif
	cm_t.cm_xorot = 0x0;
	cm_ini(&cm_t);

	for (i = 0; i < len; i++)
		cm_nxt(&cm_t, p[i]);

	return cm_crc(&cm_t);
}

#define VMX_ALIGN	16
#define VMX_ALIGN_MASK	(VMX_ALIGN-1)

int main(void)
{
	unsigned char *data;

	data = memalign(VMX_ALIGN, MAX_CRC_LENGTH+VMX_ALIGN_MASK);
	if (!data) {
		perror("memalign");
		exit(1);
	}

	srandom(1);

	while (1) {
		unsigned int crc = 0, verify = 0;
		unsigned int len, offset;
		unsigned long i;

		for (i = 0; i < MAX_CRC_LENGTH; i++)
			data[i] = random() & 0xff;

		len = random() % MAX_CRC_LENGTH;
		offset = random() & VMX_ALIGN_MASK;

		crc = crc32_vpmsum(crc, data+offset, len);
		verify = verify_crc(verify, data+offset, len);

		if (crc != verify) {
			fprintf(stderr, "FAILURE: got 0x%08x expected 0x%08x (len %d)\n", crc, verify, len);
			crc = verify;
		}
	}

	return 0;
}
