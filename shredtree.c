/* shedtree.c -- generate MD5 hash list from a file tree */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>
#include <alloca.h>
#include <netinet/in.h>
#include "md5.h"
#include "shred.h"

/* control bits, meant to be set at startup */
int c_only = 0;
int rws = 0;
int shredsize = 3;

struct item
{
    char	*file;
    struct item *next;
}
dummy;
static struct item *head = &dummy;

static int file_count;

/*************************************************************************
 *
 * File shredding 
 *
 *************************************************************************/

#define MIN_PRINTABLE	0.9

static int eligible(const char *file)
/* is the specified file eligible to be compared? */ 
{
    /* fast check for the most common suffixes */
#define endswith(suff)	!strcmp(suff, file + strlen(file) - strlen(suff))
    if (endswith(".c") || endswith(".h") || endswith(".html"))
	return(1);
    else if (endswith(".o") || endswith("~"))
	return(0);
#undef endswith
    else
    {
	FILE	*fp = fopen(file, "r");
	char	buf[BUFSIZ], *cp;
	int	printable;

	if (!fp || !fgets(buf, sizeof(buf)-1, fp))
	    return(0);
	fclose(fp);

	/* count printables */
	for (cp = buf; *cp; cp++)
	    if (isascii(*cp))
		printable++;

	/* are we over the critical percentage? */
	return (printable/strlen(buf) >= MIN_PRINTABLE);
    }
}

static int normalize(char *buf)
/* normalize a buffer in place, return 0 if it should be skipped */
{
    if (rws)		/* strip whitespace, ignore blank lines */
    {
	char *tp, *sp;

	for (tp = sp = buf; *sp; sp++)
	    if (*sp != ' ' && *sp != '\t' && *sp != '\n')
		*tp++ = *sp;
	*tp = '\0';
    }

    return(buf[0]);
}

typedef struct
{
    char	*line;
    int  	number;
}
shred;

static struct hash_t emit_chunk(shred *display, int linecount) 
/* emit chunk corresponding to current display */
{
    struct md5_ctx	ctx;
    int  		i, firstline;
    unsigned char	*cp;
    struct hash_t	out;

    /* build completed chunk onto end of array */
    md5_init_ctx(&ctx);
    for (i = 0; i < shredsize; i++)
	if (display[i].line)
	    md5_process_bytes(display[i].line, strlen(display[i].line), &ctx);
    md5_finish_ctx(&ctx, (void *)&out.hash);
    for (i = shredsize - 1; i >= 0; i--)
	if (display[i].line)
	    firstline = i;
    firstline = display[firstline].number;
    out.start = firstline;
    out.end = linecount;

    return(out);
}

void shredfile(const char *file, 
		      void (*hook)(struct hash_t, const char *))
/* emit hash section for specified file */
{
    FILE *fp;
    char buf[BUFSIZ];
    int i, linecount, accepted;
    shred *display;

    if ((fp = fopen(file, "r")) == NULL)
    {
	fprintf(stderr, "shredtree: couldn't open %s, error %d\n",
		file, errno);
	exit(1);
    }

    display = (shred *)calloc(sizeof(shred), shredsize);

    accepted = linecount = 0;
    while(fgets(buf, sizeof(buf), fp) != NULL)
    {
	linecount++;
	if (!normalize(buf))
	    continue;
	accepted++;

	/* create new shred */
	display[shredsize-1].line = strdup(buf);
	display[shredsize-1].number = linecount;

	/* flush completed chunk */
	if (accepted >= shredsize)
	    hook(emit_chunk(display, linecount), file);

	/* shreds in progress are shifted down */
	free(display[0].line);
	for (i=1; i < shredsize; i++)
	    display[i-1] = display[i];
	display[shredsize-1].line = NULL;
    }
    if (linecount < shredsize)
	hook(emit_chunk(display, linecount), file);

    free(display);
    fclose(fp);
}

/*************************************************************************
 *
 * File list generation
 *
 *************************************************************************/

static int treewalker(const char *file, const struct stat *sb, int flag)
/* walk the tree, emitting hash sections for eligible files */
{
    if (flag == FTW_F && sb->st_size > 0 && eligible(file))
    {
	struct item *new;

	new = (struct item *)malloc(sizeof(new));
        new->file = strdup(file);
	new->next = head;
	head = new;
	file_count++;
    }
    return(0);
}

static int stringsort(void *a, void *b)
/* sort comparison of strings by content */ 
{
    return strcmp(*(char **)a, *(char **)b);
}

char **sorted_file_list(const char *tree, int *fc)
/* generate a sorted list of files under the given tree */
{
    char	**place, **list;

    /* make file list */
    file_count = 0;
    ftw(tree, treewalker, 16);

    /* now that we know the length, copy into an array */
    list = place = (char **)calloc(sizeof(char *), file_count);
    for (; head->next; head = head->next)
	*place++ = head->file;

    /* the objective -- sort */
    qsort(list, file_count, sizeof(char *), stringsort);

    /* caller is responsible for freeing this */
    *fc = file_count;
    return(list);
}

/*************************************************************************
 *
 * Hash sorting
 *
 *************************************************************************/

static int sortchunk(void *a, void *b)
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

void sort_hashes(struct sorthash_t *hashlist, int hashcount)
/* the magic CPU-eating moment; sort the whole thing */ 
{
    qsort(hashlist, hashcount, sizeof(struct sorthash_t), sortchunk);
}


/* shredtree.c ends here */
