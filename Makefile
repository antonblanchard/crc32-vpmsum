ORIG_CFLAGS:= $(CFLAGS)

CFLAGS+=-m64 -g -O2 -mcpu=power8 -mcrypto -mpower8-vector -maltivec -mvsx -Wall
ASFLAGS=-m64 -g
LDFLAGS=-m64 -g -static

SHELL=/bin/bash

# Ethernet CRC
#CRC=0x04C11DB7

# CRC32
CRC=0x11EDC6F41
OPTIONS=-r -x

try-run = $(shell set -e;		\
	TMP="$(TMPOUT).$$$$.tmp";	\
	TMPO="$(TMPOUT).$$$$.o";	\
	if ($(1)) >/dev/null 2>&1;	\
	then echo "$(2)";		\
	else echo "$(3)";		\
	fi;				\
	rm -f "$$TMP" "$$TMPO")

cc-option-yn = $(call try-run,\
	$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_CFLAGS) $(1) -c -x c /dev/null -o "$$TMP",y,n)

PROGS=barrett_reduction_constants \
	final_fold_constants \
	final_fold2_constants \
	crc32_constants \
	crc32_constants.h \
	slice_by_8_bench


PROGS_ALTIVEC=barrett_reduction_test \
	final_fold_test	\
	final_fold2_test \
	crc32_test \
	crc32_bench \
	crc32_stress \
	vec_barrett_reduction_test \
	vec_final_fold_test \
	vec_final_fold2_test \
	vec_crc32_bench \
	crc32_two_implementations

CRC32_CONSTANTS_OBJS=crc32_constants.o poly_arithmetic.o crcmodel.o
ifeq ($(call cc-option-yn,-maltivec),y)
CFLAGS += -maltivec
ASFLAGS += -maltivec

PROGS += $(PROGS_ALTIVEC)
endif

all: $(PROGS)

barrett_reduction_constants: barrett_reduction_constants.o poly_arithmetic.o
final_fold_constants: final_fold_constants.o poly_arithmetic.o
final_fold2_constants: final_fold2_constants.o poly_arithmetic.o
crc32_constants: $(CRC32_CONSTANTS_OBJS)

barrett_reduction_test: barrett_reduction_test.o crcmodel.o barrett_reduction.o
final_fold_test: final_fold_test.o crcmodel.o final_fold.o
final_fold2_test: final_fold2_test.o crcmodel.o final_fold2.o

vec_barrett_reduction_test: vec_barrett_reduction_test.o crcmodel.o \
vec_barrett_reduction.o
vec_final_fold_test: vec_final_fold_test.o crcmodel.o vec_final_fold.o
vec_final_fold2_test: vec_final_fold2_test.o crcmodel.o vec_final_fold2.o


$(CRC32_CONSTANTS_OBJS) : %.o : %.c Makefile
	$(CC) -c $(ORIG_CFLAGS) $< -o $@

crc32_constants.h: crc32_constants
	$(EMULATOR) ./crc32_constants $(OPTIONS) $(CRC) > crc32_constants.h

vec_crc32_c.o: vec_crc32.c crc32_constants.h
	$(CC) -c $(CFLAGS) \
		-D CRC32_FUNCTION=crc32_vpmsum_c \
		vec_crc32.c -o $@
crc32.o: crc32.S crc32_constants.h
crc32_stress.o: crc32_stress.c crc32_constants.h
crc32_test.o: crc32_test.c crc32_constants.h
crc32_wrapper.o: crc32_wrapper.c crc32_constants.h

slice_by_8_bench: crc32_bench.o

crc32_test: crc32_test.o crcmodel.o crc32.o crc32_wrapper.o vec_crc32_c.o
crc32_bench: crc32_bench.o crc32.o crc32_wrapper.o
crc32_stress: crc32_stress.o crcmodel.o crc32.o crc32_wrapper.o

vec_crc32.o: vec_crc32.c crc32_constants.h
vec_crc32_bench: crc32_bench.o vec_crc32.o

vec_crc32_bench:
	$(CC) $(LDFLAGS) $^ -o $@

# This is an example of multiple crc32 polynomials being used
# in a single linked file.
crc32_ethernet_constants.h: crc32_constants
	$(EMULATOR) ./crc32_constants -c -r -x 0x4c11db7 > $@

vec_crc32_ethernet.o: vec_crc32.c crc32_ethernet_constants.h
	$(CC) -c $(CFLAGS) \
		-D CRC32_FUNCTION=crc32ethernet \
		-D CRC32_CONSTANTS_HEADER=\"crc32_ethernet_constants.h\" \
		vec_crc32.c -o $@

crc32k_constants.h: crc32_constants
	$(EMULATOR) ./crc32_constants -a -r -x 0x741B8CD7 > $@

crc32k_wrapper.o: crc32_wrapper.c crc32k_constants.h
crc32k.o: crc32.S crc32k_constants.h

crc32k_wrapper.o crc32k.o:
	$(CC) -c $(CFLAGS) \
		-D CRC32_FUNCTION=crc32k \
		-D CRC32_FUNCTION_ASM=crc32k_asm \
		-D CRC32_CONSTANTS_HEADER=\"crc32k_constants.h\" \
		$< -o $@

crc32_two_implementations: crc32k_wrapper.o crc32k.o vec_crc32_ethernet.o

# implementation has boundaries on datasizes 16 and 256, 32768 so ensure coverage and correctness
test: crc32_test
	set -e ; \
	for len in `seq 0 257`  32767 32768 32769 65535 65536 65537 ; do \
		echo len=$$len; \
		./crc32_test  $${RANDOM} $$len $${RANDOM} ; \
	done ; \

clean:
	rm -f crc32_constants.h crc32k_constants.h crc32_ethernet_constants.h *.o $(PROGS) $(PROGS_ALTIVEC)

.PHONY: clean test all
