/* report.c -- search hash lists for common matches */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "shred.h"

#define min(x, y)	((x < y) ? (x) : (y)) 
#define max(x, y)	((x > y) ? (x) : (y)) 

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

static int merge_ranges(struct range_t *p, struct range_t *q, int nmatches)
/* merge p into q, if the ranges in the match are compatible */
{
    int	i, mc, overlap;
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
     * There are two possible overlap cases.  Either the start line of
     * each rane in p is within the corresponding range in q or
     * vice-versa.  If we know all pairs of shreds intersect, we
     * assume they intersect in the same way because the same 
     * hash pair (the same text) is involved each time.
     */
    overlap = 0;
    mc = 0;
    for (i = 0; i < nmatches; i++)
	if (p[i].start >= q[i].start && p[i].start <= q[i].end)
	    mc++;
    if (mc == nmatches)
	overlap = 1;
    mc = 0;
    for (i = 0; i < nmatches; i++)
	if (q[i].start >= p[i].start && q[i].start <= p[i].end)
	    mc++;
    if (mc == nmatches)
	overlap = 1;
    if (!overlap)
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

	     if (debug)
	     {
		 printf("*** %d has %d in its clique\n", np-obarray, nmatches);
		 for (i = 0; i < nmatches; i++)
		     printf("%d: %s:%d:%d\n", 
			    np-obarray+i, np[i].file, np[i].hash.start, np[i].hash.end);
	     }

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

static int sortmatch(const void *a, const void *b)
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
/* report our results */
{
    struct match_t *hitlist, *sorted, *match;
    int i, matchcount;

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

/* report.c ends here */
