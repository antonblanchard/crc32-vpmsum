/*
 * Calculate the checksum of 128 bits of data.
 *
 * We add 32 bits of 0s to make 192 bits of data - this matches what a
 * CRC does. We reduce the 192 bits in two steps, first reducing the top 64
 * bits to produce 96 bits, then reducing the top 32 bits of that to produce 64
 * bits.
 *
 * We then use fixed point Barrett reduction to compute a mod n over GF(2)
 * for n = 0x104c11db7 using POWER8 instructions. We use x = 32.
 *
 * http://en.wikipedia.org/wiki/Barrett_reduction
 *
 * Copyright (C) 2015 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of either:
 *
 *  a) the GNU General Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your option)
 *     any later version, or
 *  b) the Apache License, Version 2.0
 */

#if defined (__clang__)
#define __ALTIVEC__
#include "ppc-asm.h"
#else
#include <ppc-asm.h>
#endif
#include "ppc-opcode.h"

	.section	.data
.balign 16
.constants:
	/* x^96 mod p(x) */
	.octa 0x000000000000000000000000f200aa66

	/* x^64 mod p(x) */
	.octa 0x000000000000000000000000490d678d

	/* Barrett constant m - (4^32)/n */
	.octa 0x00000000000000000000000104d101df

	/* Barrett constant n */
	.octa 0x00000000000000000000000104c11db7

	/* byte reverse permute constant */
	.octa 0x0F0E0D0C0B0A09080706050403020100

.bit_reflected_constants:
	/* x^96 mod p(x)` << 1 */
	.octa 0x000000000000000000000000ccaa009e

	/* x^64 mod p(x)` << 1 */
	.octa 0x00000000000000000000000163cd6124

	/* 33 bit reflected Barrett constant m - (4^32)/n */
	.octa 0x000000000000000000000001f7011641

	/* 33 bit reflected Barrett constant n */
	.octa 0x000000000000000000000001db710641

	/* byte reverse permute constant */
	.octa 0x0F0E0D0C0B0A09080706050403020100

	.text

#define const1		v10
#define const2		v11
#define const3		v12
#define const4		v13

#define	mask_32bit	v28
#define	mask_64bit	v29
#define zeroes		v30
#define ones		v31

/* unsigned int final_fold(void *data) */
FUNC_START(final_fold)
	lis	r4,.constants@ha
	la	r4,.constants@l(r4)

	li	r5,16
	li	r6,32
	li	r7,48
	li	r8,64

	vxor	zeroes,zeroes,zeroes
	vspltisw ones,-1

	vsldoi	mask_32bit,zeroes,ones,4
	vsldoi	mask_64bit,zeroes,ones,8

	lvx	v0,0,r3		/* load data */

#ifdef __LITTLE_ENDIAN__
	lvx	const1,r8,r4
	vperm	v0,v0,v0,const1
#endif

	lvx	const1,0,r4
	lvx	const2,r5,r4
	lvx	const3,r6,r4
	lvx	const4,r7,r4

	/*
	 * We append 32 bits of zeroes to our 128 bit value. This gives us 192
	 * bits that we reduce in two steps.
	 */

	/* Reduce the top 64 bits */
	vsldoi	v1,zeroes,v0,8		/* Grab the top 64 bits */
	VPMSUMD(v1,v1,const1)

	/* Add 32 bits of zeroes and xor with the reduced top 64 bits */
	vsldoi	v0,v0,zeroes,4
	vxor	v0,v1,v0

	/* We have a 96 bit value, now reduce the top 32 bits */
	vsldoi	v1,zeroes,v0,8		/* Grab the top 64 bits */
	vand	v1,v1,mask_32bit
	VPMSUMD(v1,v1,const2)

	vxor	v0,v1,v0
	vand	v0,v0,mask_64bit

	/*
	 * Now for Barrett reduction. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	VPMSUMD(v1,v0,const3)		/* ma */
	vsldoi	v1,zeroes,v1,8		/* q = floor(ma/(2^64)) */
	VPMSUMD(v1,v1,const4)		/* qn */
	vxor	v0,v0,v1		/* a - qn, subtraction is xor in GF(2) */

	/*
	 * Get the result into r3. We need to shift it left 8 bytes:
	 * V0 [ 0 1 2 X ]
	 * V0 [ 0 X 2 3 ]
	 */
	vsldoi	v0,v0,zeroes,8		/* shift result into top 64 bits */
	MFVRD(r3, v0)

	blr
FUNC_END(final_fold)

/* unsigned int final_fold_reflected(void *data) */
FUNC_START(final_fold_reflected)
	lis	r4,.bit_reflected_constants@ha
	la	r4,.bit_reflected_constants@l(r4)

	li	r5,16
	li	r6,32
	li	r7,48
	li	r8,64

	vxor	zeroes,zeroes,zeroes
	vspltisw ones,-1

	vsldoi	mask_32bit,zeroes,ones,4
	vsldoi	mask_64bit,zeroes,ones,8

	lvx	v0,0,r3		/* load data */

#ifndef __LITTLE_ENDIAN__
	lvx	const1,r8,r4
	vperm	v0,v0,v0,const1
#endif

	lvx	const1,0,r4
	lvx	const2,r5,r4
	lvx	const3,r6,r4
	lvx	const4,r7,r4

	/*
	 * We append 32 bits of zeroes to our 128 bit value. This gives us 192
	 * bits that we reduce in two steps. This time we are reducing the
	 * bits on the right side (ie the lower bits) and xor'ing them
	 * on the left side.
	 */

	/* Reduce the top 64 bits */
	VPMSUMD(v1,v0,const1)
	vsldoi	v1,v1,zeroes,4		/* align 96bit result to the left */

	/* Add 32 bits of zeroes and xor with the reduced top 64 bits */
	vsldoi	v0,zeroes,v0,12		/* zeroes on the left */
	vxor	v0,v1,v0

	/* We have a 96 bit value, now reduce the top 32 bits */
	vsldoi	v1,zeroes,v0,12		/* Grab the right 64 bits */
	vand	v1,v1,mask_32bit
	VPMSUMD(v1,v1,const2)

	vsldoi	v0,zeroes,v0,8
	vxor	v0,v1,v0
	vand	v0,v0,mask_64bit

	/*
	 * Now for the Barrett reduction algorithm. Instead of bit reflecting
	 * our data (which is expensive to do), we bit reflect our constants
	 * and our algorithm, which means the intermediate data in our vector
	 * registers goes from 0-63 instead of 63-0. We can reflect the
	 * algorithm because we don't carry in mod 2 arithmetic.
	 */
	vand	v1,v0,mask_32bit	/* bottom 32 bits of a */
	VPMSUMD(v1,v1,const3)		/* ma */
	vand	v1,v1,mask_32bit	/* bottom 32bits of ma */
	VPMSUMD(v1,v1,const4)		/* qn */
	vxor	v0,v0,v1		/* a - qn, subtraction is xor in GF(2) */

	/*
	 * Since we are bit reflected, the result (ie the low 32 bits) is in the
	 * high 32 bits. We just need to shift it left 4 bytes
	 * V0 [ 0 1 X 3 ]
	 * V0 [ 0 X 2 3 ]
	 */
	vsldoi	v0,v0,zeroes,4		/* shift result into top 64 bits */
	MFVRD(r3, v0)

	blr
FUNC_END(final_fold_reflected)
