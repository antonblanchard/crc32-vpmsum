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

static void print_header(int argc, char *argv[]) {
	printf("/*\n");
	printf("*\n");
	printf("* THIS FILE IS GENERATED WITH\n");
	while (argc-- > 0)
		printf("%s ", argv++[0]);
	printf("\n\n* This is from https://github.com/antonblanchard/crc32-vpmsum/\n");
	printf("* DO NOT MODIFY IT MANUALLY!\n");
	printf("*\n");
	printf("*/\n\n");
}

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
	printf("#endif /* CRC_TABLE */\n");
}

static void do_nonreflected(unsigned int crc, int xor, int assembler, int p8intrinsics)
{
	int i;
	unsigned long a, b, c, d;

	printf("#define CRC 0x%x\n", crc);
	if (xor)
		printf("#define CRC_XOR\n");
	printf("#define MAX_SIZE    %d\n", BLOCKING);
	printf("\n#ifndef __ASSEMBLER__\n");
	create_table(crc, 0);

	if (!p8intrinsics)
		goto skip_p8_intrinsics;

	/* Generate vector constants. */
	printf("#ifdef POWER8_INTRINSICS\n");
	printf("\n/* Constants */\n");
	printf("\n/* Reduce %d kbits to 1024 bits */", BLOCKING*8);
	printf("\nstatic const __vector unsigned long long vcrc_const[%d]\n",
		((BLOCKING*8)/1024)-1);
	printf("\t__attribute__((aligned (16))) = {\n");
	printf("#ifdef __LITTLE_ENDIAN__\n");
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = get_remainder(crc, 32, i+64);
		b = get_remainder(crc, 32, i);
		/* Print two remainders. */
		printf("\t\t/* x^%u mod p(x)` , x^%u mod p(x)` */\n", i+64, i);
		if (i != 1024) {
			printf("\t\t{ 0x%016lx, 0x%016lx },\n", b, a);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%016lx, 0x%016lx }\n", b, a);
		}
	}
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = get_remainder(crc, 32, i+64);
		b = get_remainder(crc, 32, i);
		/* Print two remainders. */
		printf("\t\t/* x^%u mod p(x)` , x^%u mod p(x)` */\n", i+64, i);
		if (i != 1024) {
			printf("\t\t{ 0x%016lx, 0x%016lx },\n", a, b);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%016lx, 0x%016lx }\n", a, b);
		}
	}
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");


	printf("\n/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to "
		"include the trailing 32 bits of zeros */\n");
	printf("\nstatic const __vector unsigned long long vcrc_short_const[%d]\n",
		((1024*2)/128));
	printf("\t__attribute__((aligned (16))) = {\n");
	printf("#ifdef __LITTLE_ENDIAN__\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = get_remainder(crc, 32, i+128);
		b = get_remainder(crc, 32, i+96);
		c = get_remainder(crc, 32, i+64);
		d = get_remainder(crc, 32, i+32);

		/* Print four remainders. */
		printf("\t\t/* x^%u mod p(x) , x^%u mod p(x) , x^%u mod p(x) , "
			"x^%u mod p(x)  */\n", i+128, i+96, i+64, i+32);
		if (i != 0) {
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx },\n", c, d, a, b);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx }\n", c, d, a, b);
		}
	}
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = get_remainder(crc, 32, i+128);
		b = get_remainder(crc, 32, i+96);
		c = get_remainder(crc, 32, i+64);
		d = get_remainder(crc, 32, i+32);

		/* Print four remainders. */
		printf("\t\t/* x^%u mod p(x) , x^%u mod p(x) , x^%u mod p(x) , "
			"x^%u mod p(x)  */\n", i+128, i+96, i+64, i+32);
		if (i != 0) {
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx },\n", a, b, c, d);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx }\n", a, b, c, d);
		}
	}
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");

	printf("\n/* Barrett constants */\n");
	printf("/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	printf("\nstatic const __vector unsigned long long v_Barrett_const[%d]\n"
		, 2);
	printf("\t__attribute__((aligned (16))) = {\n");
	/* Print quotient and Barrett constant. */
	printf("\t\t/* x^%u div p(x)  */\n", 64);
	printf("#ifdef __LITTLE_ENDIAN__\n");
	printf("\t\t{ 0x%016lx, 0x%016lx },\n", get_quotient(crc, 32, 64), 0UL);
	printf("\t\t{ 0x%016lx, 0x%016lx }\n", (1UL << 32) | crc, 0UL);
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	printf("\t\t{ 0x%016lx, 0x%016lx },\n", 0UL, get_quotient(crc, 32, 64));
	printf("\t\t{ 0x%016lx, 0x%016lx }\n", 0UL, (1UL << 32) | crc);
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");

	printf("#endif /* POWER8_INTRINSICS */\n\n");

