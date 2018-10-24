# recip_arith
A Multi-Symbol Division Free Arithmetic Coder with Low Coding Loss using Reciprocal Multiplication

## Getting Started

You should be able to compile and run test_recip_arith.cpp ; test_recip_arith requires a file as the first command line argument.

## Synopsis

recip_arith uses an approximation of the cdf->range map required by arithmetic coding.  This allows for exact inversion of the divide needed by the reverse map using a small reciprocal table.

The result is a simple and highly efficient generic (multi-symbol) arithmetic coder without division.

The standard range coder does :

```
	uint32_t r_norm = ac->range >> cdf_bits;

	ac->low += cdf_low * r_norm;
	ac->range = cdf_freq * r_norm;
```

while recip_arith does :

```
	uint32_t range = ac->range;

	int range_clz = clz32(range);
	uint32_t r_top = range >> (32 - range_clz - RECIP_ARITH_TABLE_BITS);
	uint32_t r_norm = r_top << ( 32 - range_clz - RECIP_ARITH_TABLE_BITS - cdf_bits);
			
	ac->low += cdf_low * r_norm;
	ac->range = cdf_freq * r_norm;
```

For more, see [blog series at cbloomrants](http://cbloomrants.blogspot.com/2018/10/10-16-18-multi-symbol-division-free.html) which describes
recip_arith and the algorithms used in detail.

## Coding Efficiency

The coding loss of "recip_arith" with 8 bit tables is reliably under 0.005 bpb (around 0.1%) , in contrast to SM98 where the coding loss can be 10X
higher (0.05 bpb or 1.0%)

```
Calgary Corpus file "news" , len=377,109

cdf_bits = 13  (symbol frequencies sum to 1<<13)

from smallest to largest output :

recip_arith coder (down/up 8-bit) :                                244,641 = 5.190 bpb

cacm87:                                                            244,642 = 5.190 bpb

range coder:                                                       244,645 = 5.190 bpb

recip_arith coder (with end of range map) :                        244,736 = 5.192 bpb

recip_arith coder:                                                 244,825 = 5.194 bpb

recip_arith coder (down/up 2-bit) (aka Daala "reduced overhead") : 245,488 = 5.208 bpb

SM98 :                                                             245,690 = 5.212 bpb
```

## Contents

recip_arith.h and recip_arith.cpp are the implementation.

clz.h is used to separate out compiler-dependent access to a count-leading-zeros intrinsic

stdint.h should be included before recip_arith.h

recip_arith_table_init() must be called before decoding.

test_recip_arith.cpp is an example demonstrating usage.

## Snark

No Google, you can't patent this.  And yes we know it can be used for video coding, in LZ77, for binary arithmetic coding, etc.  All the ways an arithmetic coder can be used, this can be used, hands off!

Some common variants you might consider :

Adaptive cumulative probabilities with Deferred Summation, "constant sum shift" nibble models.  Binary coding, nibble coding, byte coding.

The real defining characteristic of "recip_arith" is the way the cdf is scaled up to range.  Other implementation details may vary.  For example,
bit, byte, and word renormalization are all possible.  Arithmetic code interval can be 32 or 64 bit or other.  The arithmetic interval can be
stored as {low,range} or {low,high}.  Renormalization can use branches or be branchless.

## License

This project is public domain.  (MIT License)

## Acknowledgments

* Thanks to [RAD Game Tools](http://www.radgametools.com/) for supporting this work.
* Fabian Giesen was instrumental in developing this, as usual.

