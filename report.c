/* shredcompare.c -- search hash lists for common matches */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "shred.h"

#define min(x, y)	((x < y) ? (x) : (y)) 
#define max(x, y)	((x > y) ? (x) : (y)) 

struct item
{
    char	*file;
    struct item *next;
}
dummy_item;
static struct item *head = &dummy_item;

struct shif_t
{
    char	*name;
    FILE	*fp;
    char	*normalization;
    int		shred_size;
    char	*hash_method;
    char	*generator_program;
    int		hashcount;
};
static struct shif_t *shiflist;

struct sorthash_t
{
    struct hash_t	hash;
    char		*file;
};
#define HASHCMP(s, t)	memcmp((s)->hash.hash, (t)->hash.hash, HASHSIZE)

static struct sorthash_t *obarray, *np;
static int hashcount;

struct range_t
{
    char	*file;
    linenum_t	start, end;
};

struct match_t
{
    struct match_t *next;
    int            nmatches;
    struct range_t *matches;
#ifdef DEBUG
    int	index;
#endif /* DEBUG */
}
dummy_match;
static struct match_t *reduced = &dummy_match;

static void report_time(char *legend, ...)
{
    static time_t mark_time;
    time_t endtime = time(NULL);
    va_list	ap;

    if (mark_time)
    {
	int elapsed = endtime - mark_time;
	int hours = elapsed/3600; elapsed %= 3600;
	int minutes = elapsed/60; elapsed %= 60;
	int seconds = elapsed;
	char	buf[BUFSIZ];

	va_start(ap, legend);
	vsprintf(buf, legend, ap);
	fprintf(stderr, "%% %s: %dh %dm %ds\n", buf, hours, minutes, seconds);
    }
    mark_time = endtime;
}

