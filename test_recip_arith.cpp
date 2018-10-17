/**
test_recip_arith.cpp
A Multi-Symbol Division Free Arithmetic Coder with Low Coding Loss using Reciprocal Multiplication

see:
https://github.com/thecbloom/recip_arith

copyright 2018 Charles Bloom
public domain
**/
#define _CRT_SECURE_NO_WARNINGS

//#include <assert.h>
// define assert or recip_arith_assert before including recip_arith.h

#include "recip_arith.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static uint8_t * read_whole_file(const char *name,size_t * pLength);

//=================================================================
//
// test functions with the cacm87 cdf-range map
//  not in recip_arith.h

// encode a symbol with a given cdf range
static recip_arith_inline void recip_arith_encoder_put_cacm87(recip_arith_encoder * ac,uint32_t cdf_low,uint32_t cdf_freq,uint32_t cdf_bits)
{
	recip_arith_assert( (cdf_low + cdf_freq) <= ((uint32_t)1<<cdf_bits) );
	recip_arith_assert( cdf_freq > 0 );
	recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );

	uint32_t save_low = ac->low;
	
	uint32_t lo = (uint32_t)( ((uint64_t)cdf_low * ac->range) >> cdf_bits);
	uint32_t hi = (uint32_t)( ((uint64_t)(cdf_low + cdf_freq) * ac->range) >> cdf_bits);
	ac->low += lo;
	ac->range = hi - lo;
	
    if ( ac->low < save_low ) recip_arith_encoder_carry(ac);
}

// peek finds the target cdf currently specified (mutates decoder)
static recip_arith_inline uint32_t recip_arith_decoder_peek_cacm87(recip_arith_decoder * ac,uint32_t cdf_bits)
{
	recip_arith_assert( ac->range >= ((uint32_t)1<<cdf_bits) );
	
	uint32_t target = (uint32_t)( ((((uint64_t)ac->code) << cdf_bits) + ((uint64_t)1<<cdf_bits) - 1 ) / ac->range );
	recip_arith_assert( target <= ((uint32_t)1<<cdf_bits) );
	return target;
}

// remove the symbol found by the previous call to peek
static recip_arith_inline void recip_arith_decoder_remove_cacm87(recip_arith_decoder * ac,uint32_t cdf_low,uint32_t cdf_freq,int cdf_bits)
{
	uint32_t lo = (uint32_t)( ((uint64_t)cdf_low * ac->range) >> cdf_bits);
	uint32_t hi = (uint32_t)( ((uint64_t)(cdf_low + cdf_freq) * ac->range) >> cdf_bits);
    ac->code -= lo;
    ac->range = hi - lo;
}

//=================================================================

