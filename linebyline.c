/* filter.c -- filter lines for significance */
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <ctype.h>
#include "shred.h"

static char *c_patterns[] = {
    /* Idioms that don't convey any meaning in isolation */
    "return [a-z]+", "return -?[01]+",
    "goto +[a-z]+", "exit *\\([01]\\)",
    /* Pragmas */
    "/\\* *ARGSUSED *\\*/",
    "/\\* *NOTREACHED *\\*/", 
    "/\\* *FALL *THRO?UG?H? *\\*/",
    /* Bare C keywords */
    " auto ", " break ",  " case "," continue ", " default ", " do ", " else ",
    " enum ", " if ", " goto "," return ", " switch ", " while ", " int ",
    " long ", " short ", " static ", " struct ", " typedef ", " union ", 
    " void ",
    /* Preprocessor constructs (# has already been stripped)*/
    "^ *define","^ *endif","^ *else", "^ *ifdef ","^ *ifndef ",
    /* Common preprocessor macros, not significant by themselves. */
    " ASSERT ", " EXTERN ", " FALSE ", " NULL "," STATIC ", " TRUE ",
    /* Macro include lines are noise, too. */
    "^ *include.*","^ *line.*",
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

void filter_init(void)
/* initialize line filtering */
{
    int i;

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
}

static unsigned char	active = 0;
static regex_t *regexps;
static int nregexps;

void filter_set(int mask)
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
}

int filter_pass(const char *line)
/* return flags that apply to this line */
{
    if (active == 0)
	return(SIGNIFICANT);
    else
    {
	char	*sp, *tp;
	char	buf[BUFSIZ];
	int	changed, significant;

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


	significant = !(buf[0] == '\0' || strspn(buf, " ") == strlen(buf));

	return (active | (significant ? SIGNIFICANT : 0));
    }
}

#ifdef TEST
int main(int argc, char *argv[])
{
    char	buf[BUFSIZ];

    filter_init();
    filter_set(C_CODE);
    while(fgets(buf, BUFSIZ, stdin))
    {
	printf("%02x: ", filter_pass(buf));
	fputs(buf, stdout);
    }
}
#endif /* TEST */

/* filter.c ends here */
