/* main.c -- main sequence of comparator */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "shred.h"

int debug = 0;

struct scf_t
{
    char	*name;
    FILE	*fp;
    u_int32_t	totallines;
    char	*normalization;
    int		shred_size;
    char	*hash_method;
    char	*generator_program;
    struct scf_t *next;
};
static struct scf_t dummy_scf, *scflist = &dummy_scf;

static int chunk_count, sort_count;
static struct hash_t *chunk_buffer;
static struct sorthash_t *sort_buffer;

static int is_scf_file(const char *file)
/* is the specified file an SCF hash list? */
{
    char	buf[BUFSIZ];
    FILE	*fp = fopen(file, "r");

    if (!fp)
	return(0);
    else if (!fgets(buf, sizeof(buf), fp))
    {
	fclose(fp);
	return(0);
    }
    fclose(fp);
    return(!strncmp(buf, "#SCF-A ", 7));
}

/* 
 * We differentiate these two functions in order to avoid the memory
 * overhead of the extra (char *) when assembling shred lists to be written
 * to a file.  This is only worth bothering with because the data sets
 * can get quite large.
 */

static void filehook(struct hash_t hash, const char *file)
/* hook to store only hashes */
{
    chunk_buffer = (struct hash_t *)realloc(chunk_buffer, 
				      sizeof(struct hash_t) * (chunk_count+1));
    
    chunk_buffer[chunk_count++] = hash;
}


void corehook(struct hash_t hash, const char *file)
/* hook to store hash and file */
{
    sort_buffer = (struct sorthash_t *)realloc(sort_buffer, 
				      sizeof(struct sorthash_t) * (sort_count+1));
    
    sort_buffer[sort_count].hash = hash;
    sort_buffer[sort_count].file = (char *)file;
    sort_count++;
}

static void write_scf(const char *tree, FILE *ofp)
/* generate shred file for given tree */
{
    char	**place, **list;
    int		file_count, progress, totalchunks;
    u_int32_t	netfile_count, totallines = 0;
    char	buf[BUFSIZ];

    list = sorted_file_list(tree, &file_count);

    fputs("#SCF-A 1.1\n", ofp);
    fputs("Generator-Program: comparator 1.0\n", ofp);
    fputs("Hash-Method: MD5\n", ofp);
    buf[0] = '\0';
    if (remove_whitespace)
	strcat(buf, "remove-whitespace,");
    if (remove_comments)
	strcat(buf, "remove-comments,");
    if (remove_braces)
	strcat(buf, "remove-braces,");
    if (buf[0])
	buf[strlen(buf)-1] = '\0';
    else
	strcpy(buf, "none");
    fprintf(ofp, "Normalization: %s\n", buf);
    fprintf(ofp, "Shred-Size: %d\n", shredsize);
    fputs("%%\n", ofp);

    netfile_count = htonl(file_count);
    fwrite(&netfile_count, sizeof(u_int32_t), 1, ofp);
    fprintf(stderr, "%% Reading %s...   ", tree);
    progress = totalchunks = 0;
    for (place = list; place < list + file_count; place++)
    {
	linenum_t	net_chunks;
	struct hash_t	*np;

	/* shredfile adds data to chunk_buffer */
	chunk_buffer = (struct hash_t *)calloc(sizeof(struct hash_t), 1);
	chunk_count = 0;
	totallines += shredfile(*place, filehook);

	/* the actual output */
	fputs(*place, ofp);
	fputc('\n', ofp);
	net_chunks = TONET(chunk_count);
	fwrite((char *)&net_chunks, sizeof(linenum_t), 1, ofp);
	if (debug)
	    fprintf(stderr, "Chunks for %s:\n", *place);
	for (np = chunk_buffer; np < chunk_buffer + chunk_count; np++)
	{
	    struct hash_t	this = np[0];

	    if (debug)
		fprintf(stderr,
		       "%d: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %s:%d:%d\n", 
		       np-chunk_buffer, 
		       this.hash[0], 
		       this.hash[1], 
		       this.hash[2], 
		       this.hash[3], 
		       this.hash[4], 
		       this.hash[5], 
		       this.hash[6], 
		       this.hash[7], 
		       this.hash[8], 
		       this.hash[9], 
		       this.hash[10], 
		       this.hash[11], 
		       this.hash[12], 
		       this.hash[13], 
		       this.hash[14], 
		       this.hash[15],
		       *place, this.start, this.end);
	    this.start = TONET(this.start);
	    this.end   = TONET(this.end);
	    fwrite(&this.start, sizeof(linenum_t), 1, ofp);
	    fwrite(&this.end,   sizeof(linenum_t), 1, ofp);
	    fwrite(&this.hash,  sizeof(char), HASHSIZE, ofp);
	}
	totalchunks += chunk_count;
	free(chunk_buffer);
	if (!debug && progress++ % 100 == 0)
	    fprintf(stderr, "\b\b\b%02.0f%%", progress / (file_count * 0.01));
    }
    fprintf(stderr, "\b\b\b100%%, done, %d total chunks.\n", totalchunks);

    /* the statistics trailer */
    totallines = htonl(totallines);
    fwrite(&totallines, sizeof(u_int32_t), 1, ofp);
}

