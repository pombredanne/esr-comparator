/* main.c -- main sequence of comparator */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include "shred.h"

static int chunk_count, sort_count;
static struct hash_t *chunk_buffer;
static struct sorthash_t *sort_buffer;

/* 
 * We differentiate these two functions in order to avaid the memory
 * overhead of the extra (char *) when assembling shred lists to be written
 * to a file.  This is only worth bothering with because the data sets
 * can get quite large.
 */

static void filehook(struct hash_t hash, const char *file)
/* hook to store only hashes */
{
    chunk_buffer = (struct hash_t *)realloc(chunk_buffer, 
				      sizeof(struct hash_t) * (chunk_count+1));
    
    hash.start = TONET(hash.start);
    hash.end = TONET(hash.end);
    chunk_buffer[chunk_count++] = hash;
}


static void corehook(struct hash_t hash, const char *file)
/* hook to store hash and file */
{
    sort_buffer = (struct sorthash_t *)realloc(sort_buffer, 
				      sizeof(struct sorthash_t) * (sort_count+1));
    
    sort_buffer[sort_count].hash = hash;
    sort_buffer[sort_count].file = (char *)file;
    sort_count++;
}

static void generate_shredfile(const char *tree, FILE *ofp)
/* generate shred file for given tree */
{
    char	**place, **list;
    int		file_count;
    u_int32_t	netfile_count;

    list = sorted_file_list(tree, &file_count);

    fputs("#SCIF-A 1.0\n", ofp);
    fputs("Generator-Program: comparator 1.0\n", ofp);
    fputs("Hash-Method: MD5\n", ofp);
    fprintf(ofp, "Normalization: %s\n", rws ? "remove_whitespace" : "none");
    fprintf(ofp, "Shred-Size: %d\n", shredsize);
    fputs("%%\n", ofp);

    netfile_count = htonl(file_count);
    fwrite(&netfile_count, sizeof(u_int32_t), 1, ofp);
    for (place = list; place < list + file_count; place++)
    {
	linenum_t	net_chunks;

	/* shredfile adds data to chunk_buffer */
	chunk_buffer = (struct hash_t *)calloc(sizeof(struct hash_t), 1);
	chunk_count = 0;
	shredfile(*place, filehook);

	/* the actual output */
	fputs(*place, ofp);
	fputc('\n', ofp);
	net_chunks = TONET(chunk_count);
	fwrite((char *)&net_chunks, sizeof(linenum_t), 1, ofp);
	fwrite(chunk_buffer, sizeof(struct hash_t), chunk_count, ofp);
	free(chunk_buffer);
    }
}

static void generate_shredlist(int argc, char *argv[])
/* generate an in-core list of sorthash structures */
{
    int	i, file_count;

    sort_buffer = (struct sorthash_t*)calloc(sizeof(struct sorthash_t),1);
    sort_count = 0;

    for (i = 0; i < argc; i++)
    {
	char	**place, **list;
	int	oldcount;

	oldcount = sort_count;
	fprintf(stderr, "%% Reading %s...", argv[i]);
	list = sorted_file_list(argv[i], &file_count);
	for (place = list; place < list + file_count; place++)
	    shredfile(*place, corehook);
	fprintf(stderr, "%d entries\n", sort_count - oldcount);
	free(list);
    }
}

void report_time(char *legend, ...)
{
    static time_t mark_time;
    time_t endtime = time(NULL);
    va_list	ap;

    if (mark_time)
    {
	int elapsed = endtime - mark_time;
	int hours = elapsed/3600; elapsed %= 3600;
	int minutes = elapsed/60; elapsed %= 60;
	int seconds = elapsed;
	char	buf[BUFSIZ];

	va_start(ap, legend);
	vsprintf(buf, legend, ap);
	fprintf(stderr, "%% %s: %dh %dm %ds\n", buf, hours, minutes, seconds);
    }
    mark_time = endtime;
}

main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status, file_only, compile_only;

    compile_only = file_only = 0;
    while ((status = getopt(argc, argv, "cd:hs:w")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    compile_only = 1;
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
	    fprintf(stderr,"usage: shredtree [-c] [-d dir ] [-h] [-s shredsize] [-w] [-x] path\n");
	    fprintf(stderr,"  -c      = generate SCIF files\n");
	    fprintf(stderr,"  -d dir  = change directory before digesting.\n");
	    fprintf(stderr,"  -h      = help (display this message).\n");
	    fprintf(stderr,"  -s size = set shred size (default %d)\n",
		    shredsize);
	    fprintf(stderr,"  -w      = remove whitespace.\n");
	    fprintf(stderr,"  -x      = debug, display chunks in output.\n");
	    exit(0);
	}
    }

    if (compile_only)
	generate_shredfile(argv[optind], stdout);
    else
    {
	struct scif_t	localblock;

	localblock.hash_method = "MD5";
	localblock.normalization = rws ? "remove_whitespace" : "none";
	localblock.shred_size = shredsize;
	localblock.generator_program = "comparator 1.0";

	report_time(NULL);
	generate_shredlist(argc-optind, argv+optind);
	report_time("Hash merge done, %d entries", sort_count);
	sort_hashes(sort_buffer, sort_count);
	report_time("Sort done");
	emit_report(&localblock, sort_buffer, sort_count);	
    }

#if 0
    else
    {
	int	hashcount;
	struct sorthash_t *obarray;
	struct scif_t	*sciflist;

	report_time(NULL);
	sciflist = init_merge(argc-optind, argv+optind);
	obarray = merge_hashes(sciflist, argc-optind, &hashcount);
	report_time("Hash merge done, %d entries", hashcount);
	sort_hashes(obarray, hashcount);
	report_time("Sort done");
	emit_report(sciflist, obarray, hashcount);
    }
#endif /* FOO */

    exit(0);
}

/* main.c ends here */
