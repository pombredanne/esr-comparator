/* shedtree.c -- generate MD5 hash list in SHIF-A format from a file tree */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>
#include <alloca.h>
#include "md5.h"

static int c_only = 0;
static int rws = 0;
static int debug = 0;
static int shredsize = 5;
static int filecount;

struct item
{
    char	*file;
    struct item *next;
}
dummy;
struct item *head = &dummy;

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
/* emit chunk corresponding to current display */
{
    struct md5_ctx	ctx;
    int  		i, firstline;
    unsigned char hash[32], *cp;

    if (debug)
    {
	for (i = 0; i < shredsize; i++)
	    if (display[i].line)
	    {
		putchar('\'');
		for (cp = display[i].line; *cp; cp++)
		    if (*cp == '\n')
			fputs("\\n", stdout);
		    else if (*cp == '\t')
			fputs("\\t", stdout);
		    else
			putchar(*cp);
		putchar('\'');
	        putchar('\n');
	    }    
    }

    /* flush completed chunk */
    md5_init_ctx(&ctx);
    for (i = 0; i < shredsize; i++)
	if (display[i].line)
	    md5_process_bytes(display[i].line, strlen(display[i].line), &ctx);
    md5_finish_ctx(&ctx, (void *)hash);
    for (i = shredsize - 1; i >= 0; i--)
	if (display[i].line)
	    firstline = i;
    fprintf(stdout, 
	    "%d\t%d\t%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", 
	    display[firstline].number, linecount, 
	    hash[0],
	    hash[1],
	    hash[2],
	    hash[3],
	    hash[4],
	    hash[5],
	    hash[6],
	    hash[7],
	    hash[8],
	    hash[9],
	    hash[10],
	    hash[11],
	    hash[12],
	    hash[13],
	    hash[14],
	    hash[15]);
}

void shredfile(const char *file)
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

    puts(file);

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

    puts("");
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
	filecount++;
    }
    return(0);
}

int stringsort(void *a, void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status;
    char **place, **list, *dir = ".";
    while ((status = getopt(argc, argv, "cdhs:w")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    c_only = 1;
	    break;

	case 'd':
	    debug = 1;
	    break;

	case 's':
	    shredsize = atoi(optarg);
	    break;

	case 'w':
	    rws = 1;
	    break;

	case 'h':
	default:
	    fprintf(stderr,"usage: shredtree [-c] [-s shredsize] [-w] path\n");
	    fprintf(stderr,"  -c      = check .c, .h, and .txt files only.\n");
	    fprintf(stderr,"  -d      = debug, display chunks in output.\n");
	    fprintf(stderr,"  -h      = help (display this message).\n");
	    fprintf(stderr,"  -s size = set shred size (default %d)\n",
		    shredsize);
	    fprintf(stderr,"  -w      = remove whitespace.\n");
	    exit(0);
	}
    }

    puts("#SHIF-A 1.0");
    puts("Generator: shredtree 1.0");
    puts("Hash: MD5");
    printf("Shred-Size: %d\n", shredsize);
    printf("Normalization: %s\n", rws ? "remove_whitespace" : "none");
    puts("%%");

    /* make file list */
    ftw(argv[optind], treewalker, 16);

    /* now that we know the length, copy into an array */
    list = place = (char **)calloc(sizeof(char *), filecount);
    for (; head->next; head = head->next)
	*place++ = head->file;

    /* the objective -- sort */
    qsort(list, filecount, sizeof(char *), stringsort);

    for (place = list; place < list + filecount; place++)
	shredfile(*place);
}