static void read_scf(struct scf_t *scf)
/* merge hashes from specified files into an in-code list */
{
    u_int32_t	filecount, totallines;
    int hashcount = 0;
    struct stat sb;

    stat(scf->name, &sb);
    fprintf(stderr, "%% Reading hash list %s...   ", scf->name);
    fread(&filecount, sizeof(u_int32_t), 1, scf->fp);
    filecount = ntohl(filecount);
    while (filecount--)
    {
	char	buf[BUFSIZ];
	linenum_t	chunks;

	fgets(buf, sizeof(buf), scf->fp);
	*strchr(buf, '\n') = '\0';
	/*
	 * This isn't freed anywhere before end of execution.
	 * That's a potential memory leak if this routine is misapplied
	 */
	scf->name = strdup(buf);

	fread(&chunks, sizeof(linenum_t), 1, scf->fp);
	chunks = FROMNET(chunks);
	while (chunks--)
	{
	    struct hash_t	this;

	    fread(&this.start, sizeof(linenum_t), 1, scf->fp);
	    fread(&this.end,  sizeof(linenum_t), 1, scf->fp);
	    fread(this.hash, sizeof(char), HASHSIZE, scf->fp);
	    this.start = FROMNET(this.start);
	    this.end = FROMNET(this.end);
	    corehook(this, scf->name);
	    hashcount++;
	    if (!debug && hashcount % 10000 == 0)
		fprintf(stderr,"\b\b\b%02.0f%%",(ftell(scf->fp) / (sb.st_size * 0.01)));
	}
    }
    fprintf(stderr, "\b\b\b100%%...done, %d shreds\n", hashcount);

    fread(&scf->totallines, sizeof(u_int32_t), 1, scf->fp);
}

static int merge_tree(char *tree)
/* add to the in-core list of sorthash structures from a tree */
{
    char	**place, **list;
    int	old_entry_count, file_count, i;
    u_int32_t	totallines = 0;

    old_entry_count = sort_count;
    file_count = 0;
    fprintf(stderr, "%% Scanning tree %s...", tree);
    list = sorted_file_list(tree, &file_count);
    if (!file_count)
    {
	fprintf(stderr, 
		"comparator: couldn't open %s, %s\n", tree, strerror(errno));
	exit(1);
    }
    i = 0;
    fprintf(stderr, "reading...   ");
    for (place = list; place < list + file_count; place++)
    {
	totallines += shredfile(*place, corehook);
	if (!debug && !(i++ % 100))
	    fprintf(stderr, "\b\b\b%02.0f%%", i / (file_count * 0.01));
    }
    fprintf(stderr, "\b\b\b100%%...done, %d files, %d shreds.\n", 
	    file_count, sort_count - old_entry_count);
    free(list);
    return(totallines);
}

