/* shredcompare.c -- search hash lists for common matches */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
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

static struct scf_t *scflist;

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

void merge_scf(const char *name, FILE *fp)
/* merge hashes from specified files into an in-code list */
{
    u_int32_t	sectcount;
    int hashcount = 0;

    fprintf(stderr, "%% Reading %s...   ", name);
    fread(&sectcount, sizeof(u_int32_t), 1, fp);
    sectcount = ntohl(sectcount);
    while (sectcount--)
    {
	char	buf[BUFSIZ];
	linenum_t	chunks;
	struct item *new;

	fgets(buf, sizeof(buf), fp);
	*strchr(buf, '\n') = '\0';
	new = (struct item *)malloc(sizeof(struct item));
	new->file = strdup(name);
	new->next = head;
	head = new;
	fread(&chunks, sizeof(linenum_t), 1, fp);
	chunks = FROMNET(chunks);

	while (chunks--)
	{
	    struct hash_t	this;

	    fread(&this.hash, sizeof(struct hash_t), 1, fp);
	    this.start = FROMNET(this.start);
	    this.end = FROMNET(this.end);
	    corehook(this, head->file);
	    hashcount++;
#ifdef FIXME
	    if (hashcount % 10000 == 0)
		fprintf(stderr,"\b\b\b%02.0f%%",(ftell(fp) / (total * 0.01)));
#endif
	}
    }
    fprintf(stderr, "\b\b\b100%%...done, %d entries\n", hashcount);
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

    return (sn == tn) && !strncmp(s, t, sn); 
}

struct match_t *reduce_matches(struct sorthash_t *obarray, int hashcount)
/* assemble list of duplicated hashes */
{
     static struct match_t dummy; 
     struct match_t *reduced = &dummy, *sp, *tp;
     unsigned int retry, nonuniques;
     struct sorthash_t *np;

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
		 if (!sametree(np[i].file, np[(i+1) % nmatches].file))
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

static int sortmatch(void *a, void *b)
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

void emit_report(struct sorthash_t *obarray, int hashcount)
{
    struct match_t *hitlist, *sorted, *match;
    int i, matchcount;

#ifdef ODEBUG
    struct sorthash_t	*np;

    for (np = obarray; np < obarray + hashcount; np++)
	printf("%d: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %s:%d:%d\n", 
	       np-obarray, 
	       np->hash.hash[0], 
	       np->hash.hash[1], 
	       np->hash.hash[2], 
	       np->hash.hash[3], 
	       np->hash.hash[4], 
	       np->hash.hash[5], 
	       np->hash.hash[6], 
	       np->hash.hash[7], 
	       np->hash.hash[8], 
	       np->hash.hash[9], 
	       np->hash.hash[10], 
	       np->hash.hash[11], 
	       np->hash.hash[12], 
	       np->hash.hash[13], 
	       np->hash.hash[14], 
	       np->hash.hash[15],
	       np->file, np->hash.start, np->hash.end);
#endif /* ODEBUG */

    hitlist = reduce_matches(obarray, hashcount);
    report_time("Reduction done");

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
	printf("%%%%\n");
    }
}

