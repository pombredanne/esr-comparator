/* hashmap.c -- fast lookup for hash lists. */

#include <sys/types.h>
#include "shred.h"

/****************************************************************************

Fast hash-list lookup, using lots of memory.  Hash index is the first
two bytes of the MD5 digest plus some bits of the third byte; there are
131072 values.  With a typical data set size on the order of three
million chunks (approximate size of a Linux kernel in 2003) the table
will be 100% populated with a typical bucket depth of 22.

This data structure takes advantage of the fact that MD5 digests are 
cryptographically strong, e.g. they distribute input information evenly 
into the digest bytes.  This means that any byte of the hash makes a good
hash for the digest.

****************************************************************************/
 
typedef struct range_t {
    char	*file;
    linenum_t	start, end;
    struct range_t	*next;
}
range;

typedef struct head_t {
    unsigned char	hash[HASHSIZE-2];
    unsigned short	nranges;
    range		*ranges;
    struct bucket_t	*next;
}
head;

typedef struct bucket_t {
    unsigned char	hash[HASHSIZE-2];
    unsigned short	nranges;
    range		*ranges;
    struct bucket_t	*next;
    struct bucket_t	*last;
}
bucket;

/****************************************************************************

The central parameter of this data structure is how many bits beyond 16
to use for hashing.  Each additional bit doubles the hash table size and
halves the average length of the associated bucket chain.

Note: The HASH macro assumes we're on a twos-complement machine, in the
way it generates the mask for the bits in the third byte.

****************************************************************************/

#define EXTRABITS	2

static bucket headtable[256][256][1 << EXTRABITS];
#define HASH(h)	headtable[h[0]][h[1]][h[2] & (2 << (EXTRABITS-1)) - 1]

/****************************************************************************

Here is a GNU bc function for estimating density and total memory
usage for a given hash size, assuming perfect hash dispersion:

define memusage(datasize, extrabits) {
    auto tablesize, avg_depth

    scale = 4
    hashtail = 14
    hashbits = 16 + extrabits
    pointersize = 4			# 8 on 64-bit machines
    headsize   = 2 + (2 * pointersize) + hashtail
    bucketsize = 2 + (3 * pointersize) + hashtail
    tablesize = (2^hashbits)
    print "Table size: ", tablesize, " (using ", tablesize * headsize, " bytes).\n"
    density = datasize / tablesize	# Hash density
    print "Hash density: ", density, "\n"
    if (density > 1) {
        colliders = (density - 1) * datasize
    } else {
        colliders = 0
    }
    usage = tablesize * headsize + colliders * bucketsize
    print "Estimated memory used: ", usage
}

# Memory usage results for a typical dataset size
memusage(3000000, 0)
memusage(3000000, 1)
memusage(3000000, 2)

****************************************************************************/

void file_intern(const char *file)
{
    printf("Interning: %s\n", file);
}

void hash_intern(const struct hash_t *chunk)
{
    printf("Chunk: %d, %d\n", chunk->start, chunk->end);
}

/* hashmap.c ends here */
