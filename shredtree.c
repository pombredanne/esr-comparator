/* shedtree.c -- generate MD5 hash list in SHIF-A format from a file tree */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>
#include <alloca.h>
#include "md5.h"

static c_only = 0;
static rws = 0;
static shredsize = 5;

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

void shredfile(const char *file)
/* emit hash section for specified file */
{
    FILE *fp;
    char buf[BUFSIZ];
    int i, linecount, accepted;
    unsigned char hash[32];
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
	{
	    struct md5_ctx	ctx;

#ifdef SHOWCHUNK
	    printf("Chunk starts at: %d\n", display[0].number);
	    for (i = 0; i < shredsize; i++)
		puts(display[i].line);
#endif /* SHOWCHUNK */

	    /* flush completed chunk */
	    md5_init_ctx(&ctx);
	    for (i = 0; i < shredsize; i++)
		md5_process_bytes(buf, strlen(buf), &ctx);
	    md5_finish_ctx(&ctx, (void *)hash);
	    fprintf(stdout, 
		    "%d\t%d\t%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", 
		    display[0].number, linecount, 
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

	/* shreds in progress are shifted down */
	free(display[0].line);
	for (i=1; i < shredsize; i++)
	    display[i-1] = display[i];
    }

    puts("");

    free(display);
    fclose(fp);
}

int treewalker(const char *file, const struct stat *sb, int flag)
/* walk the tree, emitting hash sections for eligible files */
{
    if (flag == FTW_F && eligible(file))
        shredfile(file);
    return(0);
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status;
    char *dir = ".";
    while ((status = getopt(argc, argv, "cs:w")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    c_only = 1;
	    break;

	case 's':
	    shredsize = atoi(optarg);
	    break;

	case 'w':
	    rws = 1;
	    break;

	default:
	    fputs("usage: [-s shredsize] [-w] path", stderr);
	}
    }

    puts("#SHIF-A 1.0");
    puts("Generator: shredtree 1.0");
    puts("Hash: MD5");
    printf("Shred-Size: %d\n", shredsize);
    printf("Normalization: %s\n", rws ? "remove_whitespace" : "none");
    puts("%%");

    ftw(argv[optind], treewalker, 16);
}