skip_p8_intrinsics:

	if (!assembler)
		goto skip_assembler;

	printf("#else /* __ASSEMBLER__ */\n");
	printf(".constants:\n");

	printf("\n\t/* Reduce %d kbits to 1024 bits */\n", BLOCKING*8);
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = get_remainder(crc, 32, i+64);
		b = get_remainder(crc, 32, i);

		print_two_remainders(a, i+64, b, i, "");
		printf("\n");
	}

	printf(".short_constants:\n");

	printf("\n\t/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to "
		"include the trailing 32 bits of zeros */\n");
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

skip_assembler:

	printf("#endif /* __ASSEMBLER__ */\n");
}

static void do_reflected(unsigned int crc, int xor, int assembler, int p8intrinsics)
{
	int i;
	unsigned long a, b, c, d;

	printf("#define CRC 0x%x\n", crc);
	if (xor)
		printf("#define CRC_XOR\n");
	printf("#define REFLECT\n");
	printf("#define MAX_SIZE    %d\n", BLOCKING);
	printf("\n#ifndef __ASSEMBLER__\n");
	create_table(crc, 1);
	/* Generate vector constants (reflected). */

	if (!p8intrinsics)
		goto skip_p8_intrinsics;

	printf("#ifdef POWER8_INTRINSICS\n");
	printf("\n/* Constants */\n");
	printf("\n/* Reduce %d kbits to 1024 bits */", BLOCKING*8);
	printf("\nstatic const __vector unsigned long long vcrc_const[%d]\n",
		((BLOCKING*8)/1024)-1);
	printf("\t__attribute__((aligned (16))) = {\n");
	printf("#ifdef __LITTLE_ENDIAN__\n");
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = reflect(get_remainder(crc, 32, i), 32) << 1;
		b = reflect(get_remainder(crc, 32, i+64), 32) << 1;

		/* Print two remainders. */
		printf("\t\t/* x^%u mod p(x)` << 1, x^%u mod p(x)` << 1 */\n", i, i+64);
		if (i != 1024) {
			printf("\t\t{ 0x%016lx, 0x%016lx },\n", b, a);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%016lx, 0x%016lx }\n", b, a);
		}
	}
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = reflect(get_remainder(crc, 32, i), 32) << 1;
		b = reflect(get_remainder(crc, 32, i+64), 32) << 1;

		/* Print two remainders. */
		printf("\t\t/* x^%u mod p(x)` << 1, x^%u mod p(x)` << 1 */\n", i, i+64);
		if (i != 1024) {
			printf("\t\t{ 0x%016lx, 0x%016lx },\n", a, b);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%016lx, 0x%016lx }\n", a, b);
		}
	}
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");


	printf("\n/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to "
		"include the trailing 32 bits of zeros */\n");
	printf("\nstatic const __vector unsigned long long vcrc_short_const[%d]\n",
		((1024*2)/128));
	printf("\t__attribute__((aligned (16))) = {\n");
	printf("#ifdef __LITTLE_ENDIAN__\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = reflect(get_remainder(crc, 32, i+32), 32);
		b = reflect(get_remainder(crc, 32, i+64), 32);
		c = reflect(get_remainder(crc, 32, i+96), 32);
		d = reflect(get_remainder(crc, 32, i+128), 32);

		/* Print four remainders. */
		printf("\t\t/* x^%u mod p(x) , x^%u mod p(x) , x^%u mod p(x) , "
			"x^%u mod p(x)  */\n", i+32, i+64, i+96, i+128);
		if (i != 0) {
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx },\n", c, d, a, b);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx }\n", c, d, a, b);
		}
	}
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	for (i = (1024*2)-128; i >= 0; i -= 128) {
		a = reflect(get_remainder(crc, 32, i+32), 32);
		b = reflect(get_remainder(crc, 32, i+64), 32);
		c = reflect(get_remainder(crc, 32, i+96), 32);
		d = reflect(get_remainder(crc, 32, i+128), 32);

		/* Print four remainders. */
		printf("\t\t/* x^%u mod p(x) , x^%u mod p(x) , x^%u mod p(x) , "
			"x^%u mod p(x)  */\n", i+32, i+64, i+96, i+128);
		if (i != 0) {
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx },\n", a, b, c, d);
		} else {
			/* Do not print comma on the last element of the array. */
			printf("\t\t{ 0x%08lx%08lx, 0x%08lx%08lx }\n", a, b, c, d);
		}
	}
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");

	printf("\n/* Barrett constants */\n");
	printf("/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	printf("\nstatic const __vector unsigned long long v_Barrett_const[%d]\n"
		, 2);
	printf("\t__attribute__((aligned (16))) = {\n");
	/* Print quotient and Barrett constant. */
	printf("\t\t/* x^%u div p(x)  */\n", 64);
	printf("#ifdef __LITTLE_ENDIAN__\n");
	printf("\t\t{ 0x%016lx, 0x%016lx },\n", reflect(get_quotient(crc, 32, 64), 33), 0UL);
	printf("\t\t{ 0x%016lx, 0x%016lx }\n", reflect((1UL << 32) | crc, 33), 0UL);
	printf("#else /* __LITTLE_ENDIAN__ */\n");
	printf("\t\t{ 0x%016lx, 0x%016lx },\n", 0UL, reflect(get_quotient(crc, 32, 64),33));
	printf("\t\t{ 0x%016lx, 0x%016lx }\n", 0UL, reflect((1UL << 32) | crc, 33));
	printf("#endif /* __LITTLE_ENDIAN__ */\n");
	printf("\t};\n");

	printf("#endif /* POWER8_INTRINSICS */\n\n");

