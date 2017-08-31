#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

unsigned int crc32_vpmsum(unsigned int crc, unsigned char *p, unsigned long len);

int main(int argc, char *argv[])
{
	unsigned long length, iterations;
	unsigned char *data;
	unsigned long i;
	unsigned int crc = 0;

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

	for (i = 0; i < iterations; i++)
		crc = crc32_vpmsum(crc, data, length);

	printf("CRC: %08x\n", crc);

	return 0;
}
