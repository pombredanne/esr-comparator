/* shred.h -- types for shred similarity tools */

#include <sys/types.h>

/*
 * 65,536 lines should be enough, but maybe someday this will be 32 bits.
 * The point of making it a short is that the data sets can get quite large,
 * so we want the hash_t and sorthash_t structures to be small.
 */
typedef u_int16_t	linenum_t;
#define TONET		htons
#define FROMNET		ntohs
#define UNIQUE_FLAG	(linenum_t)-1	/* 2s-complement assumption */

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
extern int remove_braces;
extern int remove_whitespace;
extern int remove_comments;
extern int debug;
extern int shredsize;

/* main.c functions */
extern void report_time(char *legend, ...);
extern void corehook(struct hash_t hash, const char *file);

/* shredtree.c functions */
extern char **sorted_file_list(const char *, int *);
extern int shredfile(const char *, void (*hook)(struct hash_t, const char *));
extern void sort_hashes(struct sorthash_t *hashlist, int hashcount);

/* shredcompare.c functions */
extern void emit_report(struct sorthash_t *obarray, int hashcount);

/* shred.h ends here */