void init_intern(const int count)
/* prepare an alllocation area of a given size */
{
    np=obarray=(struct sorthash_t *)calloc(sizeof(struct sorthash_t),count);
    if (!np)
    {
	fprintf(stderr, "shredcompare: insufficient memory\n");
    }
}

 void file_intern(const char *file)
 /* stash the name of a file */
 {
     struct item *new;

     new = (struct item *)malloc(sizeof(struct item));
     new->file = strdup(file);
     new->next = head;
     head = new;
 }

 void hash_intern(const struct hash_t *chunk)
 {
     np->file = head->file;
     np->hash = *chunk;
     np++;
 }

 static void merge_hashes(int argc, char *argv[])
 /* merge hashes from specified files into an in-code list */
 {
     struct shif_t *sp;
     struct stat sb;
     long total;

     /* set up metadata blocks for the hash files */
     shiflist = (struct shif_t *)calloc(sizeof(struct shif_t), argc);
     for (sp = shiflist; sp < shiflist + argc; sp++)
     {
	 char	buf[BUFSIZ];

	 sp->name = strdup(argv[sp - shiflist]);
	 sp->fp   = fopen(sp->name, "r");
	 fgets(buf, sizeof(buf), sp->fp);
	 if (strncmp(buf, "#SHIF-A ", 8))
	 {
	     fprintf(stderr, 
		     "shredcompare: %s is not a SHIF-A file.", 
		     sp->name);
	     exit(1);
	 }
	 while (fgets(buf, sizeof(buf), sp->fp) != NULL)
	 {
	     char	*value;

	     if (!strcmp(buf, "%%\n"))
		 break;
	     value = strchr(buf, ':');
	     *value++ = '\0';
	     while(*value == ' ')
		 value++;
	     strchr(value, '\n')[0] = '\0';

	     if (!strcmp(buf, "Normalization"))
		 sp->normalization = strdup(value);
	     else if (!strcmp(buf, "Shred-Size"))
		 sp->shred_size = atoi(value);
	     else if (!strcmp(buf, "Hash-Method"))
		 sp->hash_method = strdup(value);
	     else if (!strcmp(buf, "Generator-Program"))
		 sp->generator_program = strdup(value);
	 }
     }

     /* consistency checks */
     for (sp = shiflist; sp < shiflist + argc-1; sp++)
     {
	 struct shif_t	*next = sp + 1;

	 if (strcmp(sp->normalization, next->normalization))
	 {
	     fprintf(stderr, 
		     "shredcompare: normalizations of %s and %s don't match\n",
		     sp->name, next->name);
	     exit(1);
	 }
	 else if (sp->shred_size != next->shred_size)
	 {
	     fprintf(stderr, 
		     "shredcompare: shred sizes of %s and %s don't match\n",
		     sp->name, next->name);
	     exit(1);

	 }
	 else if (strcmp(sp->hash_method, next->hash_method))
	 {
	     fprintf(stderr, 
		     "shredcompare: hash methods of %s and %s don't match\n",
		     sp->name, next->name);
	     exit(1);
	 }
     }

     /* compute total data to be read, we'll use this for the progress meter */
     total = 0;
     for (sp = shiflist; sp < shiflist + argc; sp++)
     {
	 stat(sp->name, &sb);
	 total += sb.st_size - ftell(sp->fp);
     }
     init_intern(total/sizeof(struct hash_t));	/* real work done here */

     /* read in all hashes */
     hashcount = 0;
     for (sp = shiflist; sp < shiflist + argc; sp++)
     {
	 u_int32_t	sectcount;

	 fprintf(stderr, "Reading %s...   ", sp->name);
	 fread(&sectcount, sizeof(u_int32_t), 1, sp->fp);
	 sectcount = ntohl(sectcount);
	 while (sectcount--)
	 {
	     char	buf[BUFSIZ];
	     linenum_t	chunks;

	     fgets(buf, sizeof(buf), sp->fp);
	     *strchr(buf, '\n') = '\0';
	     file_intern(buf);			/* real work done here */
	     fread(&chunks, sizeof(linenum_t), 1, sp->fp);
	     chunks = FROMNET(chunks);

	     while (chunks--)
	     {
		 struct hash_t	this;

		 fread(&this, sizeof(struct hash_t), 1, sp->fp);
		 this.start = FROMNET(this.start);
		 this.end = FROMNET(this.end);
		 hash_intern(&this);		/* real work done here */
		 hashcount++;
		 sp->hashcount++;
		 if (hashcount % 10000 == 0)
		     fprintf(stderr, "\b\b\b%02.0f%%", (ftell(sp->fp) / (total * 0.01)));
	     }
	 }
	 fprintf(stderr, "\b\b\b100%%...done, %d entries\n", sp->hashcount);
     }
 }

static int merge_ranges(struct range_t *p, struct range_t *q, int nmatches)
/* merge t into s, if the ranges in the match are compatible */
{
    int	i;
    /*
     * The general problem: you have two lists of shreds, of the same
     * lengths.  Within each list, all shreds have the same hash.
     * Within the lists, the shreds are sorted in filename order.
     * 
     * First, check to see that all the filenames match pairwise.
     * If there are no such matches, these chunks are completely
     * irrelevant to each other.  It might be that for some values
     * of i the filenames are equal and for others not.  In that case
     * the pair of lists of ranges cannot represent the same overlapping
     * segments of text, which is the only case we are interested in.
     */
    for (i = 0; i < nmatches; i++)
	if (strcmp(p[i].file, q[i].file))
	{
#ifdef DEBUG
	    printf("File mismatch\n");
#endif /* DEBUG */
	    return(0);
	}
 
    /*
     * There are two possible overlap cases.  Either the start line of s is
     * within t or vice-versa.  In either case, all shreds must overlap by
     * the same offset for this to be an eligible match.
     */
    if (p->start >= q->start && p->start <= q->end)
    {
	int offset = p->start - q->start;

	for (i = 1; i < nmatches; i++)
	    if (p[i].start - q[i].start != offset)
	    {
#ifdef DEBUG
		printf("Mismatched offset (A)\n");
#endif /* DEBUG */
		return(0);
	    }
    }
    else if (q->start >= p->start && q->start <= p->end)
    {
	int offset = q->start - p->start;

	for (i = 1; i < nmatches; i++)
	    if (q[i].start - p[i].start != offset)
	    {
#ifdef DEBUG
		printf("Mismatched offset (B)\n");
#endif /* DEBUG */
		return(0);
	    }
    }
    else
    {
#ifdef DEBUG
	printf("Intervals don't intersect\n");
#endif /* DEBUG */
	return(0);
    }

    /* merge attempt successful */
    for (i = 0; i < nmatches; i++)
    {
	p[i].start = min(p[i].start, q[i].start);
	p[i].end   = max(p[i].end, q[i].end);
    }
    return(1);
}

