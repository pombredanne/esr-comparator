/* shred.h -- types for shred similarity tools */

/*
 * 65,536 lines should be enough, but maybe someday this will be 32 bits.
 * The point of making it a short is that the data sets can get quite large.
 */
typedef u_int16_t	linenum_t;
#define TONET		htons
#define FROMNET		ntohs

/*
 * The code depends on the assumption that this structure has 
 * no leading, trailing, or internal padding. This should be the 
 * case on all modern 8-bit-byte, byte-addressible machines,
 * whether 32 or 64 bit.
 */
#define HASHSIZE	16
struct hash_t
{
    linenum_t	start, end;
    char	hash[HASHSIZE];
};

/* hashmap.c */
void file_intern(const char *);
void hash_intern(const struct hash_t *);

/* shred.h ends here */
