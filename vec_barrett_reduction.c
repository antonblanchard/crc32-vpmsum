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

/*
 * Those stubs fix clang incompatibilitie issues with GCC builtins.
 */
#if defined (__clang__)
#define __builtin_crypto_vpmsumw __builtin_crypto_vpmsumb
#define __builtin_crypto_vpmsumd __builtin_crypto_vpmsumb

__vector unsigned long long __attribute__((overloadable))
vec_ld(int __a, const __vector unsigned long long* __b)
{
	return (__vector unsigned long long)__builtin_altivec_lvx(__a, __b);
}

/*
 * GCC __builtin_pack_vector_int128 returns a vector __int128_t but Clang
 * seems to not recognize this type. On GCC this builtin is translated to a
 * xxpermdi instruction that only move the registers __a, __b instead generates
 * a load. Clang doesn't have this builtin or xxpermdi intrinsics. Was recently
 * implemented https://reviews.llvm.org/rL303760.
 * */
__vector unsigned long long  __builtin_pack_vector (unsigned long __a,
												    unsigned long __b)
{
	__vector unsigned long long __v = {__b, __a};
	return __v;
}

#ifndef vec_xxpermdi

unsigned long __builtin_unpack_vector (__vector unsigned long long __v,
									   int __o)
{
	return __v[__o];
}

#define __builtin_unpack_vector_0(a) __builtin_unpack_vector ((a), 1)
#define __builtin_unpack_vector_1(a) __builtin_unpack_vector ((a), 0)

#else

/*__vector unsigned long long  __builtin_pack_vector (unsigned long __a, unsigned long __b)
{
    __vector unsigned long long __av = { __a }, __bv = { __b };
    return vec_xxpermdi(__bv, __av, 0x00);
}*/

unsigned long __builtin_unpack_vector_0 (__vector unsigned long long __v)
{
	return vec_xxpermdi(__v, __v, 0x0)[0];
}

unsigned long __builtin_unpack_vector_1 (__vector unsigned long long __v)
{
	return vec_xxpermdi(__v, __v, 0x3)[0];
}
#endif /* vec_xxpermdi */

#else /* clang */

#define __builtin_pack_vector(a, b)  __builtin_pack_vector_int128 ((a), (b))
#define __builtin_unpack_vector_0(a) __builtin_unpack_vector_int128 ((vector __int128_t)(a), 0)
#define __builtin_unpack_vector_1(a) __builtin_unpack_vector_int128 ((vector __int128_t)(a), 1)

#endif

#if defined(__LITTLE_ENDIAN__)
static const __vector unsigned long long v_Barrett_const[2]
	 __attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000104d101dfUL, 0x0000000000000000UL },
		/* Barrett constant n */
		{ 0x0000000104c11db7UL, 0x0000000000000000UL }
	};

static const __vector unsigned long long v_Barrett_reflect_const[2]
	__attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x00000001f7011641UL, 0x0000000000000000UL },
		/* Barrett constant n */
		{ 0x00000001db710641UL, 0x0000000000000000UL }
	};
#else
static const __vector unsigned long long v_Barrett_const[2]
	__attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x0000000104d101dfUL },
		/* Barrett constant n */
		{ 0x0000000000000000UL, 0x0000000104c11db7UL  }
	};

static const __vector unsigned long long v_Barrett_reflect_const[2]
	 __attribute__ ((aligned (16))) = {
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x00000001f7011641UL },
		/* Barrett constant n */
		{ 0x0000000000000000UL, 0x00000001db710641UL }
	};
#endif

unsigned long __attribute__ ((aligned (32)))
barrett_reduction (unsigned long data){
	const __vector unsigned long long vzero = {0,0};

	__vector unsigned long long vconst1 = vec_ld(0, v_Barrett_const);
	__vector unsigned long long vconst2 = vec_ld(16,v_Barrett_const);

	__vector unsigned long long v0;

	unsigned long result = 0;

	/* Get (unsigned long) a into vdata */
	__vector unsigned long long vdata = (__vector unsigned long long)
			__builtin_pack_vector(0UL, data);
	/*
	 * Now for the actual algorithm. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	/* ma */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long long)vdata,
			(__vector unsigned long long)vconst1);
	/* q = floor(ma/(2^64)) */
	v0 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v0, 8);
	/* qn */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v0,
			(__vector unsigned long long)vconst2);
	/* a - qn, subtraction is xor in GF(2) */
	v0 = vec_xor (vdata, v0);
	/*
	 * Get the result into r3. We need to shift it left 8 bytes:
	 * V0 [ 0 1 2 X ]
	 * V0 [ 0 X 2 3 ]
	 */
	result = __builtin_unpack_vector_1(v0);

	return result;
}

unsigned long __attribute__ ((aligned (32)))
barrett_reduction_reflected (unsigned long data){

	const __vector unsigned long long vzero = {0,0};
	const __vector unsigned long long vones = {0xffffffffffffffffUL,
											   0xffffffffffffffffUL};

	const __vector unsigned long long vmask_32bit =
		(__vector unsigned long long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 4);

	__vector unsigned long long vconst1 = vec_ld(0, v_Barrett_reflect_const);
	__vector unsigned long long vconst2 = vec_ld(16,v_Barrett_reflect_const);

	__vector unsigned long long v0;

	unsigned long result = 0;

	/* Get (unsigned long) a into vdata */
	__vector unsigned long long vdata = (__vector unsigned long long)
			__builtin_pack_vector(0UL, data);
	/*
	 * Now for the actual algorithm. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	/* bottom 32 bits of a */
	v0 = vec_and (vdata, vmask_32bit);
	/* ma */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long long)vdata,
			(__vector unsigned long long)vconst1);
	/* bottom 32 bits of a */
	v0 = vec_and (v0, vmask_32bit);
	/* qn */
	v0 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v0,
			(__vector unsigned long long)vconst2);
	/* a - qn, subtraction is xor in GF(2) */
	v0 = vec_xor (vdata, v0);
	/*
		 * Since we are bit reflected, the result (ie the low 32 bits) is in the
		 * high 32 bits. We just need to shift it left 4 bytes
		 * V0 [ 0 1 X 3 ]
		 * V0 [ 0 X 2 3 ]
		 */
	v0 = (__vector unsigned long long )vec_sld((__vector unsigned char)v0,
			(__vector unsigned char)vzero, 4);
	result = __builtin_unpack_vector_0(v0);

	return result;
}
