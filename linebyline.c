/******************************************************************************

NAME:
   linebyline.c -- line-oriented feature analyzer four source comparisons

******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <ctype.h>
#include <string.h>
#include "shred.h"

/* control bits */
static int remove_braces = 0;
static int remove_whitespace = 0;
static int remove_comments = 0;

static char *c_patterns[] = {
    /* Idioms that don't convey any meaning in isolation */
    "return [a-z]+", "return [01]+",
    "goto +[a-z]+", "exit *[01]",
    /* Pragmas */
    " ARGSUSED ",
    " NOTREACHED ", 
    " FALL *THRO?UG?H? ",
    /* Bare C keywords */
    " auto ", " break ",  " case ", "char", " const ", " continue ", 
    " default ", " do ", " double ", " else ", " enum ", " extern ",
    " float ", " for ", " goto ", " if ", " int ", " long ", " register ",
    " return ", " short ", " signed ", " sizeof ", " static ", " struct ", 
    " switch ", " typedef ", " union ", " unsigned ", " void ",
    " volatile ", " while ",
    /* Preprocessor constructs (# has already been stripped) */
    "^ define"," endif"," else", " ifdef "," ifndef ",
    /* Common preprocessor macros, not significant by themselves. */
    " ASSERT ", " EXTERN ", " FALSE ", " NULL "," STATIC ", " TRUE ",
    /* Macro include lines are noise, too. */
    " include .*"," line .*",
    /* Common error macros. */
    " EFAULT ",
    " EINVAL ",
    " ENOSYS ",
};
static regex_t c_regexps[sizeof(c_patterns)/sizeof(*c_patterns)];

static char *shell_patterns[] = {
    " break ", " case ", " done ", " do ", " else ", " esac ", " exit *[01]?",
    " false ", " fi ", " for", " function", " if ", " return ", " shift ", 
    " true ", "until", " while ", 
};
static regex_t shell_regexps[sizeof(shell_patterns)/sizeof(*shell_patterns)];

static linenum_t	linecount;

int analyzer_init(const char *buf)
/* initialize line filtering */
{
    int i;
    char	*cp;

    for (i = 0; i < sizeof(c_patterns)/sizeof(*c_patterns); i++)
	if (regcomp(c_regexps+i, c_patterns[i], REG_EXTENDED))
	{
	    fprintf(stderr, "comparator: error while compiling %s\n",
		    c_patterns[i]);
	    exit(1);
	}
    for (i = 0; i < sizeof(shell_patterns)/sizeof(*shell_patterns); i++)
	if (regcomp(shell_regexps+i, shell_patterns[i], REG_EXTENDED))
	{
	    fprintf(stderr, "comparator: error while compiling %s\n", 
		    shell_patterns[i]);
	    exit(1);
	}

    cp = strtok(strdup((const char *)buf), ", ");
    if (strcmp(cp, "line-oriented"))
	return(1);
    while (cp = strtok(NULL, ", "))
    {
	if (strcmp(cp, "remove-whitespace") == 0)
	    remove_whitespace = 1;
	else if (strcmp(cp, "remove-comments") == 0)
	    remove_comments = 1;
	else if (strcmp(cp, "remove-braces") == 0)
	    remove_braces = 1;
	else
	    return(-1);
    }
    return(0);
}

static unsigned char	active = 0;
static regex_t *regexps;
static int nregexps;

void analyzer_mode(int mask)
/* set the analyzer mode -- meant to be called at the start of a file scan */
{
    active = mask;

    if (mask & C_CODE)
    {
	regexps = c_regexps;
	nregexps = sizeof(c_regexps)/sizeof(*c_regexps);
    }
    else if (mask & SHELL_CODE)
    {
	regexps = shell_regexps;
	nregexps = sizeof(shell_regexps)/sizeof(*shell_regexps);
    }

    /* this may have to be fixed someday! */
    linecount = 0;
}

static int normalize(char *buf)
/* normalize a buffer in place, return 0 if it should be skipped */
{
    if (remove_comments)
    {
	if (active & C_CODE)	/* remove C comments */
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

static int filter_pass(const char *line)
/* return flags that apply to this line */
{
    if (active == 0)
	return(0);
    else
    {
	char	*sp, *tp;
	char	buf[BUFSIZ];
	int	changed;

	/* change all punctuation to spaces */
	buf[0] = ' ';
	for (sp = (char *)line, tp = buf+1; *sp; sp++)
	    if (ispunct(*sp) || *sp == '\n' || *sp == '\t')
		*tp++ = ' ';
	    else
		*tp++ = *sp;
	*tp = '\0';

#ifdef TEST
	fprintf(stderr, "After removing punctuation: '%s'\n", buf);
#endif /* TEST */

	if (strspn(line, " ") == strlen(line))
	    return(active);

	do {
	    regex_t	*re;

	    changed = 0;

	    /* replace all regexps with the empty string */
	    for (re = regexps; re < regexps + nregexps; re++)
	    {
		regmatch_t f;

		if (!regexec(re, buf, 1, &f, 0))
		{
		    char	*start, *end;

		    for (start = buf+f.rm_so, end = buf+f.rm_eo; *end; end++)
			*start++ = *end;
		    *start = '\0'; 
#ifdef TEST
		    fprintf(stderr, "%d...%d -> '%s'\n", f.rm_so, f.rm_eo, buf);
#endif /* TEST */
		    changed++;
		}
	    }
	} while
	    (changed);

	return (buf[0] == '\0' || strspn(buf, " ") == strlen(buf));
    }
}

feature_t *analyzer_get(const struct filehdr_t *file, FILE *fp, linenum_t *linenump)
/* get a feature (in this case, a line) from the input stream */
{
    char	buf[BUFSIZ];
    static feature_t	feature;

    while (fgets(buf, sizeof(buf), fp) != NULL)
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
	    continue;

	/* maybe we can get the file type from the first line? */
	if (linecount == 1 && buf[0] == '#')
	    if (strstr(buf, "sh"))
	    {
		analyzer_mode(SHELL_CODE);
		linecount = 1;
	    }

	/* time to return the feature */
	feature.text = strdup(buf);
	feature.flags = filter_pass(buf) ? INSIGNIFICANT : 0;
	*linenump = linecount;
	return &feature;
    }

    *linenump = linecount;
    return(NULL);
}

void analyzer_free(const char *text)
/* free a piece of storage previously handed to shredtree */
{
    free((char *)text);
}

void analyzer_dump(char *buf)
/* dump a Normalization line representing current options */
{
    strcpy(buf, "line-oriented, ");
    if (remove_whitespace)
	strcat(buf, "remove-whitespace, ");
    if (remove_comments)
	strcat(buf, "remove-comments, ");
    if (remove_braces)
	strcat(buf, "remove-braces, ");
    if (buf[0])
	buf[strlen(buf)-2] = '\0';
}

/* our method table */
struct analyzer_t linebyline =
{
    init: analyzer_init,
    mode: analyzer_mode,
    get:  analyzer_get,
    free: analyzer_free,
    dumpopt: analyzer_dump,
};

#ifdef TEST
int main(int argc, char *argv[])
{
    char	buf[BUFSIZ];

    analyzer_init();
    analyzer_mode(C_CODE);
    while(fgets(buf, BUFSIZ, stdin))
    {
	printf("%02x: ", filter_pass(buf));
	fputs(buf, stdout);
    }
}
#endif /* TEST */

/* linebyline.c ends here */