int main(int argc,char * argv[])
{
	printf("test_recip_arith <file>\n");
	if ( argc != 2 ) return 1;

	recip_arith_table_init();

	size_t file_len;
	uint8_t * file_buf = read_whole_file(argv[1],&file_len);
	if ( file_buf == NULL )
	{
		printf("read_whole_file %s failed\n",argv[1]);
		return 1;
	}

	printf("loaded %s , len=%d\n",argv[1],(int)file_len);

	uint8_t * dec_buf = (uint8_t *) malloc(file_len);
	uint8_t * comp_buf = (uint8_t *) malloc(file_len + (file_len/8) + 4096);

	//-----------------------------------------
	
	const int cdf_bits = 13; // low enough to fit decode_table in L1
	const uint32_t cdf_tot = 1<<cdf_bits;
	
	//-----------------------------------------
	
	uint32_t histogram[256] = { };
	
	for(size_t i=0;i<file_len;i++) histogram[ file_buf[i] ] += 1;
	
	// normalize histo to cdf_bits
	// for correct normalization here : http://cbloomrants.blogspot.com/2014/02/02-11-14-understanding-ans-10.html
	// simple scheme here to keep the code small :
	
	uint32_t max_histo_val = 0;
	int max_histo_i = 0;
	uint32_t new_sum = 0;
	for(int i=0;i<256;i++)
	{
		uint32_t c = histogram[i];
		if ( c == 0 ) continue;
		if ( c > max_histo_val ) { max_histo_val = c; max_histo_i = i; }
		histogram[i] = (uint32_t)( ( ((uint64_t)c << cdf_bits) + (file_len/2) ) / file_len );
		if ( histogram[i] == 0 ) histogram[i] = 1;
		new_sum += histogram[i];
	}
	int32_t err = (int32_t)new_sum - cdf_tot;
	if ( err >= (int32_t)histogram[max_histo_i] )
	{
		printf("fail: can't normalize histogram with this simple scheme\n");
		return 10;
	}
	histogram[max_histo_i] -= err;
	
	// sum of histgoram is now cdf_tot
	
	uint32_t cdf[257];
	
	uint8_t * decode_table;
	decode_table = (uint8_t *)malloc(cdf_tot+1);
	
	cdf[0] = 0;
	for(int i=0;i<256;i++)
	{
		uint32_t lo = cdf[i];
		uint32_t hi = lo + histogram[i];
		cdf[i+1] = hi;
		for(uint32_t c=lo;c<hi;c++)
		{
			decode_table[c] = (uint8_t) i;
		}
	}
	// pad one extra slot at the end so that cdf target == cdf_tot is okay :
	decode_table[cdf_tot] = decode_table[cdf_tot-1];
	
	recip_arith_assert( cdf[256] == cdf_tot );
		
	//-----------------------------------------
	size_t comp_len_cacm87;
	size_t comp_len_rangecoder;
	size_t comp_len_reciparith;
	
	{	
	printf("cacm87:\n");

	recip_arith_encoder enc;
	recip_arith_encoder_start(&enc,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		int sym = file_buf[i];
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]
		
		recip_arith_encoder_put_cacm87(&enc,low,freq,cdf_bits);
		recip_arith_encoder_renorm(&enc);
	}
	
	uint8_t * comp_end = recip_arith_encoder_finish(&enc);

	comp_len_cacm87 = comp_end - comp_buf; 

	printf("comp_len : %d = %.3f bpb\n",(int)comp_len_cacm87,comp_len_cacm87*8.0/file_len);

	}
	//-----------------------------------------
	{
	
	recip_arith_decoder dec;
		
	recip_arith_decoder_start(&dec,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		
		uint32_t target = recip_arith_decoder_peek_cacm87(&dec,cdf_bits);
		uint8_t sym = decode_table[target];
		dec_buf[i] = sym;
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]		
		recip_arith_decoder_remove_cacm87(&dec,low,freq,cdf_bits);
		recip_arith_decoder_renorm(&dec);
	}
		
	int chk = memcmp(file_buf,dec_buf,file_len);
	recip_arith_assert(chk == 0 );
	printf("memcmp : %d\n",chk);
	memset(dec_buf,0,file_len);
	
	}
	//-----------------------------------------
	{	
	printf("range coder:\n");

	recip_arith_encoder enc;
	recip_arith_encoder_start(&enc,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		int sym = file_buf[i];
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]
		
		recip_arith_encoder_put_rangecoder(&enc,low,freq,cdf_bits);
		recip_arith_encoder_renorm(&enc);
	}
	
	uint8_t * comp_end = recip_arith_encoder_finish(&enc);

	comp_len_rangecoder = comp_end - comp_buf; 

	printf("comp_len : %d = %.3f bpb\n",(int)comp_len_rangecoder,comp_len_rangecoder*8.0/file_len);

	}
	//-----------------------------------------
	{
	
	recip_arith_decoder dec;
	
	recip_arith_decoder_start(&dec,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		
		uint32_t target = recip_arith_decoder_peek_rangecoder(&dec,cdf_bits);
		uint8_t sym = decode_table[target];
		dec_buf[i] = sym;
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]		
		recip_arith_decoder_remove_rangecoder(&dec,low,freq);
		recip_arith_decoder_renorm(&dec);
	}
	
	int chk = memcmp(file_buf,dec_buf,file_len);
	recip_arith_assert(chk == 0 );
	printf("memcmp : %d\n",chk);
	memset(dec_buf,0,file_len);
	
	}
	//-----------------------------------------
	{
	
	printf("recip_arith coder:\n");

	recip_arith_encoder enc;
	recip_arith_encoder_start(&enc,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		int sym = file_buf[i];
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]
		
		recip_arith_encoder_put(&enc,low,freq,cdf_bits);
		recip_arith_encoder_renorm(&enc);
	}
	
	uint8_t * comp_end = recip_arith_encoder_finish(&enc);

	comp_len_reciparith = comp_end - comp_buf; 

	printf("comp_len : %d = %.3f bpb\n",(int)comp_len_reciparith,comp_len_reciparith*8.0/file_len);

	}
	//-----------------------------------------
	{
	
	recip_arith_decoder dec;
		
	recip_arith_decoder_start(&dec,comp_buf);
	
	for(size_t i=0;i<file_len;i++) 
	{
		uint32_t target = recip_arith_decoder_peek(&dec,cdf_bits);
		uint8_t sym = decode_table[target];
		dec_buf[i] = sym;
		uint32_t low = cdf[sym];
		uint32_t freq = cdf[sym+1] - low; // == histogram[sym]		
		recip_arith_decoder_remove(&dec,low,freq);
		recip_arith_decoder_renorm(&dec);
	}
		
	int chk = memcmp(file_buf,dec_buf,file_len);
	recip_arith_assert(chk == 0 );
	printf("memcmp : %d\n",chk);
	memset(dec_buf,0,file_len);
	
	}
	//-----------------------------------------
	{
	
	recip_arith64_decoder dec;
		
	recip_arith64_decoder_start(&dec,comp_buf);
	
	recip_arith_assert( (3*cdf_bits) + RECIP_ARITH_TABLE_BITS <= 56 );
	
	uint8_t * pout = dec_buf;
	for(size_t i=0;i<(file_len/3);i++) 
	{
		uint64_t target = recip_arith64_decoder_peek(&dec,cdf_bits);
		uint64_t sym = decode_table[target];	
		pout[0] = (uint8_t) sym;
		recip_arith64_decoder_remove(&dec,cdf[sym],cdf[sym+1] - cdf[sym]);
		
		target = recip_arith64_decoder_peek(&dec,cdf_bits);
		sym = decode_table[target];	
		pout[1] = (uint8_t) sym;
		recip_arith64_decoder_remove(&dec,cdf[sym],cdf[sym+1] - cdf[sym]);
		
		target = recip_arith64_decoder_peek(&dec,cdf_bits);
		sym = decode_table[target];	
		pout[2] = (uint8_t) sym;
		recip_arith64_decoder_remove(&dec,cdf[sym],cdf[sym+1] - cdf[sym]);
		
		pout += 3;
		
		recip_arith64_decoder_renorm(&dec);
		// no advantage :
		//recip_arith64_decoder_renorm_branchless(&dec);
	}
	
	for(size_t i=0;i<(file_len%3);i++) 
	{
		uint64_t target = recip_arith64_decoder_peek(&dec,cdf_bits);
		uint64_t sym = decode_table[target];	
		*pout++ = (uint8_t) sym;
		recip_arith64_decoder_remove(&dec,cdf[sym],cdf[sym+1] - cdf[sym]);
		
		recip_arith64_decoder_renorm(&dec);
	}
		
	int chk = memcmp(file_buf,dec_buf,file_len);
	recip_arith_assert(chk == 0 );
	printf("memcmp : %d\n",chk);
	memset(dec_buf,0,file_len);
	
	}
	//-----------------------------------------

	printf("recip_arith coding loss: %.3f bpb\n",(comp_len_reciparith - comp_len_rangecoder)*8.0/file_len);

	free(file_buf);
	free(comp_buf);
	free(dec_buf);
	free(decode_table);

	return 0;
}



/*

read_whole_file : helper to read a file with stdio

*/

static uint8_t * read_whole_file(const char *name,size_t * pLength)
{
	FILE * fp = fopen(name,"rb");
	if ( ! fp )
		return NULL;
	
	fseek(fp,0,SEEK_END);
	
	size_t length;
	
	#ifdef _MSC_VER
	__int64 l64 = _ftelli64(fp);
	length = (size_t)l64;
	if ( (__int64)length != l64 ) { fclose(fp); return NULL; }
	#else
	// fgetpos64 or fstat64
	// warning : not 64-bit :
	length = ftell(fp);
	#endif

	if ( length <= 0 ) { fclose(fp); return NULL; }
	
	*pLength = length;	
		
	rewind(fp);

	uint8_t * data = (uint8_t *) malloc( (size_t)length );

	if ( data )
	{
		fread(data,1,(size_t)length,fp);
	}
		
	fclose(fp);
	
	return data;
}
