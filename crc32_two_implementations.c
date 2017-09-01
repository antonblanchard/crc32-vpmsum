#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

unsigned int crc32k(unsigned int crc, unsigned char *p, unsigned long len);
unsigned int crc32ethernet(unsigned int crc, unsigned char *p, unsigned long len);

int main(int argc, char *argv[])
{
	unsigned long length, iterations;
	unsigned char *data;
	unsigned long i;
	unsigned int crck = 0;
	unsigned int crcethernet = 0;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s length iterations\n", argv[0]);
		fprintf(stderr, "Performs crc32 checksum an [iterations] number of times on a buffer filled with junk data of [length] bytes\n");
		exit(1);
	}

	length = strtoul(argv[1], NULL, 0);
	iterations = strtoul(argv[2], NULL, 0);

	data = memalign(getpagesize(), length);

	srandom(1);
	for (i = 0; i < length; i++)
		data[i] = random() & 0xff;

	for (i = 0; i < iterations; i++) {
		crcethernet = crc32ethernet(crcethernet, data, length);
		crck = crc32k(crck, data, length);
	}

	printf("CRC(k): %08x\n", crck);
	printf("CRC(ethernet): %08x\n", crcethernet);

	return 0;
}