static int sametree(const char *s, const char *t)
/* are two files from the same tree? */
{
    int sn = strchr(s, '/') - s;
    int tn = strchr(t, '/') - t;

    return (sn == tn) && strncmp(s, t, sn); 
}

struct match_t *reduce_matches(void)
/* assemble list of duplicated hashes */
{
     static struct match_t dummy; 
     struct match_t *reduced = &dummy, *sp, *tp;
     int retry, nonuniques;

     /* build list of hashes with more than one range associated with */
     nonuniques = 0;
     for (np = obarray; np < obarray + hashcount; np++)
     {
	 if (np < obarray + hashcount - 1 && !HASHCMP(np, np+1))
	 {
	     struct match_t *new;
	     int i, heterogenous, nmatches;
	     struct sorthash_t *mp;

	     /* count the number of hash matches */
	     nmatches = 1;
	     for (mp = np+1; mp < obarray + hashcount; mp++)
		 if (HASHCMP(np, mp))
		     break;
		 else
		     nmatches++;

	     /* if all these matches are within the same tree, toss them */
	     heterogenous = 0;
	     for (i = 0; i < nmatches; i++)
		 if (sametree(np[i].file, np[(i+1) % nmatches].file))
		     heterogenous++;
	     if (!heterogenous)
		 continue;

#ifdef ODEBUG
	     printf("*** %d has %d in its clique\n", np-obarray, nmatches);
	     for (i = 0; i < nmatches; i++)
		 printf("%d: %s:%d:%d\n", 
			np-obarray+i, np[i].file, np[i].hash.start, np[i].hash.end);
#endif /* ODEBUG */
	     /* passed all tests, keep this set of ranges */
	     new = (struct match_t *)malloc(sizeof(struct match_t));
#ifdef DEBUG
	     new->index = np - obarray;
#endif /* DEBUG */
	     new->next  = reduced;
	     reduced = new;
	     new->nmatches = nmatches;
	     new->matches=(struct range_t *)calloc(sizeof(struct range_t),new->nmatches);
	     nonuniques++;
	     for (i = 0; i < new->nmatches; i++)
	     {
		 new->matches[i].file  = np[i].file;
		 new->matches[i].start = np[i].hash.start;
		 new->matches[i].end   = np[i].hash.end;
	     }
	     np = mp-1;
	}
     }
     free(obarray);

     report_time("%d range groups after removing unique hashes", nonuniques);

#ifdef DEBUG
     for (sp = reduced; sp->next; sp = sp->next)
     {
	 struct range_t	*rp;

	 printf("Clique beginning at %d:\n", sp->index);
	 for (rp = sp->matches; rp < sp->matches + sp->nmatches; rp++)
	     printf("%s:%d:%d\n",  rp->file, rp->start, rp->end);
     }
#endif /* DEBUG */

     /* time to remove duplicates */
     do {
	 retry = 0;
	 for (sp = reduced; sp->next; sp = sp->next)
	     for (tp = reduced; tp->next; tp = tp->next)
	     {
		 /* intersection is symmetrical */
		 if (sp >= tp)
		     continue;
#ifdef DEBUG
		 printf("Trying merge of %d into %d\n", tp->index, sp->index);
#endif /* DEBUG */
		 /* neither must have been deleted */
		 if (!sp->matches || !tp->matches)
		 {
#ifdef DEBUG
		     printf("Null match pointer: %d=%p, %d=%p\n", 
			    sp->index, sp->matches, tp->index, tp->matches);
#endif /* DEBUG */
		     continue;
		 }
		 /* ranges must be the same length */
		 if (sp->nmatches != tp->nmatches)
		 {
#ifdef DEBUG
		     printf("Range length mismatch\n");
#endif /* DEBUG */
		     continue;
		 }
		 /* attempt the merge */
		 if (merge_ranges(sp->matches, tp->matches, sp->nmatches))
 		 {		 
#ifdef DEBUG
		     struct range_t	*rp;

		     printf("Merged %d into %d\n", tp->index, sp->index);
		     for (rp=sp->matches; rp < sp->matches+sp->nmatches; rp++)
			 printf("%s:%d:%d\n",  rp->file, rp->start, rp->end);
#endif /* DEBUG */
		     free(tp->matches);
		     nonuniques--;
		     tp->matches = NULL;
		     /* list is altered, do another merge pass */
		     retry++;
		 }
	     }
     } while
	 (retry);
     report_time("%d range groups after merging", nonuniques);
     return reduced;
}