skip_p8_intrinsics:

	if (!assembler)
		goto skip_assembler;

	printf("#else /* __ASSEMBLER__ */\n");
	printf(".constants:\n");

	printf("\n\t/* Reduce %d kbits to 1024 bits */\n", BLOCKING*8);
	for (i = (BLOCKING*8)-1024; i > 0; i -= 1024) {
		a = reflect(get_remainder(crc, 32, i), 32) << 1;
		b = reflect(get_remainder(crc, 32, i+64), 32) << 1;

		print_two_remainders(a, i, b, i+64, "` << 1");
		printf("\n");
	}

	printf(".short_constants:\n");

	printf("\n\t/* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to "
		"include the trailing 32 bits of zeros */\n");
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

skip_assembler:

	printf("#endif /* __ASSEMBLER__ */\n");
}

static void usage(char *argv[])
{
	fprintf(stderr, "Usage: %s {-r} {-x} CRC\n", argv[0]);
	fprintf(stderr, "\tCRC without top bit\n");
	fprintf(stderr, "\t-r bit reflect\n");
	fprintf(stderr, "\t-x xor input and ouput\n");
	fprintf(stderr, "\t-a generate constants for assembler implementaiton\n");
	fprintf(stderr, "\t-c generate constants for P8 intrinsics (C) implementation\n");
	fprintf(stderr, "Without -a or -c - both will be generated\n");
	fprintf(stderr, "Usual usage is to redirect this into a file called crc32_constants.h\n");
}

int main(int argc, char *argv[])
{
	int reflect = 0;
	int xor = 0;
	int p8intrinsics = 0;
	int assembler = 0;
	unsigned int crc;

	while (1) {
		signed char c = getopt(argc, argv, "rxac");
		if (c < 0)
			break;

		switch (c) {
		case 'r':
			reflect = 1;
			break;

		case 'x':
			xor = 1;
			break;

		case 'c':
			p8intrinsics = 1;
			break;

		case 'a':
			assembler = 1;
			break;

		default:
			usage(argv);
			exit(1);
			break;
		}
	}

	if ((argc - optind) != 1) {
		usage(argv);
		exit(1);
	}

	crc = strtoul(argv[optind], NULL, 0);
	print_header(argc, argv);

	/* neither specified so use both */
	if (assembler == 0 && p8intrinsics == 0) assembler = p8intrinsics = 1;

	if (reflect)
		do_reflected(crc, xor, assembler, p8intrinsics);
	else
		do_nonreflected(crc, xor, assembler, p8intrinsics);

	return 0;
}
