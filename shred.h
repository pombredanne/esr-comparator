/* shred.h -- types for shred similarity tools */

#include <sys/types.h>

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
    linenum_t   	start, end;
    unsigned char	hash[HASHSIZE];
};

struct sorthash_t
{
    struct hash_t	hash;
    char		*file;
};

/* control bits, meant to be set at startup */
extern int c_only;
extern int rws;
extern int debug;
extern int shredsize;

/* globally visible data; chunk_buffer should be a malloc area set by the caller */
extern int file_count, chunk_count;
extern struct hash_t *chunk_buffer;

/* functions */
extern void generate_shredfile(const char *, FILE *);
extern struct sorthash_t *generate_shredlist(const int, const char *argv[]);
extern void shredreport(int argc, char *argv[]);

/* shred.h ends here */
