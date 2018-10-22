#pragma once
/**
recip_arith.h
A Multi-Symbol Division Free Arithmetic Coder with Low Coding Loss using Reciprocal Multiplication

see:
https://github.com/thecbloom/recip_arith

copyright 2018 Charles Bloom
public domain
**/
#ifndef RECIP_ARITH_H
#define RECIP_ARITH_H

#include "clz.h"

#define RECIP_ARITH_TABLE_BITS          (8)

// RECIP_ARITH_TABLE_BITS is the number of bits of "range" used in the cdf->range map
//  more bits = less coding loss , but the table takes more of L1
//  more bits also requires larger reciprocals to invert exactly

#define RECIP_ARITH_NUMERATOR_BITS      (32)

// RECIP_ARITH_NUMERATOR_BITS must be large enough for the reciprocal to
//   be exact for numerators up to (cdf_bits + RECIP_ARITH_TABLE_BITS)
//  it must also be small enough to fit in a u32

#define RECIP_ARITH_RANGE_MIN_BITS      (24)  // 32-bit range coder has 24-31 bits

// maximum cdf is limitted by what can fit in range, and by the reciprocal numerator precision :
#define RECIP_ARITH_MAX_CDF_BITS        min( RECIP_ARITH_RANGE_MIN_BITS , (RECIP_ARITH_NUMERATOR_BITS - RECIP_ARITH_TABLE_BITS) )

//=========================================================================================

// define recip_arith_assert & recip_arith_inline before including

#ifndef recip_arith_assert
#ifdef assert
#define recip_arith_assert assert
#else
#define recip_arith_assert(x)
#endif
#endif

#ifndef recip_arith_inline
#define recip_arith_inline __forceinline
#endif

//=========================================================================================

// reciprocal table must be filled out by calling recip_arith_table_init :
extern uint32_t recip_arith_table[(1<<RECIP_ARITH_TABLE_BITS)];

void recip_arith_table_init();

//=========================================================================================

/**

the arithmetic encoder specifies an internal in [low,low+range)

low & range could be 64 bit in the recip_arith scheme

**/

struct recip_arith_encoder
{
    uint32_t low,range;
    uint8_t * ptr;
};

static recip_arith_inline void recip_arith_encoder_start(recip_arith_encoder * ac,uint8_t * ptr)
{
    ac->low = 0;
    ac->range = ~(uint32_t)0;
    ac->ptr = ptr;
}

static recip_arith_inline void recip_arith_encoder_renorm(recip_arith_encoder * ac)
{
    // make range >= (1<<24) , stream out bytes where low == high
    while ( ac->range < (1<<24) )
    {
        *(ac->ptr)++ = (uint8_t)(ac->low>>24);
        ac->low <<= 8;
        ac->range <<= 8;
        // top bits of "low" are dropped as they go off the 32 bit word
        // note that if those top bits are FF
        // (low+range) could have been greater than 1<<32
        // this will be fixed later with carry propagation
    }
}

static recip_arith_inline void recip_arith_encoder_carry(recip_arith_encoder * ac)
{
    // propagate carry into the previous streamed bytes :
    uint8_t * p = ac->ptr;
    do {
        --p;
        *p += 1;
    } while( *p == 0 );
}

// _finish returns the end pointer
static recip_arith_inline uint8_t * recip_arith_encoder_finish(recip_arith_encoder * ac)
{
    // need to ensure that the interval in [low,low+range] is specified :
    if ( ac->range > (1<<25) )
    {
        // just one byte needed :
        uint32_t code = ac->low + (1<<24);
        if ( code < ac->low ) recip_arith_encoder_carry(ac);
        *ac->ptr++ = (uint8_t)(code>>24);
    }
    else
    {
        // two bytes needed : ; this is rare
        uint32_t code = ac->low + (1<<16);
        if ( code < ac->low ) recip_arith_encoder_carry(ac);
        *ac->ptr++ = (uint8_t)(code>>24);
        *ac->ptr++ = (uint8_t)(code>>16);
    }
    
    return ac->ptr;
}



//=========================================================================================

/**

arith decoder "code" is the arithmetic code value minus "low"

**/

struct recip_arith_decoder
{
    uint32_t code,range;
    uint8_t const * ptr;
};

