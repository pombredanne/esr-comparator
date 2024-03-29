/*
 * main.c -- main sequence of comparator
 *
 * SPDX-License-Identifier: BSD-2-clause
 */

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#ifdef __sun
#include <alloca.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "shred.h"

int verbose, debug, minsize, nofilter;

struct scf_t
{
    char	*name;
    char	*file;
    FILE	*fp;
    u_int32_t	totallines;
    char	*normalization;
    int		shred_size;
    char	*hash_method;
    char	*generator_program;
    struct scf_t *next;
};
static struct scf_t dummy_scf, *scflist = &dummy_scf;

static struct filehdr_t dummy_filehdr, *filelist = &dummy_filehdr;

static int chunk_count, sort_count, dofilter;
static size_t chunk_buffer_alloc_sz;
static struct hash_t *chunk_buffer;
static size_t sort_buffer_alloc_sz;
static struct sorthash_t *sort_buffer;

struct filehdr_t *register_file(const char *file, linenum_t length)
/* register a file and its line count into the in-core list */
{
    struct filehdr_t	*new = (struct filehdr_t *)malloc(sizeof(struct filehdr_t));
    new->name = strdup(file);
    new->length = length;
    new->next = filelist;
    filelist = new;
    return(new);
}

static bool is_scf_file(const char *file)
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
    return(strncmp(buf, "#SCF-A ", 7) == 0);
}

/* 
 * We differentiate these two functions in order to avoid the memory
 * overhead of the extra (char *) when assembling shred lists to be written
 * to a file.  This is only worth bothering with because the data sets
 * can get quite large.
 */

static void filehook(struct hash_t hash, struct filehdr_t *file)
/* hook to store only hashes */
{
    if (chunk_buffer_alloc_sz < chunk_count + 1) {
	chunk_buffer_alloc_sz = 2*chunk_buffer_alloc_sz + 1;
	chunk_buffer = (struct hash_t *)realloc(chunk_buffer, 
			  sizeof(struct hash_t) * (chunk_buffer_alloc_sz));
    }

    chunk_buffer[chunk_count++] = hash;
}


void corehook(struct hash_t hash, struct filehdr_t *file)
/* hook to store hash and file */
{
    if (sort_buffer_alloc_sz < sort_count + 1) {
	sort_buffer_alloc_sz = 2*sort_buffer_alloc_sz + 1;
	sort_buffer = (struct sorthash_t *)realloc(sort_buffer, 
			  sizeof(struct sorthash_t) * (sort_buffer_alloc_sz));
    }
    
    sort_buffer[sort_count].hash = hash;
    sort_buffer[sort_count].file = file;
    sort_count++;
}

void extend_current_chunk(linenum_t linenumber)
/* set the end-line number on the last chunk, if there is one */
{
    if (sort_count > 0)
	sort_buffer[sort_count-1].hash.end = linenumber;
    if (chunk_count > 0)
	chunk_buffer[chunk_count-1].end = linenumber;
}

