/*
 * Use the fixed point version of Barrett reduction to compute a mod n
 * over GF(2) for n = 0x104c11db7 using POWER8 instructions. We use k = 32.
 *
 * http://en.wikipedia.org/wiki/Barrett_reduction
 *
 * Copyright (C) 2017 Rogerio Alves <rogealve@br.ibm.com>, IBM
 * Copyright (C) 2017 Steven Munroe <sjmunroe@us.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of either:
 *
 *  a) the GNU General Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your option)
 *     any later version, or
 *  b) the Apache License, Version 2.0
 */

#include <stdint.h>
#include <altivec.h>

#if defined(__LITTLE_ENDIAN__)
static const __vector unsigned long v_Barrett_const[2]
	 __attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000104d101dfUL, 0x0000000000000000UL },
		/* Barrett constant n */
		{ 0x0000000104c11db7UL, 0x0000000000000000UL }
	};

static const __vector unsigned long v_Barrett_reflect_const[2]
	__attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x00000001f7011641UL, 0x0000000000000000UL },
		/* Barrett constant n */
		{ 0x00000001db710641UL, 0x0000000000000000UL }
	};
#else
static const __vector unsigned long v_Barrett_const[2]
	__attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x0000000104d101dfUL },
		/* Barrett constant n */
		{ 0x0000000000000000UL, 0x0000000104c11db7UL  }
	};

static const __vector unsigned long v_Barrett_reflect_const[2]
	 __attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x00000001f7011641UL },
		/* Barrett constant n */
		{ 0x0000000000000000UL, 0x00000001db710641UL }
	};
#endif

unsigned long __attribute__ ((aligned (32)))
barrett_reduction (unsigned long data){
	const __vector unsigned long vzero = {0,0};
	__vector unsigned long vconst1 = vec_ld(0, v_Barrett_const);
	__vector unsigned long vconst2 = vec_ld(16, v_Barrett_const);

	__vector unsigned long vdata, v0;

	unsigned long result = 0;

	/* Get (unsigned long) a into vdata */
	vdata = (__vector unsigned long)__builtin_pack_vector_int128(0UL, data);

	/*
	 * Now for the actual algorithm. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	/* ma */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long)vdata,
			(__vector unsigned long)vconst1);
	/* q = floor(ma/(2^64)) */
	v0 = (__vector unsigned long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v0, 8);
	/* qn */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long)v0,
			(__vector unsigned long)vconst2);
	/* a - qn, subtraction is xor in GF(2) */
	v0 = vec_xor (vdata, v0);
	/*
	 * Get the result into r3. We need to shift it left 8 bytes:
	 * V0 [ 0 1 2 X ]
	 * V0 [ 0 X 2 3 ]
	 */

	result = __builtin_unpack_vector_int128 ((vector __int128_t)v0, 1);

	return result;
}

unsigned long __attribute__ ((aligned (32)))
barrett_reduction_reflected (unsigned long data){
	const __vector unsigned long vzero = {0,0};
	const __vector unsigned long vones = {0xffffffffffffffffUL,
												0xffffffffffffffffUL};
	const __vector unsigned long vmask_32bit =
		(__vector unsigned long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 4);

	__vector unsigned long vconst1 = vec_ld(0, v_Barrett_reflect_const);
	__vector unsigned long vconst2 = vec_ld(16, v_Barrett_reflect_const);

	__vector unsigned long vdata, v0;

	unsigned long result = 0;

	/* Get (unsigned long) a into vdata */
	vdata = (__vector unsigned long)__builtin_pack_vector_int128(0UL, data);

	/*
	 * Now for the actual algorithm. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	/* bottom 32 bits of a */
	v0 = vec_and (vdata, vmask_32bit);
	/* ma */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long)v0,
			(__vector unsigned long)vconst1);
	/* bottom 32 bits of a */
	v0 = vec_and (v0, vmask_32bit);
	/* qn */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long)v0,
			(__vector unsigned long)vconst2);
	/* a - qn, subtraction is xor in GF(2) */
	v0 = vec_xor (vdata, v0);
	/*
		 * Since we are bit reflected, the result (ie the low 32 bits) is in the
		 * high 32 bits. We just need to shift it left 4 bytes
		 * V0 [ 0 1 X 3 ]
		 * V0 [ 0 X 2 3 ]
		 */
	v0 = (__vector unsigned long)vec_sld((__vector unsigned char)v0,
			(__vector unsigned char)vzero, 4);
	result = __builtin_unpack_vector_int128 ((vector __int128_t)v0, 0);

	return result;
}
