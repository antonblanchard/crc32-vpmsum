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
	__vector unsigned long long __v = {__a, __b};
	return __v;
}

unsigned long __builtin_unpack_vector (__vector unsigned long long __v,
									   int __o)
{
	return __v[__o];
}
#endif

#if defined(__LITTLE_ENDIAN__)
static const __vector unsigned long long vfold_const[5]
	__attribute__ ((aligned (16))) = {
		/* x^96 mod p(x) */
		{ 0x00000000f200aa66UL, 0x0000000000000000UL },
		/* x^64 mod p(x) */
		{ 0x00000000490d678dUL, 0x0000000000000000UL },
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000104d101dfUL, 0x0000000000000000UL },
		/* Barrett constant n */
		{ 0x0000000104c11db7UL, 0x0000000000000000UL },
		/* byte reverse permute constant, in LE order */
		{ 0x08090A0B0C0D0E0FUL, 0x0001020304050607UL }
	};

static const __vector unsigned long long vfold_reflect_const[5]
	__attribute__ ((aligned (16))) = {
		/* x^96 mod p(x)` << 1 */
		{ 0x00000000ccaa009eUL, 0x0000000000000000UL },
		/* x^64 mod p(x)` << 1 */
		{ 0x0000000163cd6124UL, 0x0000000000000000UL },
		/* 33 bit reflected Barrett constant m - (4^32)/n */
		{ 0x00000001f7011641UL, 0x0000000000000000UL },
		/* 33 bit reflected Barrett constant n */
		{ 0x00000001db710641UL, 0x0000000000000000UL },
		/* byte reverse permute constant, in LE order */
		{ 0x08090A0B0C0D0E0FUL, 0x0001020304050607UL }
	};
#else
static const __vector unsigned long long vfold_const[5]
	__attribute__ ((aligned (16))) = {
		/* x^96 mod p(x) */
		{ 0x0000000000000000UL, 0x00000000f200aa66UL },
		/* x^64 mod p(x) */
		{ 0x0000000000000000UL, 0x00000000490d678dUL },
		/* Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x0000000104d101dfUL },
		/* Barrett constant n */
		{ 0x0000000000000000UL, 0x0000000104c11db7UL },
		/* byte reverse permute constant, in BE order */
		{ 0x0F0E0D0C0B0A0908UL, 0X0706050403020100UL }
	};

static const __vector unsigned long long vfold_reflect_const[5]
	__attribute__ ((aligned (16))) = {
		/* x^96 mod p(x)` << 1 */
		{ 0x0000000000000000UL, 0x00000000ccaa009eUL },
		/* x^64 mod p(x)` << 1 */
		{ 0x0000000000000000UL, 0x0000000163cd6124UL },
		/* 33 bit reflected Barrett constant m - (4^32)/n */
		{ 0x0000000000000000UL, 0x00000001f7011641UL },
		/* 33 bit reflected Barrett constant n */
		{ 0x0000000000000000UL, 0x00000001db710641UL },
		/* byte reverse permute constant, in BE order */
		{ 0x0F0E0D0C0B0A0908UL, 0X0706050403020100UL }
	};
#endif

unsigned long __attribute__ ((aligned (32)))
final_fold(void* __restrict__ data) {
	const __vector unsigned long long vzero = {0,0};
	const __vector unsigned long long vones = {0xffffffffffffffffUL,
											0xffffffffffffffffUL};
	const __vector unsigned long long vmask_32bit =
		(__vector unsigned long long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 4);
	const __vector unsigned long long vmask_64bit =
		(__vector unsigned long long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 8);

	__vector unsigned long long vconst1 = vec_ld(0, vfold_const);
	__vector unsigned long long vconst2 = vec_ld(16, vfold_const);
	__vector unsigned long long vconst3 = vec_ld(32, vfold_const);
	__vector unsigned long long vconst4 = vec_ld(48, vfold_const);

	__vector unsigned long long vdata, v0, v1;

	unsigned long result = 0;

	vdata = vec_ld(0, (__vector unsigned long long*) data);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	__vector unsigned long long vperm_const = vec_ld(64, vfold_const);
	vdata = vec_perm (vdata, vdata, (__vector unsigned char)vperm_const);
#endif
	/*
	 * We append 32 bits of zeroes to our 128 bit value. This gives us 160
	 * bits that we reduce in two steps.
	 */

	/* Reduce the top 64 bits */
	v1 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)vdata, 8);
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst1);
	/* Add 32 bits of zeroes and xor with the reduced top 64 bits */
	v0 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vdata,
			(__vector unsigned char)vzero, 4);
	v0 = vec_xor (v1, v0);

	v1 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v0, 8);
	v1 = vec_and (v1, (__vector unsigned long long)vmask_32bit);
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst2);
	v0 = vec_xor (v1, v0);
	v0 = vec_and (v0, vmask_64bit);

	/*
	 * Now for Barrett reduction. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v0,
			(__vector unsigned long long)vconst3);
	v1 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v1, 8);
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst4);
	v0 = vec_xor (v1, v0);
	/*
	 * Get the result into r3. We need to shift it left 8 bytes:
	 * V0 [ 0 1 2 X ]
	 * V0 [ 0 X 2 3 ]
	 */
