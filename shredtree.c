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
int remove_braces = 0;
int remove_whitespace = 0;
int remove_comments = 0;
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
    else if (endswith(".o") || endswith("~") || endswith(".bdf"))
	return(0);
    else if (strstr(file, "CVS") || strstr(file,"RCS") || strstr(file,"SCCS"))
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

static int normalize(char *buf)
/* normalize a buffer in place, return 0 if it should be skipped */
{
    if (remove_comments)
    {
	if (remove_braces)	/* remove C comments */
	{
	    char	*ss = strstr(buf, "//");

	    if (ss)
		*ss = '\0';
	    else
	    {
		char *start = strstr(buf, "/*"), *end = strstr(buf, "*/");

		if (start && end && start < end)
		    strcpy(start, end+2);
		else if (start && !end)
		    *start = '\0';
		else if (end && !start)
		    *end = '\0';
	    }
	}
	else
	{
	    char	*ss = strchr(buf, '#');

	    if (ss)
		*ss = '\0';
	}
    }

    if (remove_whitespace)	/* strip whitespace, ignore blank lines */
    {
	char *tp, *sp;

	for (tp = sp = buf; *sp; sp++)
	    if (*sp != ' ' && *sp != '\t' && *sp != '\n')
		*tp++ = *sp;
	*tp = '\0';
    }
    if (remove_braces)		/* strip C statement brackets */
    {
	char *tp, *sp;

	for (tp = sp = buf; *sp; sp++)
	    if (*sp != '{' && *sp != '}')
		*tp++ = *sp;
	*tp = '\0';
    }

    return(buf[0]);
}

typedef struct
{
    char	*line;
    linenum_t  	start;
    flag_t	flags;
}
shred;

static struct hash_t emit_chunk(shred *display, int linecount) 
/* emit chunk corresponding to current display */
{
    int  		i, firstline;
    struct hash_t	out;

    /* build completed chunk onto end of array */
    out.flags = 0;
    hash_init();
    firstline = shredsize;
    for (i = shredsize - 1; i >= 0; i--)
	if (display[i].line)
	    firstline = i;
    firstline = display[firstline].start;
    if (debug)
	fprintf(stderr, "Chunk at line %d:\n", firstline);
    for (i = 0; i < shredsize; i++)
	if (display[i].line)
	{
	    if (debug)
		fprintf(stderr, "%d (%02x): '%s'\n", i, display[i].flags, display[i].line);
	    hash_update(display[i].line, strlen(display[i].line));
	    out.flags |= display[i].flags;
	}
    hash_complete(&out.hash);
    out.start = firstline;
    out.end = linecount;

    return(out);
}

int shredfile(struct filehdr_t *file, 
		      void (*hook)(struct hash_t, struct filehdr_t *))
/* emit hash section for specified file */
{
    FILE *fp;
    char buf[BUFSIZ];
    int i, linecount, accepted;
    shred *display;

    if ((fp = fopen(file->name, "r")) == NULL)
    {
	fprintf(stderr, "shredtree: couldn't open %s, error %d\n",
		file->name, errno);
	exit(1);
    }

    /* deduce what filtering type we should use */
#define endswith(suff) !strcmp(suff,file->name+strlen(file->name)-strlen(suff))
    filter_set(0);
    if (endswith(".c") || endswith(".h"))
	filter_set(C_CODE);
    else if (endswith(".sh"))
	filter_set(SHELL_CODE);
#undef endswith

    display = (shred *)calloc(sizeof(shred), shredsize);

    accepted = linecount = 0;
    while(fgets(buf, sizeof(buf), fp) != NULL)
    {
	int	braceline = 0;

	linecount++;
	if (linecount >= MAX_LINENUM)
	{
	    fprintf(stderr, "comparator: %s too large, only first %d lines will be compared.\n", file->name, MAX_LINENUM-1);
	    break;
	}

	if (remove_braces)
	{
	    char *cp;

	    for (cp = buf; *cp && isspace(*cp); cp++)
		continue;
	    braceline = (*cp == '}');
	}
	if (!normalize(buf))
	{
	    /*
	     * What this is for is to include trailing C } lines in chunk
	     * listings even though we're ignoring them for comparison 
	     * purposes (in order not to be fooled by variance in indent
	     * styles).  This is a kluge, but it means we will capture
	     * entire C functions that differ only by brace placement.
	     */
	    if (remove_braces && braceline)
		extend_current_chunk(file->length);
	    continue;
	}
	accepted++;

	/* maybe we can get the file type from the first line? */
	if (linecount == 1 && buf[0] == '#')
	    if (strstr(buf, "sh"))
		filter_set(SHELL_CODE);

	/* create new shred */
	display[shredsize-1].line = strdup(buf);
	display[shredsize-1].start = linecount;
	display[shredsize-1].flags = filter_pass(buf) ? INSIGNIFICANT : 0;

	/* flush completed chunk */
	if (accepted >= shredsize)
	    hook(emit_chunk(display, linecount), file);

	/* shreds in progress are shifted down */
	free(display[0].line);
	for (i=1; i < shredsize; i++)
	    display[i-1] = display[i];
	display[shredsize-1].line = NULL;
	display[shredsize-1].flags = 0;
    }
    if (accepted && accepted < shredsize)
	hook(emit_chunk(display, linecount), file);

    free(display);
    fclose(fp);
    return(linecount);
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
