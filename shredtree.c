/* shedtree.c -- generate MD5 hash list in SHIF-A format from a file tree */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>
#include <alloca.h>
#include <netinet/in.h>
#include <sys/types.h>
#include "md5.h"
#include "shred.h"

/* control bits, meant to be set at startup */
static int c_only = 0;
static int rws = 0;
static int debug = 0;
static int shredsize = 5;

/* globally visible data; chunk_buffer should be a malloc area set by the caller */
static int file_count, chunk_count;
static struct hash_t *chunk_buffer;

struct item
{
    char	*file;
    struct item *next;
}
dummy;
static struct item *head = &dummy;

static int eligible(const char *file)
/* is the specified file eligible to be compared? */ 
{
#define endswith(suff)	!strcmp(suff, file + strlen(file) - strlen(suff))
    if (c_only)
	return endswith(".c") || endswith(".h") || endswith(".txt");
    else
	return(1);
#undef endswith
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

static void emit_chunk(shred *display, int linecount)
/* emit chunk corresponding to current display; modifies globals chunk_buffer and chunk_count */
{
    struct md5_ctx	ctx;
    int  		i, firstline;
    unsigned char	*cp;

    if (debug)
    {
	for (i = 0; i < shredsize; i++)
	    if (display[i].line)
	    {
		fputc('\'', stderr);
		for (cp = display[i].line; *cp; cp++)
		    if (*cp == '\n')
			fputs("\\n", stderr);
		    else if (*cp == '\t')
			fputs("\\t", stderr);
		    else
			fputc(*cp, stderr);
		fputc('\'', stderr);
	        fputc('\n', stderr);
	    }    
    }

    chunk_buffer = (struct hash_t *)realloc(chunk_buffer, 
				      sizeof(struct hash_t) * (chunk_count+1));

    /* build completed chunk onto end of array */
    md5_init_ctx(&ctx);
    for (i = 0; i < shredsize; i++)
	if (display[i].line)
	    md5_process_bytes(display[i].line, strlen(display[i].line), &ctx);
    md5_finish_ctx(&ctx, (void *)&chunk_buffer[chunk_count].hash);
    for (i = shredsize - 1; i >= 0; i--)
	if (display[i].line)
	    firstline = i;
    firstline = display[firstline].number;
    chunk_buffer[chunk_count].start = TONET(firstline);
    chunk_buffer[chunk_count].end = TONET(linecount);
    chunk_count++;
}

static void shredfile(const char *file)
/* emit hash section for specified file; modifies globals chunk_buffer and chunk_count */
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
	    emit_chunk(display, linecount);

	/* shreds in progress are shifted down */
	free(display[0].line);
	for (i=1; i < shredsize; i++)
	    display[i-1] = display[i];
	display[shredsize-1].line = NULL;
    }
    if (linecount < shredsize)
	emit_chunk(display, linecount);

    free(display);
    fclose(fp);
}

int treewalker(const char *file, const struct stat *sb, int flag)
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

static void generate_shredfile(const char *tree, FILE *ofp)
/* generate shred file for given tree */
{
    char	**place, **list;
    u_int32_t	netfile_count;

    fputs("#SHIF-A 1.0\n", ofp);
    fputs("Generator-Program: shredtree 1.0\n", ofp);
    fputs("Hash-Method: MD5\n", ofp);
    fprintf(ofp, "Normalization: %s\n", rws ? "remove_whitespace" : "none");
    fprintf(ofp, "Shred-Size: %d\n", shredsize);
    fputs("%%\n", ofp);

    /* make file list */
    ftw(tree, treewalker, 16);

    /* now that we know the length, copy into an array */
    list = place = (char **)calloc(sizeof(char *), file_count);
    for (; head->next; head = head->next)
	*place++ = head->file;

    /* the objective -- sort */
    qsort(list, file_count, sizeof(char *), stringsort);

    /* generate the list */
    netfile_count = htonl(file_count);
    fwrite(&netfile_count, sizeof(u_int32_t), 1, ofp);
    for (place = list; place < list + file_count; place++)
    {
	linenum_t	net_chunks;

	/* shredfile adds data to chunk_buffer */
	chunk_buffer = (struct hash_t *)calloc(sizeof(struct hash_t), 1);
	chunk_count = 0;
	shredfile(*place);

	/* the actual output */
	fputs(*place, ofp);
	fputc('\n', ofp);
	net_chunks = TONET(chunk_count);
	fwrite((char *)&net_chunks, sizeof(linenum_t), 1, ofp);
	fwrite(chunk_buffer, sizeof(struct hash_t), chunk_count, ofp);
	free(chunk_buffer);
    }
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status;
    while ((status = getopt(argc, argv, "cd:hs:w")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    c_only = 1;
	    break;

	case 'd':
	    chdir(optarg);
	    break;

	case 's':
	    shredsize = atoi(optarg);
	    break;

	case 'w':
	    rws = 1;
	    break;

	case 'x':
	    debug = 1;
	    break;

	case 'h':
	default:
	    fprintf(stderr,"usage: shredtree [-c] [-d dir ] [-s shredsize] [-w] [-x] path\n");
	    fprintf(stderr,"  -c      = check .c, .h, and .txt files only.\n");
	    fprintf(stderr,"  -d dir  = change directory before digesting.\n");
	    fprintf(stderr,"  -h      = help (display this message).\n");
	    fprintf(stderr,"  -x      = debug, display chunks in output.\n");
	    fprintf(stderr,"  -s size = set shred size (default %d)\n",
		    shredsize);
	    fprintf(stderr,"  -w      = remove whitespace.\n");
	    exit(0);
	}
    }

    generate_shredfile(argv[optind], stdout);
    exit(0);
}