static void init_scf(char *file, struct scf_t *scf)
/* add to the in-core list of sorthash structures from a SCF file */
{
    char	buf[BUFSIZ];

    /* read in the SCF metadata block and add it to the in-core list */
    scf->name = strdup(file);
    scf->fp   = fopen(scf->name, "r");
    fgets(buf, sizeof(buf), scf->fp);
    if (strncmp(buf, "#SCF-A 1.1", 9))
    {
	fprintf(stderr, 
		"shredcompare: %s is not a SCF-A file.\n", 
		scf->name);
	exit(1);
    }
    while (fgets(buf, sizeof(buf), scf->fp) != NULL)
    {
	char	*value;

	if (!strcmp(buf, "%%\n"))
	    break;
	value = strchr(buf, ':');
	*value++ = '\0';
	while(*value == ' ')
	    value++;
	strchr(value, '\n')[0] = '\0';

	if (!strcmp(buf, "Normalization"))
	    scf->normalization = strdup(value);
	else if (!strcmp(buf, "Shred-Size"))
	    scf->shred_size = atoi(value);
	else if (!strcmp(buf, "Hash-Method"))
	    scf->hash_method = strdup(value);
	else if (!strcmp(buf, "Generator-Program"))
	    scf->generator_program = strdup(value);
    }
}

static void dump_array(struct sorthash_t *obarray, 
		int hashcount, 
		linenum_t (*fn)(linenum_t))
/* dump the contents of a sort_hash array */
{
    struct sorthash_t	*np;

    for (np = obarray; np < obarray + hashcount; np++)
    {
	struct hash_t scratch;

	scratch = np->hash;
	if (fn)
	{
	    scratch.start = fn(np->hash.start);
	    scratch.end   = fn(np->hash.end);
	}
	printf("%d: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %s:%d:%d\n", 
	       np-obarray, 
	       scratch.hash[0], 
	       scratch.hash[1], 
	       scratch.hash[2], 
	       scratch.hash[3], 
	       scratch.hash[4], 
	       scratch.hash[5], 
	       scratch.hash[6], 
	       scratch.hash[7], 
	       scratch.hash[8], 
	       scratch.hash[9], 
	       scratch.hash[10], 
	       scratch.hash[11], 
	       scratch.hash[12], 
	       scratch.hash[13], 
	       scratch.hash[14], 
	       scratch.hash[15],
	       np->file, scratch.start, scratch.end);
    }
}

void report_time(char *legend, ...)
/* report on time since last report_mark */
{
    static time_t mark_time;
    time_t endtime = time(NULL);
    va_list	ap;

    if (mark_time)
    {
    	int elapsed, hours, minutes, seconds;
	char	buf[BUFSIZ];
	elapsed = endtime - mark_time;
	hours = elapsed/3600; elapsed %= 3600;
	minutes = elapsed/60; elapsed %= 60;
	seconds = elapsed;

	va_start(ap, legend);
	vsprintf(buf, legend, ap);
	fprintf(stderr, "%% %s: %dh %dm %ds\n", buf, hours, minutes, seconds);
    }
    mark_time = endtime;
}

static void usage(void)
{
    fprintf(stderr,"usage: comparator [-c] [-C] [-d dir ] [-r] [-o file] [-s shredsize] [-w] [-x] path...\n");
    fprintf(stderr,"  -c      = generate SCF files\n");
    fprintf(stderr,"  -C      = apply C normalizations\n");
    fprintf(stderr,"  -d dir  = change directory before digesting.\n");
    fprintf(stderr,"  -o file = write to the specified file.\n");
    fprintf(stderr,"  -r      = renove comments\n");
    fprintf(stderr,"  -s size = set shred size (default %d)\n", shredsize);
    fprintf(stderr,"  -w      = remove whitespace.\n");
    fprintf(stderr,"  -x      = debug, display chunks in output.\n");
    exit(0);
}

