/* shedtree.c -- generate MD5 hash list from a file tree */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#if defined(__FreeBSD__)
# include <sys/types.h>
# include <sys/stat.h>
# include <fts.h>
# define FTW_F 1
#else
# include <ftw.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include "shred.h"

/* control bits, meant to be set at startup */
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
    if (strstr(file, "CVS") || strstr(file,"RCS") || strstr(file,"SCCS") || strstr(file, "SVN") || strstr(file, ".svn"))
	return(0);
    /* fast check for the most common suffixes */
#define endswith(suff)	!strcmp(suff, file + strlen(file) - strlen(suff))
    else if (endswith(".c") || endswith(".h") || endswith(".html"))
	return(1);
    else if (endswith(".o") || endswith("~") || endswith(".bdf"))
	return(0);
#undef endswith
    else
    {
	FILE	*fp = fopen(file, "r");
	char	buf[BUFSIZ], *cp;
	int	printable = 0;

	if (!fp || !fgets(buf, sizeof(buf)-1, fp))
	    return(0);
	fclose(fp);

	/* count printables */
	for (cp = buf; *cp; cp++)
	    if (isgraph(*cp) || isspace(*cp))
		printable++;

	/* are we over the critical percentage? */
	return (printable >= MIN_PRINTABLE * strlen(buf));
    }
}

typedef struct
{
    char	*feature;
    linenum_t  	start;
    flag_t	flags;
}
shred;

static struct hash_t emit_chunk(shred *display, int linecount) 
/* emit chunk corresponding to current display */
{
    int  		i, firstindex;
    struct hash_t	out;

    /* build completed chunk onto end of array */
    out.flags = 0;
    hash_init();
    firstindex = shredsize;
    for (i = shredsize - 1; i >= 0; i--)
	if (display[i].feature)
	    firstindex = i;
    firstindex = display[firstindex].start;
    if (debug)
	fprintf(stderr, "Chunk at line %d:\n", firstindex);
    for (i = 0; i < shredsize; i++)
	if (display[i].feature)
	{
	    if (debug)
		fprintf(stderr, "%d (%02x): '%s'\n", i, display[i].flags, display[i].feature);
	    hash_update((unsigned char*) display[i].feature, strlen(display[i].feature));
	    out.flags |= display[i].flags;
	}
    hash_complete(&out.hash);
    out.start = firstindex;
    out.end = linecount;

    return(out);
}

int shredfile(struct filehdr_t *file, 
		      void (*hook)(struct hash_t, struct filehdr_t *))
/* emit hash section for specified file */
{
    FILE *fp;
    int i, accepted;
    linenum_t	linenumber;
    shred *display;
    feature_t *feature;

    if ((fp = fopen(file->name, "r")) == NULL)
    {
	fprintf(stderr, "shredtree: couldn't open %s, error %d\n",
		file->name, errno);
	exit(1);
    }

    /* deduce what filtering type we should use */
#define endswith(suff) !strcmp(suff,file->name+strlen(file->name)-strlen(suff))
    linebyline.mode(0);
    if (endswith(".c") || endswith(".cc") || endswith(".h"))
	linebyline.mode(C_CODE);
    else if (endswith(".sh"))
	linebyline.mode(SHELL_CODE);
#undef endswith

    display = (shred *)calloc(sizeof(shred), shredsize);

    linenumber = accepted = 0;
    while ((feature = linebyline.get(file, fp, &linenumber)))
    {
	accepted++;

	/* create new shred */
	display[shredsize-1].feature = feature->text;
	display[shredsize-1].start = linenumber;
	display[shredsize-1].flags = feature->flags;

	/* flush completed chunk */
	if (accepted >= shredsize)
	    hook(emit_chunk(display, linenumber), file);

	/* shreds in progress are shifted down */
	linebyline.free(display[0].feature);
	for (i=1; i < shredsize; i++)
	    display[i-1] = display[i];
	display[shredsize-1].feature = NULL;
	display[shredsize-1].flags = 0;
    }
    if (accepted && accepted < shredsize)
	hook(emit_chunk(display, linenumber), file);
    else if (accepted)
	/*
	 * What this is for is to include trailing C } lines in chunk
	 * listings even though we're ignoring them for comparison 
	 * purposes (in order not to be fooled by variance in indent
	 * styles).  This is a kluge, but it means we will capture
	 * entire C functions that differ only by brace placement.
	 */
	extend_current_chunk(linenumber);

    free(display);
    fclose(fp);
    return(linenumber);
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

static int stringsort(const void *a, const void *b)
/* sort comparison of strings by content */ 
{
    return strcmp(*(char **)a, *(char **)b);
}

char **sorted_file_list(const char *tree, int *fc)
/* generate a sorted list of files under the given tree */
{
    char	**place, **list;
#if defined(__FreeBSD__)
    char *dirlist[2];
    FTS *ftsptr;
    FTSENT *entry;
#endif

    /* make file list */
    file_count = 0;
#if defined(__FreeBSD__)
    dirlist[0]= tree; dirlist[1]= NULL;
    ftsptr= fts_open(dirlist, FTS_LOGICAL, NULL);

    /* process each entry that is a file */
    while(1) {
      entry= fts_read(ftsptr);
      if (entry==NULL) break;
      if (entry->fts_info==FTS_F) {
	treewalker(entry->fts_accpath, entry->fts_statp, 1);
      }
    }
    fts_close(ftsptr);
#else
    ftw(tree, treewalker, 16);
#endif

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

static int sortchunk(const void *a, const void *b)
/* sort by hash */
{
    int cmp = SORTHASHCMP((struct sorthash_t *)a, (struct sorthash_t *)b);

    /*
     * Using the file name as a secondary key implies that, later on when
     * we use sort adjacency to build a duplicates list, the duplicates
     * will be ordered by filename -- thus, implicitly, by tree of origin.
     */
    if (cmp)
	return(cmp);
    else
	return(strcmp(((struct sorthash_t *)a)->file->name,
		      ((struct sorthash_t *)b)->file->name));
}

void sort_hashes(struct sorthash_t *hashlist, int hashcount)
/* the magic CPU-eating moment; sort the whole thing */ 
{
    qsort(hashlist, hashcount, sizeof(struct sorthash_t), sortchunk);
}


/* shredtree.c ends here */
