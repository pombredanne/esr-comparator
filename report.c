/* shredcompare.c -- search hash lists for common matches */

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

static int local_duplicates;

struct shif_t
{
    char	*name;
    FILE	*fp;
    char	*normalization;
    int		shred_size;
    char	*hash_method;
    char	*generator_program;
};

static report_time(char *legend)
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

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */
    int	status, filecount;
    struct shif_t *shiflist, *sp;
    struct stat sb;
    long total;

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

    /* set up metadata blocks for the hash files */
    filecount = argc - optind;
    shiflist = (struct shif_t *)calloc(sizeof(struct shif_t), filecount);
    for (sp = shiflist; sp < shiflist + filecount; sp++)
    {
	char	buf[BUFSIZ];

	sp->name = strdup(argv[optind++]);
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
    for (sp = shiflist; sp < shiflist + filecount-1; sp++)
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
    for (sp = shiflist; sp < shiflist + filecount-1; sp++)
    {
	stat(sp->name, &sb);
	total += sb.st_size;
    }
    fprintf(stderr, "%ld bytes to be read.\n", total);

    /* read in all hashes */
}