int
main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status, file_only, compile_only, argcount;
    struct scf_t	*scf;
    char *dir, *outfile;
    struct sorthash_t *np;

    compile_only = file_only = 0;
    dir = outfile = NULL;
    while ((status = getopt(argc, argv, "cCd:ho:rs:wx")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    compile_only = 1;
	    break;

	case 'C':
	    remove_whitespace = remove_braces = 1;
	    break;

	case 'd':
	    dir = optarg;
	    break;

	case 'o':
	    outfile = optarg;
	    break;

	case 'r':
	    remove_comments = 1;

	case 's':
	    shredsize = atoi(optarg);
	    break;

	case 'w':
	    remove_whitespace = 1;
	    break;

	case 'x':
	    debug = 1;
	    break;

	default:
	    usage();
	}
    }

    if (!compile_only)
    {
	sort_buffer = (struct sorthash_t*)calloc(sizeof(struct sorthash_t),1);
	sort_count = 0;
    }

    argcount = (argc - optind);
    if (argcount == 0)
	usage();

    report_time(NULL);

    if (outfile)
    {
	if (freopen(outfile, "w", stdout) == NULL)
	{
	    perror("comparator");
	    exit(1);
	}
    }

    /* special case if user gave exactly one tree */
    if (!compile_only && argcount == 1)
    {
	write_scf(argv[optind], stdout);
	exit(0);
    }

    /* two or more arguments */
    for(; optind < argc; optind++)
    {
	char	*olddir, *source = argv[optind];

	if (is_scf_file(source))
	{
	    if (dir)
	    {
		olddir = getcwd(NULL, 0);	/* may fail off Linux */
		chdir(dir);
	    }
	    scf = (struct scf_t *)malloc(sizeof(struct scf_t));
	    init_scf(source, scf);
	    scf->next = scflist;
	    scflist = scf;
	    if (dir)
		chdir(olddir);
	}
	else if (compile_only)
	{
	    char	*outfile = alloca(strlen(source) + 4);
	    FILE	*ofp;

	    strcpy(outfile, source);
	    strcat(outfile, ".scf");
	    ofp = fopen(outfile, "w");
	    if (!ofp)
	    {
		fprintf(stderr, "comparator: couldn't open output file %s\n",
			outfile);
		exit(1);
	    }

	    if (dir)
	    {
		olddir = getcwd(NULL, 0);	/* may fail off Linux */
		chdir(dir);
	    }
	    write_scf(source, ofp);
	    if (dir)
		chdir(olddir);
	    fclose(ofp);
	}
	else
	{
	    if (dir)
	    {
		olddir = getcwd(NULL, 0);	/* may fail off Linux */
		chdir(dir);
	    }
	    merge_tree(source);
	    if (dir)
		chdir(olddir);
	}
    }

    /* if there were no SCFs, create a dummy one */
    if (scflist == &dummy_scf)
    {
	char	buf[BUFSIZ];

	scflist->hash_method = "MD5";
	buf[0] = '\0';
	if (remove_whitespace)
	    strcat(buf, "remove-whitespace, ");
	if (remove_comments)
	    strcat(buf, "remove-comments, ");
	if (remove_braces)
	    strcat(buf, "remove-braces, ");
	if (buf[0])
	    buf[strlen(buf)-1] = '\0';
	else
	    strcpy(buf, "none");
	scflist->normalization = strdup(buf);
	scflist->shred_size = shredsize;
	scflist->generator_program = "comparator " VERSION;
    }
    else
    {
	/* consistency checks on the SCFs */
	for (scf = scflist; scf->next->next; scf = scf->next)
	{
	    if (strcmp(scf->normalization, scf->next->normalization))
	    {
		fprintf(stderr, 
			"shredcompare: normalizations of %s and %s don't match\n",
			scf->name, scf->next->name);
		exit(1);
	    }
	    else if (scf->shred_size != scf->next->shred_size)
	    {
		fprintf(stderr, 
			"shredcompare: shred sizes of %s and %s don't match\n",
			scf->name, scf->next->name);
		exit(1);

	    }
	    else if (strcmp(scf->hash_method, scf->next->hash_method))
	    {
		fprintf(stderr, 
			"shredcompare: hash methods of %s and %s don't match\n",
			scf->name, scf->next->name);
		exit(1);
	    }
	}

	/* finish reading in all SCFs */
	for (scf = scflist; scf->next; scf = scf->next)
	{
	    read_scf(scf);
	    fclose(scf->fp);
	}

    }

    if (!compile_only)
    {
	if (debug)
	{
	    printf("Consolidated hash list:\n");
	    dump_array(sort_buffer, sort_count, NULL);
	}

	/* now we're ready to emit the report */
	puts("#SCF-B 1.0");
	printf("Hash-Method: %s\n", scflist->hash_method);
	puts("Merge-Program: comparator 1.0");
	printf("Normalization: %s\n", scflist->normalization);
	printf("Shred-Size: %d\n", scflist->shred_size);
	puts("%%");

	report_time("Hash merge done, %d shreds", sort_count);
	sort_hashes(sort_buffer, sort_count);
	report_time("Sort done");

	if (debug)
	{
	    puts("Chunk list before reduction.");
	    dump_array(sort_buffer, sort_count, NULL);
	}

	emit_report(sort_buffer, sort_count);
    }

    exit(0);
}

/* main.c ends here */
