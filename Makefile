ORIG_CFLAGS:= $(CFLAGS)

CFLAGS+=-m64 -g -O2 -mcpu=power8 -mpower8-vector -Wall
ASFLAGS=-m64 -g
LDFLAGS=-m64 -g -static

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
	vec_crc32_test \
	vec_crc32_bench

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
crc32.o: crc32.S crc32_constants.h
crc32_stress.o: crc32_stress.c crc32_constants.h
crc32_test.o: crc32_test.c crc32_constants.h
crc32_wrapper.o: crc32_wrapper.c crc32_constants.h

crc32_test: crc32_test.o crcmodel.o crc32.o crc32_wrapper.o
crc32_bench: crc32_bench.o crcmodel.o crc32.o crc32_wrapper.o
crc32_stress: crc32_stress.o crcmodel.o crc32.o crc32_wrapper.o

vec_crc32.o: vec_crc32.c crc32_constants.h
vec_crc32_test.o: vec_crc32_test.c crc32_constants.h
vec_crc32_wrapper.o: vec_crc32_wrapper.c crc32_constants.h
vec_crc32_test: vec_crc32_test.o crcmodel.o vec_crc32.o vec_crc32_wrapper.o
vec_crc32_bench: vec_crc32_bench.o crcmodel.o vec_crc32.o crc32_wrapper.o

clean:
	rm -f crc32_constants.h *.o $(PROGS) $(PROGS_ALTIVEC)
