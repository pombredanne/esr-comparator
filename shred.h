/* shred.h -- types for shred similarity tools */

#include <sys/types.h>
#include <netinet/in.h>

#ifndef LARGEFILES
/*
 * 65,536 lines should be enough, but make it possible to compile with 32 bits.
 * The point of making it a short is that the data sets can get quite large,
 * so we want the hash_t and sorthash_t structures to be small.
 */
typedef u_int16_t	linenum_t;
#define TONET		htons
#define FROMNET		ntohs
#else
typedef u_int32_t	linenum_t;
#define TONET		htonl
#define FROMNET		ntohl
#endif
#define MAX_LINENUM	(linenum_t)-1	/* 2s-complement assumption */

/* use this to hold total line count of the entire source tree set */
typedef u_int32_t	linecount_t;

typedef unsigned char	flag_t;

#include "hash.h"

struct hash_t
{
    linenum_t   	start, end;
    hashval_t		hash;
    flag_t		flags;
#define INSIGNIFICANT	0x40	/* pure syntax in  a programming language */
#define C_CODE		0x01	/* identified as C code */
#define SHELL_CODE	0x02	/* identified as shell code */
#define CATEGORIZED	0x03	/* we can significance-test this */
#define INTERNAL_FLAG	0x80	/* internal use only */
};
#define SORTHASHCMP(s, t) hash_compare((s)->hash.hash, (t)->hash.hash)

struct filehdr_t
{
    char	*name;
    linenum_t	length;
    struct filehdr_t *next;
};

struct sorthash_t
{
    struct hash_t	hash;
    struct filehdr_t	*file;
};

typedef struct
{
    char	*text;
    int		flags;
}
feature_t;

/* control bits, meant to be set at startup */
extern int remove_braces;
extern int remove_whitespace;
extern int remove_comments;
extern int verbose, debug, nofilter;
extern int shredsize, minsize;

/* main.c functions */
extern void report_time(char *legend, ...);
struct filehdr_t *register_file(const char *file, linenum_t length);
extern void corehook(struct hash_t hash, struct filehdr_t *file);
extern void extend_current_chunk(int);
extern void dump_array(const char *legend,
		       struct sorthash_t *obarray, int hashcount);
extern void dump_flags(const int flags, FILE *fp);

/* shredtree.c functions */
extern char **sorted_file_list(const char *, int *);
extern int shredfile(struct filehdr_t *, 
		     void (*hook)(struct hash_t, struct filehdr_t *));
extern void sort_hashes(struct sorthash_t *hashlist, int hashcount);

/* linebyline.c functions */
extern void analyzer_init(void);
extern void analyzer_mode(int);
extern feature_t *analyzer_get(const struct filehdr_t *,
			       FILE *, linenum_t *linenumber);
extern void analyzer_free(const char *);

/* shredcompare.c functions */
extern int merge_compare(struct sorthash_t *obarray, int hashcount);
extern void emit_report(void);
extern int match_count(const char *name);
extern int line_count(const char *name);

/* shred.h ends here */
