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
#define HASHCMP(s, t)	memcmp((s)->hash.hash, (t)->hash.hash, HASHSIZE)

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

/* main.c functions */
extern void report_time(char *legend, ...);
extern void corehook(struct hash_t hash, const char *file);

/* shredtree.c functions */
extern char **sorted_file_list(const char *, int *);
extern void shredfile(const char *, void (*hook)(struct hash_t, const char *));
extern void sort_hashes(struct sorthash_t *hashlist, int hashcount);

/* shredcompare.c functions */
extern void emit_report(struct sorthash_t *obarray, int hashcount);

/* shred.h ends here */
