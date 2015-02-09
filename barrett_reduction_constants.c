#include <stdio.h>
#include "poly_arithmetic.h"

#define CRC		0x04C11DB7
#define CRC_FULL	0x104C11DB7

int main(void)
{
	printf("\n.constants:\n");
	printf("\t/* Barrett constant m - (4^32)/n */\n");
	print_quotient(get_quotient(CRC, 32, 64), 64, "");
	printf("\t/* Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", CRC_FULL);

	printf("\n.bit_reflected_constants:\n");
	printf("\t/* 33 bit reflected Barrett constant m - (4^32)/n */\n");
	print_quotient(reflect(get_quotient(CRC, 32, 64), 33), 64, "`");
	printf("\t/* 33 bit reflected Barrett constant n */\n");
	printf("\t.octa 0x%032lx\n", reflect(CRC_FULL, 33));

	return 0;
}
