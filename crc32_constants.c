#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "crcmodel.h"
#include "poly_arithmetic.h"

/*
 * The sweet spot for POWER8 (found via benchmarking) is checksumming
 * in blocks of 32kB (256 kbits).
 */
#define BLOCKING	(32*1024)

static void create_table(unsigned int crc, int reflect)
{
	int i;
	cm_t cm_t = { 0, };

	cm_t.cm_width = 32;
	cm_t.cm_poly = crc;
	cm_t.cm_refin = reflect;

	printf("#ifdef CRC_TABLE\n");
	printf("static const unsigned int crc_table[] = {");

	for (i = 0; i < 256; i++) {
		if (!(i % 4))
			printf("\n\t");
		else
			printf(" ");
		printf("0x%08lx,", cm_tab(&cm_t, i));
	}

	printf("};\n\n");
	printf("#endif\n");
}

static void do_nonreflected(unsigned int crc, int xor)
{
	int i;
	unsigned long a, b, c, d;

	printf("#define CRC 0x%x\n", crc);
	if (xor)
		printf("#define CRC_XOR\n");
	printf("\n#ifndef __ASSEMBLY__\n");
	create_table(crc, 0);
	printf("#else\n");

	printf("#define MAX_SIZE	%d\n", BLOCKING);
	printf(".constants:\n");

	printf("\n\t/* Reduce %d kbits to 1024 bits */\n", BLOCKING*8);
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = get_remainder(crc, 32, i+64);
		b = get_remainder(crc, 32, i);

		print_two_remainders(a, i+64, b, i, "");
		printf("\n");
	}

	printf(".short_constants:\n");

	printf("\n\t/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to include the trailing 32 bits of zeros */\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = get_remainder(crc, 32, i+128);
		b = get_remainder(crc, 32, i+96);
		c = get_remainder(crc, 32, i+64);
		d = get_remainder(crc, 32, i+32);

		print_four_remainders(a, i+128, b, i+96, c, i+64, d, i+32, "");
		printf("\n");
	}

	printf("\n.barrett_constants:\n");
	printf("\t/* Barrett constant m - (4^32)/n */\n");
	print_quotient(get_quotient(crc, 32, 64), 64, "");
	printf("\t/* Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", (1UL << 32) | crc);

	printf("#endif\n");
}

static void do_reflected(unsigned int crc, int xor)
{
	int i;
	unsigned long a, b, c, d;

	printf("#define CRC 0x%x\n", crc);
	if (xor)
		printf("#define CRC_XOR\n");
	printf("#define REFLECT\n");
	printf("\n#ifndef __ASSEMBLY__\n");
	create_table(crc, 1);
	printf("#else\n");

	printf("#define MAX_SIZE	%d\n", BLOCKING);
	printf(".constants:\n");

	printf("\n\t/* Reduce %d kbits to 1024 bits */\n", BLOCKING*8);
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = reflect(get_remainder(crc, 32, i), 32) << 1;
		b = reflect(get_remainder(crc, 32, i+64), 32) << 1;

		print_two_remainders(a, i, b, i+64, "` << 1");
		printf("\n");
	}

	printf(".short_constants:\n");

	printf("\n\t/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to include the trailing 32 bits of zeros */\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = reflect(get_remainder(crc, 32, i+32), 32);
		b = reflect(get_remainder(crc, 32, i+64), 32);
		c = reflect(get_remainder(crc, 32, i+96), 32);
		d = reflect(get_remainder(crc, 32, i+128), 32);

		print_four_remainders(a, i+32, b, i+64, c, i+96, d, i+128, "`");
		printf("\n");
	}

	printf("\n.barrett_constants:\n");
	printf("\t/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	print_quotient(reflect(get_quotient(crc, 32, 64), 33), 64, "`");
	printf("\t/* 33 bit reflected Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", reflect((1UL << 32) | crc, 33));

	printf("#endif\n");
}

static void usage(void)
{
	fprintf(stderr, "Usage: crc32_constants {-r} {-x} CRC\n");
	fprintf(stderr, "\tCRC without top bit\n");
	fprintf(stderr, "\t-r bit reflect\n");
	fprintf(stderr, "\t-x xor input and ouput\n");
}

int main(int argc, char *argv[])
{
	int reflect = 0;
	int xor = 0;
	unsigned int crc;

	while (1) {
		signed char c = getopt(argc, argv, "rx");
		if (c < 0)
			break;

		switch (c) {
		case 'r':
			reflect = 1;
			break;

		case 'x':
			xor = 1;
			break;

		default:
			usage();
			exit(1);
			break;
		}
	}

	if ((argc - optind) != 1) {
		usage();
		exit(1);
	}

	crc = strtoul(argv[optind], NULL, 0);

	if (reflect)
		do_reflected(crc, xor);
	else
		do_nonreflected(crc, xor);

	return 0;
}
