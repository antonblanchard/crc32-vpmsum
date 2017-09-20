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

/* ASM implementation */
unsigned int crc32_vpmsum(unsigned int crc, unsigned char *p,
			  unsigned long len);

/* C Implementation */
unsigned int crc32_vpmsum_c(unsigned int crc, unsigned char *p,
			  unsigned long len);

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


/* copied from C implementation, need to check the same result occurs regardless
   of initial alignment */
#define VMX_ALIGN       16

int main(int argc, char *argv[])
{
	unsigned long length;
	unsigned long i;
	unsigned int initial_value;
	unsigned char *data, *data_initial;
	unsigned int crc, crc_c, verify, seed;
	int j, ret = 0;

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: %s initial_value length [seed]\n", argv[0]);
		fprintf(stderr, "Verifies the crc32_vpmsum returns the same result as a reference implementation [initial_value] is checksummed with [length] bytes of random data from seed (default 1).\n");
		exit(1);
	}

	initial_value = strtoul(argv[1], NULL, 0);
	length = strtoul(argv[2], NULL, 0);

	if (argc >= 4) {
		char *endptr;
		seed = strtoul(argv[3], &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Not a valid value for seed, not seeding random sequence\n");
		} else {
			srandom(seed);
		}
	} else {
		srandom(1);
	}

	data_initial = data = memalign(16, length + VMX_ALIGN);
	if (!data) {
		perror("memalign");
		exit(1);
	}

	for (i = 0; i < length; i++)
		data[i] = random() & 0xff;

	crc = crc32_vpmsum(initial_value, data, length);
	crc_c = crc32_vpmsum_c(initial_value, data, length);
	verify = verify_crc(initial_value, data, length);

	if (crc_c != verify) {
		printf("FAILURE: C crc32_vpmsum got 0x%08x expected 0x%08x\n", crc_c, verify);
		ret = 1;
	}

	if (crc != verify) {
		printf("FAILURE: ASM crc32_vpmsum got 0x%08x expected 0x%08x\n", crc, verify);
		ret = 1;
	}
	else
		printf("Calculated CRC32 to be: %08x\n", crc);

	for (j = 1; j < VMX_ALIGN; j++) {
		memmove(data+1, data, length);
		data += 1;

		crc = crc32_vpmsum(initial_value, data, length);
		crc_c = crc32_vpmsum_c(initial_value, data, length);
		if (crc_c != verify) {
			printf("FAILURE: C crc32_vpmsum, at alignment offset %d, got 0x%08x expected 0x%08x\n", j, crc_c, verify);
			ret = 1;
		}

		if (crc != verify) {
			printf("FAILURE: ASM crc32_vpmsum, at alignment offset %d, got 0x%08x expected 0x%08x\n", j, crc, verify);
			ret = 1;
		}
	}

	free(data_initial);

	return ret;
}
