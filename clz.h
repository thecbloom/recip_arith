#pragma once
/**
clz.h
clz32 & clz64

undefined on 0
returns in 0-31 , 0-63

copyright 2018 Charles Bloom
public domain
**/
#ifndef CLZ_H
#define CLZ_H

#ifdef _MSC_VER
#ifndef _STDINT

typedef unsigned char uint8_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#endif
#else
#include <stdint.h>
#endif


#ifdef _MSC_VER

#include <intrin.h>

#ifdef CLZ_PROCESSOR_HAS_LZCNT

// if lzcnt is available (ABM/Haswell), it is preferred :

#define clz32(val)	__lzcnt(val)
#define clz64(val)	__lzcnt64(val)

// Intel :
//#define clz32(val)	_lzcnt_u32(val)
//#define clz64(val)	_lzcnt_u64(val)

#else

extern "C" unsigned char _BitScanReverse(unsigned long* Index, unsigned long Mask);
#pragma intrinsic(_BitScanReverse)

static inline int clz_bsr32( uint32_t val )
{
	//assert( val != 0 );
    unsigned long b = 0;
    _BitScanReverse( &b, val );
    return (int)b;
}
 
#define clz32(val)	(31 - clz_bsr32(val))

#if (_M_IA64 || _M_AMD64)

extern "C" unsigned char _BitScanReverse64(unsigned long* Index, unsigned __int64 Mask);
#pragma intrinsic(_BitScanReverse64)

static inline int clz_bsr64( uint64_t val )
{
	//assert( val != 0 );
    unsigned long b = 0;
    _BitScanReverse64( &b, val );
    return (int)b;
}

#define clz64(val)	(63 - clz_bsr64(val))

#else

static inline int clz64( uint64_t val )
{
	if ( (val>>32) == 0 ) return 32 + clz32((uint32_t)val);
	else return clz32((uint32_t)(val>>32));
}

#endif // 64

#endif

#else // _MSC_VER

// assume non-MSVC is GCC/clang with builtin_clz :

#define clz32 __builtin_clz
#define clz64 __builtin_clzll

#endif

#endif // CLZ_H
