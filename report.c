/* shredcompare.c -- search hash lists for common matches */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "shred.h"

static int local_duplicates;

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
};

struct sorthash_t
{
    struct hash_t	hash;
    char		*file;
};
#define HASHCMP(s, t)	memcmp((s)->hash.hash, (t)->hash.hash, HASHSIZE)

static struct sorthash_t *obarray, *np;
static int hashcount;

struct match_t
{
    char	*file;
    linenum_t	start, end;
    struct match_t *next;
}
dummy_match;
static struct match_t *reduced = &dummy_match;

static void report_time(char *legend)
{
    static time_t mark_time;
    time_t endtime = time(NULL);

    if (mark_time)
    {
	int elapsed = endtime - mark_time;
	int hours = elapsed/3600; elapsed %= 3600;
	int minutes = elapsed/60; elapsed %= 60;
	int seconds = elapsed;

	fprintf(stderr, "%% %s: %dh, %dm, %ds\n", 
		legend, hours, minutes, seconds);
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

static long merge_hashes(int argc, char *argv[])
/* merge hashes from specified files into an in-code list */
{
    struct shif_t *shiflist, *sp;
    struct stat sb;
    long total, hashcount = 0;

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
    fprintf(stderr, "%ld bytes to be read.\n", total);
    init_intern(total/sizeof(struct hash_t));	/* real work done here */

    /* read in all hashes */
    for (sp = shiflist; sp < shiflist + argc; sp++)
    {
	u_int32_t	sectcount;

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
	    }
	}
    }

    return hashcount;
}

struct match_t *reduce_matches(int localdups)
{
    static struct match_t dummy; 
    struct match_t *reduced;

    for (np = obarray; np < obarray + hashcount; np++)
	if (np < obarray + hashcount - 1 && !HASHCMP(np, np+1))
	    for (;;)
	    {
		struct match_t *new;

		printf("Foo %d\n", np - obarray);
		new = (struct match_t *)malloc(sizeof(struct match_t));
		new->file  = np->file;
		new->start = np->hash.start;
		new->end   = np->hash.end;
		new->next  = reduced;
		reduced = new;
		if (HASHCMP(np, np+1))
		    break;
		np++;
	    }
}

int sortchunk(void *a, void *b)
/* sort by hash */
{
    return HASHCMP((struct sorthash_t *)a, (struct sorthash_t *)b);
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */
    int	status;
    struct match_t *reduced;

    while ((status = getopt(argc, argv, "dh")) != EOF)
    {
	switch (status)
	{
	case 'd':
	    local_duplicates = 1;
	    break;

	case 'h':
	default:
	    fprintf(stderr,"usage: shredtree [-d] hashfile...\n");
	    fprintf(stderr,"  -d      = remove local duplicates.\n");
	    fprintf(stderr,"  -h      = help (display this message).\n");
	    exit(0);
	}
    }

    report_time(NULL);
    hashcount = merge_hashes(argc - optind, argv + optind);
    report_time("Hash merge done.");

    /* the magic CPU-eating moment; sort the whole thing */ 
    qsort(obarray, hashcount, sizeof(struct sorthash_t), sortchunk);
    report_time("Sort done.");

    for (np = obarray; np < obarray + hashcount; np++)
	printf("%d: %s:%d:%d (%02x%02x)\n", np-obarray, np->file, np->hash.start, np->hash.end, np->hash.hash[0], np->hash.hash[1]);

    reduced = reduce_matches(local_duplicates);
    report_time("Reduction done.\n");

    for (; reduced; reduced = reduced->next)
    {
	printf("%s:%d:%d\n", reduced->file, reduced->start, reduced->end);

    }
}