static recip_arith_inline void recip_arith_decoder_start(recip_arith_decoder * ac,uint8_t const * ptr)
{
    ac->range = ~(uint32_t)0;
    // read 32 bits, big endian :
    ac->code = *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->ptr = ptr;
}

static recip_arith_inline void recip_arith_decoder_renorm(recip_arith_decoder * ac)
{
    // make range >= (1<<24) , stream out bytes where low == high
    while ( ac->range < (1<<24) )
    {
        ac->code <<= 8;
        ac->code |= *(ac->ptr)++;
        ac->range <<= 8;
    }
}

//=========================================================================================

/**

A Multi-Symbol Division Free Arithmetic Coder with Low Coding Loss using Reciprocal Multiplication

call recip_arith_table_init first

replaces division with reciprocal multiplication and shifts

encoder puts a cdf interval in [low,low+freq)
the cumulative probabilities should sum to a power of 2 (1<<cdf_bits)

the decoder requires two steps : peek & remove
"peek" finds the currently specified target in the cdf interval
you then need to find which symbol that cdf corresponds to
"remove" 
note that peek is mutating and should only be called once

you must call _renorm to input/output bytes
_renorm must be called often enough to ensure range has at least cdf_bits of precision

**/

// encode a symbol with a given cdf range
static recip_arith_inline void recip_arith_encoder_put(recip_arith_encoder * ac,uint32_t cdf_low,uint32_t cdf_freq,uint32_t cdf_bits)
{
    recip_arith_assert( (cdf_low + cdf_freq) <= ((uint32_t)1<<cdf_bits) );
    recip_arith_assert( cdf_freq > 0 );
    recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );

    uint32_t range = ac->range;
    int range_clz = clz32(range);

    uint32_t r_top = range >> (32 - range_clz - RECIP_ARITH_TABLE_BITS);
    uint32_t r_norm = r_top << (32 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
            
    uint32_t save_low = ac->low;    
    ac->low += cdf_low * r_norm;
    ac->range = cdf_freq * r_norm;
    
    if ( ac->low < save_low ) recip_arith_encoder_carry(ac);
}


// peek finds the target cdf currently specified (mutates decoder)
static recip_arith_inline uint32_t recip_arith_decoder_peek(recip_arith_decoder * ac,uint32_t cdf_bits)
{
    recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );
    recip_arith_assert( recip_arith_table[(1<<RECIP_ARITH_TABLE_BITS)-1] != 0 ); // call recip_arith_table_init

    uint32_t range = ac->range;
    int range_clz = clz32(range);

    uint32_t r_top = range >> (32 - range_clz - RECIP_ARITH_TABLE_BITS);
    uint32_t r_norm = r_top << (32 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
    
    // save r_norm for the "remove" step later :    
    ac->range = r_norm; 
        
    // map code back to target :

    // shift code down first :
    uint32_t code_necessary_bits = ac->code >> (32 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
    // code_necessary_bits is cdf_bits + RECIP_ARITH_TABLE_BITS
    // which is the same precision as the cdf's in the encoder after scaling by r_top
    //  that way when we do the reciprocal multiply we get back to those values exactly

    uint32_t target = (uint32_t)( ( code_necessary_bits * (uint64_t)recip_arith_table[r_top] ) >> RECIP_ARITH_NUMERATOR_BITS );

    //recip_arith_assert( target == code_necessary_bits / r_top );
    //recip_arith_assert( target == ac->code / r_norm );
        
    recip_arith_assert( target <= ((uint32_t)1<<cdf_bits) );
    return target;
}

// remove the symbol found by the previous call to peek
static recip_arith_inline void recip_arith_decoder_remove(recip_arith_decoder * ac,uint32_t cdf_low,uint32_t cdf_freq)
{
    uint32_t r_norm = ac->range;
    ac->code -= cdf_low * r_norm;
    ac->range = cdf_freq * r_norm;
}


//=========================================================================================

/**

classic "range coder" for reference :

see Michael Schindler
http://www.compressconsult.com/rangecoder/

requires division in the decoder

encoder puts a cdf interval in [low,low+freq)
the cumulative probabilities should sum to a power of 2 (1<<cdf_bits)

the decoder requires two steps : peek & remove
"peek" finds the currently specified target in the cdf interval
you then need to find which symbol that cdf corresponds to
"remove" 
note that peek is mutating and should only be called once

you must call _renorm to input/output bytes
_renorm must be called often enough to ensure range has at least cdf_bits of precision

**/

// encode a symbol with a given cdf range
static recip_arith_inline void recip_arith_encoder_put_rangecoder(recip_arith_encoder * ac,uint32_t cdf_low,uint32_t cdf_freq,uint32_t cdf_bits)
{
    recip_arith_assert( (cdf_low + cdf_freq) <= ((uint32_t)1<<cdf_bits) );
    recip_arith_assert( cdf_freq > 0 );
    recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );

    uint32_t save_low = ac->low;
    
    uint32_t r_norm = ac->range >> cdf_bits;
    ac->low += cdf_low * r_norm;
    ac->range = cdf_freq * r_norm;
    
    if ( ac->low < save_low ) recip_arith_encoder_carry(ac);
}

