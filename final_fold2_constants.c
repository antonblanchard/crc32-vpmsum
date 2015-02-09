#include <stdio.h>
#include "poly_arithmetic.h"

#define CRC		0x04C11DB7
#define CRC_FULL	0x104C11DB7

int main(void)
{
	unsigned long a, b, c, d;

	printf("\n.constants:\n");
	a = get_remainder(CRC, 32, 128);
	b = get_remainder(CRC, 32, 96);
	c = get_remainder(CRC, 32, 64);
	d = get_remainder(CRC, 32, 32);
	print_four_remainders(a, 128, b, 96, c, 64, d, 32, "");
	printf("\t/* Barrett constant m - (4^32)/n */\n");
	print_quotient(get_quotient(CRC, 32, 64), 64, "");
	printf("\t/* Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", CRC_FULL);
	printf("\t.octa 0x0F0E0D0C0B0A09080706050403020100\t/* byte reverse permute constant */\n");

	printf("\n.bit_reflected_constants:\n");
	a = reflect(get_remainder(CRC, 32, 32), 32);
	b = reflect(get_remainder(CRC, 32, 64), 32);
	c = reflect(get_remainder(CRC, 32, 96), 32);
	d = reflect(get_remainder(CRC, 32, 128), 32);
	print_four_remainders(a, 32, b, 64, c, 96, d, 128, "`");
	printf("\t/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	print_quotient(reflect(get_quotient(CRC, 32, 64), 33), 64, "`");
	printf("\t/* 33 bit reflected Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", reflect(CRC_FULL, 33));
	printf("\t/* byte reverse permute constant */\n");
	printf("\t.octa 0x0F0E0D0C0B0A09080706050403020100\n");

	return 0;
}
