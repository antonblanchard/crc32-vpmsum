/*
 * Based on code from Mark Adler:
 *
 * Placed in the public domain by Mark Adler, Jan 18, 2014.
 */

#include <stdio.h>
#include <inttypes.h>

/* Return x^n mod p(x) over GF(2).  x^deg is the highest power of x in p(x).
   The positions of the bits set in poly represent the remaining powers of x in
   p(x).  In addition, returned in *div are as many of the least significant
   quotient bits as will fit in a uint64_t. */
uint64_t xnmodp(unsigned int n, uint64_t poly, unsigned int deg,
		       uint64_t *div)
{
	uint64_t mod, mask, high;

	if (n < deg) {
		*div = 0;
		return poly;
	}
	mask = ((uint64_t) 1 << deg) - 1;
	poly &= mask;
	mod = poly;
	*div = 1;
	deg--;
	while (--n > deg) {
		high = (mod >> deg) & 1;
		*div = (*div << 1) | high;	/* quotient bits may be lost off the top */
		mod <<= 1;
		if (high)
			mod ^= poly;
	}
	return mod & mask;
}

/* Reflect the data about the center bit. */
uint64_t reflect(uint64_t data, unsigned int nr_bits)
{
	uint64_t reflection = 0;
	unsigned int bit;

	for (bit = 0; bit < nr_bits; ++bit) {
		if (data & 0x01)
			reflection |= (1UL << ((nr_bits - 1) - bit));

		data = (data >> 1);
	}

	return reflection;
}

unsigned long get_remainder(uint64_t crc, unsigned int bits, unsigned int n)
{
	unsigned long div;

	return xnmodp(n, crc, bits, &div);
}

unsigned long get_quotient(uint64_t crc, unsigned int bits, unsigned int n)
{
	unsigned long div;

	xnmodp(n, crc, bits, &div);

	return div;
}

void print_one_remainder(unsigned long rem, unsigned int n, char *str)
{
	printf("\t.octa 0x%032lx\t/* x^%u mod p(x)%s */\n", rem, n, str);
}

void print_two_remainders(unsigned long rem1, unsigned int n1,
			  unsigned long rem2, unsigned int n2,
			  char *str)
{
	printf("\t/* x^%u mod p(x)%s, x^%u mod p(x)%s */\n",  n1, str, n2, str);
	printf("\t.octa 0x%016lx%016lx\n", rem1, rem2);
}

void print_four_remainders(unsigned long rem1, unsigned int n1,
			   unsigned long rem2, unsigned int n2,
			   unsigned long rem3, unsigned int n3,
			   unsigned long rem4, unsigned int n4,
			   char *str)
{
	printf("\t/* x^%u mod p(x)%s, x^%u mod p(x)%s, x^%u mod p(x)%s, x^%u mod p(x)%s */\n",  n1, str, n2, str, n3, str, n4, str);
	printf("\t.octa 0x%08lx%08lx%08lx%08lx\n", rem1, rem2, rem3, rem4);
}

void print_quotient(unsigned long quotient, unsigned int n, char *str)
{
	printf("\t.octa 0x%032lx\t/* x^%u div p(x)%s */\n", quotient, n, str);
}
