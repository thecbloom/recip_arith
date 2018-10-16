#include "recip_arith.h"

// zero recip_arith_table so we can assert that recip_arith_table_init as called
uint32_t recip_arith_table[(1<<RECIP_ARITH_TABLE_BITS)] = { };

void recip_arith_table_init()
{	
	// first half of table is empty :
	for(int i=((1<<RECIP_ARITH_TABLE_BITS)/2);i<(1<<RECIP_ARITH_TABLE_BITS);i++)
	{
		// ceil reciprocal :
		uint64_t val = ( (((uint64_t)1<<RECIP_ARITH_NUMERATOR_BITS) + i-1) / (i) );
		recip_arith_assert( (val>>32) == 0 ); // so we can store in u32
		recip_arith_table[i] = (uint32_t)val;
	}
}
