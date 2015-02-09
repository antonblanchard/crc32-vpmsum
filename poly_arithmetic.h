#include <inttypes.h>

/* Return x^n mod p(x) over GF(2).  x^deg is the highest power of x in p(x).
   The positions of the bits set in poly represent the remaining powers of x in
   p(x).  In addition, returned in *div are as many of the least significant
   quotient bits as will fit in a uint64_t. */
uint64_t xnmodp(unsigned int n, uint64_t poly, unsigned int deg, uint64_t *div);

/* Reflect the data about the center bit. */
uint64_t reflect(uint64_t data, unsigned int nr_bits);

unsigned long get_remainder(uint64_t crc, unsigned int bits, unsigned int n);

unsigned long get_quotient(uint64_t crc, unsigned int bits, unsigned int n);

void print_one_remainder(unsigned long rem, unsigned int n, char *str);

void print_two_remainders(unsigned long rem1, unsigned int n1,
			  unsigned long rem2, unsigned int n2,
			  char *str);

void print_four_remainders(unsigned long rem1, unsigned int n1,
			   unsigned long rem2, unsigned int n2,
			   unsigned long rem3, unsigned int n3,
			   unsigned long rem4, unsigned int n4,
			   char *str);

void print_quotient(unsigned long quotient, unsigned int n, char *str);