static void write_scf(const char *tree, FILE *ofp)
/* generate shred file for given tree */
{
    char	**place, **list;
    int		file_count, progress, totalchunks;
    linecount_t	netfile_count, totallines = 0;
    char	buf[BUFSIZ];

    if (verbose)
	fprintf(stderr, "%% Scanning tree %s...", tree);
    list = sorted_file_list(tree, &file_count);
    if (!file_count)
    {
	fprintf(stderr, 
		"comparator: couldn't open %s, %s\n", tree, strerror(errno));
	exit(1);
    }

    fputs("#SCF-A 2.0\n", ofp);
    fputs("Generator-Program: comparator 1.0\n", ofp);
    fputs("Hash-Method: " HASHMETHOD "\n", ofp);
    linebyline.dumpopt(buf);
    fprintf(ofp, "Normalization: %s\n", buf);
    fprintf(ofp, "Root: %s\n", tree);
    fprintf(ofp, "Shred-Size: %d\n", shredsize);
    fputs("%%\n", ofp);

    netfile_count = htonl(file_count);
    fwrite(&netfile_count, sizeof(linecount_t), 1, ofp);
    if (verbose)
	fprintf(stderr, "reading %d files...    ", file_count);
    progress = totalchunks = 0;
    for (place = list; place < list + file_count; place++)
    {
	linenum_t	net_chunks, lines;
	struct hash_t	*np;
	struct filehdr_t	*filep;

	/* shredfile adds data to chunk_buffer */
	chunk_buffer = (struct hash_t *)calloc(sizeof(struct hash_t), 1);
	chunk_count = 0;
	filep = register_file(*place, 0);
	lines = shredfile(filep, filehook);
	filep->length = lines;

	/* the actual output */
	fputs(*place, ofp);
	fputc('\n', ofp);
	totallines += lines;
	lines = TONET(lines);
	fwrite((char *)&lines, sizeof(linenum_t), 1, ofp);
	net_chunks = TONET(chunk_count);
	fwrite((char *)&net_chunks, sizeof(linenum_t), 1, ofp);
	if (debug)
	    fprintf(stderr, "Chunks for %s:\n", *place);
	for (np = chunk_buffer; np < chunk_buffer + chunk_count; np++)
	{
	    struct hash_t	this = np[0];

	    if (debug)
	    {
		fprintf(stderr,
			"%ld: %s %s:%d:%d",
			np-chunk_buffer, hash_dump(this.hash),
		       *place, this.start, this.end);
		if (np->flags)
		{
		    fputc('\t', stderr);
		    dump_flags(np->flags, stderr);
		    fprintf(stderr, " (0x%02x)", np->flags);
		}
		fputc('\n', stderr);
	    }
	    this.start = TONET(this.start);
	    this.end   = TONET(this.end);
	    fwrite(&this.start, sizeof(linenum_t), 1, ofp);
	    fwrite(&this.end,   sizeof(linenum_t), 1, ofp);
	    fwrite(&this.hash,  sizeof(hashval_t), 1, ofp);
	    fwrite(&this.flags, sizeof(flag_t), 1, ofp);
	}
	totalchunks += chunk_count;
	free(chunk_buffer);
	chunk_buffer_alloc_sz = 0;
	if (verbose && !debug && progress++ % 100 == 0)
	    fprintf(stderr, "\b\b\b\b%3.0f%%", progress / (file_count * 0.01));
    }
    if (verbose)
	fprintf(stderr, "\b\b\b\b100%%, done, %d total chunks.\n",totalchunks);

    /* the statistics trailer */
    totallines = htonl(totallines);
    fwrite(&totallines, sizeof(linecount_t), 1, ofp);
}

static void read_scf(struct scf_t *scf)
/* merge hashes from specified files into an in-code list */
{
    linecount_t	filecount;
    int hashcount = 0;
    struct stat sb;

    stat(scf->file, &sb);
    if (verbose)
	fprintf(stderr, "%% Reading hash list %s...    ", scf->file);
    if (fread(&filecount, sizeof(linecount_t), 1, scf->fp) != 1) 
    {
	(void)fputs("comparator: fread() failed!\n", stderr);
    }
    filecount = ntohl(filecount);
    while (filecount--)
    {
	char	buf[BUFSIZ];
	linenum_t	lines, chunks;
	struct filehdr_t	*filehdr;

	if (fgets(buf, sizeof(buf), scf->fp) == NULL)
	{
	    (void)fputs("comparator: fgets() failed!\n", stderr);
	}
	*strchr(buf, '\n') = '\0';

	if (fread(&lines, sizeof(linenum_t), 1, scf->fp) != 1)
	{
	    (void)fputs("comparator: fread() failed!\n", stderr);
	}
	lines = FROMNET(lines);
	filehdr = register_file(buf, lines);

	if (fread(&chunks, sizeof(linenum_t), 1, scf->fp) != 1)
	{
	    (void)fputs("comparator: fread() failed!\n", stderr);
	}
	chunks = FROMNET(chunks);
	while (chunks--)
	{
	    struct hash_t	this;

	    if (fread(&this.start, sizeof(linenum_t), 1, scf->fp) != 1
		|| fread(&this.end,  sizeof(linenum_t), 1, scf->fp) != 1
		|| fread(&this.hash, sizeof(hashval_t), 1, scf->fp) != 1
		|| fread(&this.flags, sizeof(flag_t), 1, scf->fp) != 1)
	    {
		(void)fputs("comparator: fread() failed!\n", stderr);
	    }
	    this.start = FROMNET(this.start);
	    this.end = FROMNET(this.end);
	    corehook(this, filehdr);
	    hashcount++;
	    if (verbose && !debug && hashcount % 10000 == 0)
		fprintf(stderr,"\b\b\b\b%3.0f%%",(ftell(scf->fp) / (sb.st_size * 0.01)));
	}
    }
    if (verbose)
	fprintf(stderr, "\b\b\b\b100%%...done, %d shreds\n", hashcount);

    if (fread(&scf->totallines, sizeof(linecount_t), 1, scf->fp) != 1)
    {
	(void)fputs("comparator: fread() failed!\n", stderr);
    }
    scf->totallines = ntohl(scf->totallines);
}