int sortchunk(void *a, void *b)
/* sort by hash */
{
    int cmp = HASHCMP((struct sorthash_t *)a, (struct sorthash_t *)b);

    /*
     * Using the file name as a secondary key implies that, later on when
     * we use sort adjacency to build a duplicates list, the duplicates
     * will be ordered by filename -- thus, implicitly, by tree of origin.
     */
    if (cmp)
	return(cmp);
    else
	return(strcmp(((struct sorthash_t *)a)->file,
		      ((struct sorthash_t *)b)->file));
}

int sortmatch(void *a, void *b)
/* sort by file and first line */
{
    struct range_t *s = ((struct match_t *)a)->matches;
    struct range_t *t = ((struct match_t *)b)->matches;

    int cmp = strcmp(s->file, t->file);
    if (cmp)
	return(cmp);
    else
	return(s->start - t->start);

    return(0);
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */
    int	i, status, matchcount;
    struct match_t *hitlist, *sorted, *match;

    while ((status = getopt(argc, argv, "h")) != EOF)
    {
	switch (status)
	{
	case 'h':
	default:
	    fprintf(stderr,"usage: shredtree hashfile...\n");
	    fprintf(stderr,"  -h      = help (display this message).\n");
	    exit(0);
	}
    }

    report_time(NULL);
    merge_hashes(argc - optind, argv + optind);
    report_time("Hash merge done, %d entries", hashcount);

    /* the magic CPU-eating moment; sort the whole thing */ 
    qsort(obarray, hashcount, sizeof(struct sorthash_t), sortchunk);
    report_time("Sort done");

#ifdef ODEBUG
    for (np = obarray; np < obarray + hashcount; np++)
	printf("%d: %s:%d:%d (%02x%02x)\n", np-obarray, np->file, np->hash.start, np->hash.end, np->hash.hash[0], np->hash.hash[1]);
#endif /* ODEBUG */

    hitlist = reduce_matches();
    report_time("Reduction done");

    puts("#SHIF-B 1.0");
    printf("Hash-Method: %s\n", shiflist[0].hash_method);
    puts("Merge-Program: shredcompare 1.0");
    printf("Normalization: %s\n", shiflist[0].normalization);
    printf("Shred-Size: %d\n", shiflist[0].shred_size);
    puts("%%");

    /* we go through a little extra effort to emit a sorted list */
    matchcount = 0;
    for (match = hitlist; match->next; match = match->next)
	if (match->matches)
	    matchcount++;

    sorted = (struct match_t *)calloc(sizeof(struct match_t), matchcount);
    i = 0;
    for (match = hitlist; match->next; match = match->next)
	if (match->matches)
	    sorted[i++] = *match;
    qsort(sorted, matchcount, sizeof(struct match_t), sortmatch);

    for (match = sorted; match < sorted + matchcount; match++)
    {
	int	i;

	for (i=0; i < match->nmatches; i++)
	{
	    struct range_t	*rp = match->matches+i;

	    printf("%s:%d:%d\n",  rp->file, rp->start, rp->end);
	}
	printf("-\n");
    }

    exit(0);
}
