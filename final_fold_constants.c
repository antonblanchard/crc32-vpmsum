#include <stdio.h>
#include "poly_arithmetic.h"

#define CRC		0x04C11DB7
#define CRC_FULL	0x104C11DB7

int main(void)
{
	printf("\n.constants:\n");
	print_one_remainder(get_remainder(CRC, 32, 96), 96, "");
	print_one_remainder(get_remainder(CRC, 32, 64), 64, "");
	printf("\t/* Barrett constant m - (4^32)/n */\n");
	print_quotient(get_quotient(CRC, 32, 64), 64, "");
	printf("\t/* Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", CRC_FULL);
	printf("\t.octa 0x0F0E0D0C0B0A09080706050403020100\t/* byte reverse permute constant */\n");

	printf("\n.bit_reflected_constants:\n");
	print_one_remainder(reflect(get_remainder(CRC, 32, 96), 32) << 1, 96, "` << 1");
	print_one_remainder(reflect(get_remainder(CRC, 32, 64), 32) << 1, 64, "` << 1");
	printf("\t/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	print_quotient(reflect(get_quotient(CRC, 32, 64), 33), 64, "`");
	printf("\t/* 33 bit reflected Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", reflect(CRC_FULL, 33));

	printf("\t.octa 0x0F0E0D0C0B0A09080706050403020100\t/* byte reverse permute constant */\n");


	return 0;
}