static int merge_tree(char *tree)
/* add to the in-core list of sorthash structures from a tree */
{
    char	**place, **list;
    int	old_entry_count, file_count, i;
    linecount_t	totallines = 0;

    old_entry_count = sort_count;
    file_count = 0;
    if (verbose)
	fprintf(stderr, "%% Scanning tree %s...", tree);
    list = sorted_file_list(tree, &file_count);
    if (!file_count)
    {
	fprintf(stderr, 
		"comparator: couldn't open %s, %s\n", tree, strerror(errno));
	exit(1);
    }
    i = 0;
    if (verbose)
	fprintf(stderr, "reading %d files...    ", file_count);
    for (place = list; place < list + file_count; place++)
    {
	struct filehdr_t *filep = register_file(*place, 0);
	linenum_t lines = shredfile(filep, corehook);

	filep->length = lines; 
	totallines += lines;
	if (verbose && !debug && !(i++ % 100))
	    fprintf(stderr, "\b\b\b\b%3.0f%%", i / (file_count * 0.01));
    }
    if (verbose)
	fprintf(stderr, "\b\b\b\b100%%...done, %d files, %d shreds.\n", 
		file_count, sort_count - old_entry_count);
    free(list);
    return(totallines);
}

static void init_scf(char *file, struct scf_t *scf, const int readfile)
/* add to the in-core list of sorthash structures from a SCF file */
{
    scf->file = strdup(file);
    if (readfile)
    {
	char	buf[BUFSIZ];

	/* read in the SCF metadata block and add it to the in-core list */
	scf->fp   = fopen(scf->file, "r");
	if (!scf->fp)
	{
	    (void)fprintf(stderr, 
			  "comparator: file %s, %s", 
			  scf->file, strerror(errno));
	    exit(1);
	}
	if (fgets(buf, sizeof(buf), scf->fp) == NULL)
	{
	    (void)fputs("comparator: frgets() failed!\n", stderr);
	}
	if (strncmp(buf, "#SCF-A 2.0", 9))
	{
	    fprintf(stderr, 
		    "comparator: %s is not a SCF-A file.\n", 
		    scf->file);
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
	    else if (!strcmp(buf, "Root"))
		scf->name = strdup(value);
	}
    }
    else
    {
	char	buf[BUFSIZ];

	scf->hash_method = HASHMETHOD;
	linebyline.dumpopt(buf);
	scf->normalization = strdup(buf);
	scf->shred_size = shredsize;
	scf->generator_program = "comparator " VERSION;
	scf->name = strdup(file);
    }
}

void dump_flags(const int flags, FILE *fp)
/* dump tokens corresponding to a flag set */
{
    fputc(' ', fp);
    if (flags & INSIGNIFICANT)
	fputs(" insignificant", fp);
    if (flags & C_CODE)
	fputs(" C", fp);
    if (flags & SHELL_CODE)
	fputs(" shell", fp);
}