#if defined (__clang__)
	result = __builtin_unpack_vector (v0, 0);
#else
	result = __builtin_unpack_vector_int128 ((vector __int128_t)v0, 1);
#endif
	return result;
}

unsigned long  __attribute__ ((aligned (32)))
final_fold_reflected(void *__restrict__ data) {
	const __vector unsigned long long vzero = {0,0};
	const __vector unsigned long long vones = {0xffffffffffffffffUL,
											0xffffffffffffffffUL};
	const __vector unsigned long long vmask_32bit =
		(__vector unsigned long long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 4);
	const __vector unsigned long long vmask_64bit =
		(__vector unsigned long long)vec_sld((__vector unsigned char)vzero,
			(__vector unsigned char)vones, 8);

	__vector unsigned long long vconst1 = vec_ld(0, vfold_reflect_const);
	__vector unsigned long long vconst2 = vec_ld(16, vfold_reflect_const);
	__vector unsigned long long vconst3 = vec_ld(32, vfold_reflect_const);
	__vector unsigned long long vconst4 = vec_ld(48, vfold_reflect_const);

	__vector unsigned long long vdata, v0, v1;

	unsigned long result = 0;

	vdata = vec_ld(0, (__vector unsigned long long*) data);

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	__vector unsigned long long vperm_const = vec_ld(64, vfold_reflect_const);
	vdata = vec_perm (vdata, vdata, (__vector unsigned char)vperm_const);
#endif

	/*
	 * We append 32 bits of zeroes to our 128 bit value. This gives us 192
	 * bits that we reduce in two steps. This time we are reducing the
	 * bits on the right side (ie the lower bits) and xor'ing them
	 * on the left side.
	 */

	/* Reduce the top 64 bits */
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)vdata,
			(__vector unsigned long long)vconst1);
	v1 = (__vector unsigned long long)vec_sld ((__vector unsigned char)v1,
			(__vector unsigned char)vzero, 4);

	/* Add 32 bits of zeroes and xor with the reduced top 64 bits */
	v0 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)vdata, 12);
	v0 = vec_xor (v1, v0);

	/* We have a 96 bit value, now reduce the top 32 bits */
	v1 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v0, 12);
	v1 = vec_and (v1, (__vector unsigned long long)vmask_32bit);

	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst2);

	v0 = (__vector unsigned long long)vec_sld ((__vector unsigned char)vzero,
			(__vector unsigned char)v0, 8);
	v0 = vec_xor (v1, v0);
	v0 = vec_and (v0, vmask_64bit);

	/*
	 * Now for Barrett reduction. The idea is to calculate q,
	 * the multiple of our polynomial that we need to subtract. By
	 * doing the computation 2x bits higher (ie 64 bits) and shifting the
	 * result back down 2x bits, we round down to the nearest multiple.
	 */
	v1 = vec_and (v0, vmask_32bit);
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst3);
	v1 = vec_and (v1, (__vector unsigned long long)vmask_32bit);
	v1 = __builtin_crypto_vpmsumd ((__vector unsigned long long)v1,
			(__vector unsigned long long)vconst4);
	v0 = vec_xor (v0, v1);
	/*
	 * Get the result into r3. We need to shift it left 8 bytes:
	 * V0 [ 0 1 2 X ]
	 * V0 [ 0 X 2 3 ]
	 */
	v0 = (__vector unsigned long long)vec_sld ((__vector unsigned char)v0,
			(__vector unsigned char)vzero, 4);
#if defined (__clang__)
	result = __builtin_unpack_vector (v0, 1);
#else
	result = __builtin_unpack_vector_int128 ((vector __int128_t)v0, 0);
#endif
	return result;
}