// peek finds the target cdf currently specified (mutates decoder)
static recip_arith_inline uint32_t recip_arith_decoder_peek_rangecoder(recip_arith_decoder * ac,uint32_t cdf_bits)
{
    recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );
    
    uint32_t r_norm = ac->range >> cdf_bits;
    uint32_t target = ac->code / r_norm;
    ac->range = r_norm; // store for "remove" stage
    recip_arith_assert( target <= ((uint32_t)1<<cdf_bits) );
    return target;
}

// remove the symbol found by the previous call to peek
static recip_arith_inline void recip_arith_decoder_remove_rangecoder(recip_arith_decoder * ac,uint32_t cdf_low,uint32_t cdf_freq)
{
    uint32_t r_norm = ac->range; // == range >> cdf_bits
    ac->code -= cdf_low * r_norm;
    ac->range = cdf_freq * r_norm;
}

//=========================================================================================

/**

arith decoder "code" is the arithmetic code value minus "low"

**/

struct recip_arith64_decoder
{
    uint64_t code,range;
    uint8_t const * ptr;
};

static recip_arith_inline void recip_arith64_decoder_start(recip_arith64_decoder * ac,uint8_t const * ptr)
{
    ac->range = ~(uint32_t)0;
    ac->range <<= 32;
    
    // read 64 bits, big endian :
    ac->code = *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    ac->code <<= 8; ac->code |= *ptr++;
    
    ac->ptr = ptr;
}

static recip_arith_inline void recip_arith64_decoder_renorm(recip_arith64_decoder * ac)
{
    while ( ac->range < ((uint64_t)1<<56) )
    {
        ac->code <<= 8;
        ac->code |= *(ac->ptr)++;
        ac->range <<= 8;
    }
}

//=========================================================================================

// peek finds the target cdf currently specified (mutates decoder)
static recip_arith_inline uint32_t recip_arith64_decoder_peek(recip_arith64_decoder * ac,uint32_t cdf_bits)
{
    recip_arith_assert( ac->range >= ((uint64_t)1<<cdf_bits) );
    recip_arith_assert( recip_arith_table[(1<<RECIP_ARITH_TABLE_BITS)-1] != 0 ); // call recip_arith_table_init

    uint64_t range = ac->range;
    int range_clz = clz64(range);

    uint64_t r_top = range >> (64 - range_clz - RECIP_ARITH_TABLE_BITS);
    uint64_t r_norm = r_top << (64 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
    
    // save r_norm for the "remove" step later :    
    ac->range = r_norm; 
        
    // map code back to target :

    // shift code down first :
    uint64_t code_necessary_bits = ac->code >> (64 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
    // code_necessary_bits is cdf_bits + RECIP_ARITH_TABLE_BITS
    // which is the same precision as the cdf's in the encoder after scaling by r_top
    //  that way when we do the reciprocal multiply we get back to those values exactly

    uint32_t target = (uint32_t)( ( code_necessary_bits * recip_arith_table[r_top] ) >> RECIP_ARITH_NUMERATOR_BITS );
        
    recip_arith_assert( target <= ((uint32_t)1<<cdf_bits) );
    return target;
}

// remove the symbol found by the previous call to peek
static recip_arith_inline void recip_arith64_decoder_remove(recip_arith64_decoder * ac,uint32_t cdf_low,uint32_t cdf_freq)
{
    uint64_t r_norm = ac->range;
    ac->code -= cdf_low * r_norm;
    ac->range = cdf_freq * r_norm;
}


//=========================================================================================

#endif // RECIP_ARITH_H
