/* hash.c -- hash and digest computation for comparator */
#include <stdio.h>
#include "hash.h"

#ifndef FORCE_MD5
/******************************************************************************

According to the Birthday Theorem, successive random choices from a
set of size N will produces a collision with some previous choice
after about sqrt(2N) choices.  Thus a hash function returning B bytes
will produce a collision in about sqrt(2 * (256^B)) random choices.  the
bc(1) function to compute this is:

define collide(n) {
    return sqrt(2 * (256^n))
}

1       22
2       362
3       5792
4       92681
5       1482910
6       23726566
7       379625062
8       6074000999
9       97184015999
10      1554944255987
11      24879108095803
12      398065729532860
13      6369051672525772
14      101904826760412361
15      1630477228166597776
16      26087635650665564424

It follows that if we're expecting to be comparing codebases with
on the order of 10 million lines of code, we want at least a 6-byte
hash.  The nearest integral size that will contain this on a 32-bit
machine is 8 bytes, long long.

This was Ron Rivest's suggestion:

> Let n be an upper bound on the length of text you are hashing
> (e.g. 4 lines of 128 characters each = 512 characters).
>  
> Set up n tables of "random" 8-byte values, so T[i,j] is
> for 0<=i<512 and 0<=j<256 a random 8-byte value (e.g. the low order
> 8 bytes of MD5(i||j).  The table takes 2**17 = 128K bytes to
> store, and so will fit in cache.  (Here || is concatenation of
> string representations)
>  
> For a text string X[0]...X[k-1] of bytes define its hash to be
>  
>          T[0,X[0]] xor T[1,X[1]] xor ... xor T[k-1,X[k-1]]
>  
> This should be quite fast when coded carefully, and quite robust
> against collisions...  The table T could also be generated with
> a random "salt", e.g. T[i,j] = MD5(salt||i||j); if the salt is
> chosen at run time it would be essentially impossible even for
> an adversary to game the system, since he won't know the salt.

Other good material is at <http://burtleburtle.net/bob/hash/>.

****************************************************************************/

#include "hashtab.h"	/* table T of magic values */

/* following code only relies on hashval_t being an integral type */

static hashval_t hstate;
static int cind;

void hash_init(void)
{
    cind = 0;
    hstate = 0;
}

void hash_update(unsigned char *buffer, const int len)
/* update the hash */
{
    unsigned char *p;
    
    for (p = buffer; *p; p++)
	hstate ^= magicbits[*p][cind++ % sizeof(magicbits[0]) /
				sizeof(magicbits[0][0])];
}

void hash_complete(hashval_t *hp)
/* return the completed hash */
{
    *hp = hstate;
}

char *hash_dump(hashval_t hash)
/* dump a hash value in a human-readable form */
{
    static char buffer[33];

    snprintf(buffer, sizeof(buffer)-1, "%016llx", hash);
    return(buffer);
}

#else	/* use MD5 rather than the custom hash */
#include "md5.h"

static struct md5_ctx	ctx;

void hash_init(void)
{
    md5_init_ctx(&ctx);
}

void hash_update(unsigned char *buffer, const int len)
/* update the hash */
{
    md5_process_bytes(buffer, len, &ctx);
}

void hash_complete(hashval_t *hp)
/* return the completed hash */
{
    md5_finish_ctx(&ctx, (void *)hp);
}

char *hash_dump(hashval_t hash)
/* dump a hash value in a human-readable form */
{
    static char buffer[33];

    snprintf(buffer, sizeof(buffer)-1,
	    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    hash[0], 
	    hash[1], 
	    hash[2], 
	    hash[3], 
	    hash[4], 
	    hash[5], 
	    hash[6], 
	    hash[7], 
	    hash[8], 
	    hash[9], 
	    hash[10], 
	    hash[11], 
	    hash[12], 
	    hash[13], 
	    hash[14], 
	    hash[15]);
    return buffer;
}

#endif 

/* hash.c ends here */