void dump_array(const char *legend, 
		struct sorthash_t *obarray, int hashcount)
/* dump the contents of a sort_hash array */
{
    struct sorthash_t	*np;

    fputs(legend, stdout);
    for (np = obarray; np < obarray + hashcount; np++)
	if (!(np->hash.flags & INTERNAL_FLAG))
	{
	    fprintf(stdout, "%2ld: %s %s:%d:%d",
		    np-obarray, 
		    hash_dump(np->hash.hash),
		    np->file->name, np->hash.start, np->hash.end);
	    if (np->hash.flags)
	    {
		fputc('\t', stdout);
		dump_flags(np->hash.flags, stdout);
		fprintf(stdout, " (0x%02x)", np->hash.flags);
	    }
	    fputc('\n', stdout);
	}
}

void report_time(char *legend, ...)
/* report on time since last report_mark */
{
    static time_t mark_time;
    time_t endtime = time(NULL);
    va_list	ap;

    if (mark_time && verbose)
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

static FILE *redirect(const char *outfile)
/* reditrect output to specified file */
{
    if (outfile)
    {
	if (freopen(outfile, "w", stdout) == NULL)
	{
	    perror("comparator");
	    exit(1);
	}
    }

    return(stdout);
}

static void usage(void)
{
    fprintf(stderr,"usage: comparator [-h] [-c] [-C] [-d dir ] [-m minsize] [-n] [-o file] [-s shredsize] [-v] [-x] path...\n");
    fprintf(stderr,"  -h      = print this help\n");
    fprintf(stderr,"  -c      = generate SCF files\n");
    fprintf(stderr,"  -d dir  = change directory before digesting.\n");
    fprintf(stderr,"  -m size = set minimum size of span to be output.\n");
    fprintf(stderr,"  -n      = suppress significance filtering.\n");
    fprintf(stderr,"  -o file = write to the specified file.\n");
    fprintf(stderr,"  -s size = set shred size (default %d)\n", shredsize);
    fprintf(stderr,"  -v      = enable progress messages on stderr.\n");
    fprintf(stderr,"  -x      = debug, display chunks in output.\n");
    fprintf(stderr,"This is comparator version " VERSION ".\n");
    exit(0);
}

int
main(int argc, char *argv[])
{
    extern char	*optarg;	/* set by getopt */
    extern int	optind;		/* set by getopt */

    int status, file_only, compile_only, argcount, mergecount;
    struct scf_t	*scf;
    char *dir, *outfile, *normalization = "line-oriented";

    compile_only = file_only = nofilter = 0;
    dir = outfile = NULL;
    while ((status = getopt(argc, argv, "cd:hm:nN:o:s:vx")) != EOF)
    {
	switch (status)
	{
	case 'c':
	    compile_only = 1;
	    break;

	case 'd':
	    dir = optarg;
	    break;

	case 'm':
	    minsize = atoi(optarg);
	    break;

	case 'n':
	    nofilter = 1;
	    break;

	case 'N':
	    normalization = strdup(optarg);
	    break;

	case 'o':
	    outfile = optarg;
	    break;

	case 's':
	    shredsize = atoi(optarg);
	    break;

	case 'v':
	    verbose = 1;
	    break;

	case 'x':
	    debug = 1;
	    break;

	case 'h':
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

    /*
     * Check each normalizer to see if it fires.
     */
    status = linebyline.init(normalization);
    if (status != 0)
    {
	fprintf(stderr, "comparator: bad analyzer option\n");
	exit(1);
    }

    /* special case if user gave exactly one tree */
    if (!compile_only && argcount == 1)
    {
	if (dir)
	{
	    if (chdir(dir) != 0)
	    {
		fprintf(stderr, "comparator: single-tree cd %s failed!\n", dir);
		exit(1);
	    }
	}
	write_scf(argv[optind], redirect(outfile));
	exit(0);
    }

    /* two or more arguments */
    for(; optind < argc; optind++)
    {
	char	*olddir = NULL, *source = argv[optind];

	scf = (struct scf_t *)calloc(sizeof(struct scf_t), 1);
	scf->next = scflist;
	scflist = scf;

	if (is_scf_file(source))
	    init_scf(source, scf, 1);
	else if (compile_only)
	{
	    FILE	*ofp;
	    char	*scf_out;	

	    if (outfile)
		scf_out = outfile;
	    else
	    {
		scf_out  = alloca(strlen(source) + 4);
		strcpy(scf_out, source);
		strcat(scf_out, ".scf");
	    }
	    ofp = redirect(scf_out);

	    if (dir)
	    {
		olddir = getcwd(NULL, PATH_MAX);
		if (chdir(dir) != 0)
		{
		    fprintf(stderr, "comparator: descent cd %s failed!\n", dir);
		    exit(1);
		}
	    }
	    write_scf(source, ofp);
	    if (dir) {
		if (chdir(olddir) != 0)
		{
		    fprintf(stderr, "comparator: return cd %s failed!\n", dir);
		    exit(1);
		}
	    }
	    fclose(ofp);
	}
	else
	{
	    if (dir)
	    {
		olddir = getcwd(NULL, PATH_MAX);
		if (chdir(dir) != 0)
		{
		    fprintf(stderr, "comparator: descent cd %s failed!\n", dir);
		    exit(1);
		}
	    }
	    init_scf(source, scf, 0);
	    scf->totallines = merge_tree(source);
	    if (dir)
	    {
		if (chdir(olddir) != 0)
		{
		    fprintf(stderr, "comparator: return cd %s failed!\n", dir);
		    exit(1);
		}
	    }
	}
    }

    if (compile_only)
	exit(0);

    /* are we running the right instance of comparator? */
    for (scf = scflist; scf->next; scf = scf->next)
	if (strcmp(scf->hash_method, HASHMETHOD))
	{
	    fprintf(stderr, 
		    "comparator: hash method %s of %s is not compiled in.\n",
		    scf->hash_method, scf->file);
	    exit(1);
	}

    /* consistency checks on the SCFs */
    for (scf = scflist; scf->next->next; scf = scf->next)
    {
	if (!scf->fp)
	    continue;

	if (strcmp(scf->normalization, scf->next->normalization))
	{
	    fprintf(stderr, 
		    "comparator: normalizations of %s and %s don't match\n",
		    scf->file, scf->next->file);
	    exit(1);
	}
	else if (scf->shred_size != scf->next->shred_size)
	{
	    fprintf(stderr, 
		    "comparator: shred sizes of %s and %s don't match\n",
		    scf->file, scf->next->file);
	    exit(1);

	}
    }

    /* finish reading in all SCFs */
    for (scf = scflist; scf->next; scf = scf->next)
	if (scf->fp)
	{
	    read_scf(scf);
	    scf->file[strlen(scf->file) - strlen(".scf")] = '\0';
	    fclose(scf->fp);
	}

    if (debug)
	dump_array("Consolidated hash list:\n", sort_buffer, sort_count);

    /* now we're ready to emit the report */
    redirect(outfile);
    puts("#SCF-B 2.0");
    printf("Filtering: %s\n", nofilter ? "none" : "language");
    printf("Hash-Method: %s\n", scflist->hash_method);

    report_time("Hash merge done, %d shreds", sort_count);
    sort_hashes(sort_buffer, sort_count);
    report_time("Sort done");

    mergecount = merge_compare(sort_buffer, sort_count);
    printf("Matches: %d\n", mergecount);
    puts("Merge-Program: comparator " VERSION);
    printf("Normalization: %s\n", scflist->normalization);
    printf("Shred-Size: %d\n", scflist->shred_size);

    puts("%%");
    for (scf = scflist; scf->next; scf = scf->next)
	printf("%s: matches=%d, matchlines=%d, totallines=%d\n", 
	       scf->name, 
	       match_count(scf->name), 
	       line_count(scf->name), 
	       scf->totallines);
    puts("%%");

    emit_report();

    exit(0);
}

/* main.c ends here */
