/* filter.c -- filter lines for significance */
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include "shred.h"

static char *c_patterns[] = {
    /* Idioms that don't convey any meaning in isolation */
    "return *\\(?[a-z]+\\)? *;", "return *\\(?-?[01]+\\)? *;",
    "goto +[a-z]+;", "exit *\\([01]\\);",
    /* Pragmas */
    "/\\* *ARGSUSED *\\*/",
    "/\\* *NOTREACHED *\\*/", 
    "/\\* *FALL *THRO?UG?H? *\\*/",
    /* Bare C keywords */

    "[:<:]break[:>:]",  "[:<:]case[:>:]","[:<:]continue[:>:]", 
    "[:<:]default[:>:]", "[:<:]do[:>:]", "[:<:]else[:>:]", 
    "[:<:]enum[:>:]", "[:<:]if[:>:]", "[:<:]goto[:>:]",
    "[:<:]return[:>:]", "[:<:]switch[:>:]", "[:<:]while[:>:]",
    "[:<:]enum[:>:]", "[:<:]int[:>:]", "[:<:]long[:>:]",
    "[:<:]short[:>:]", "[:<:]static[:>:]", "[:<:]struct[:>:]",
    "[:<:]typedef[:>:]", "[:<:]union[:>:]", "[:<:]void[:>:]",
    /* Preprocessor constructs */
    "/* *define","# *endif","# *else","# *if[:>:]",
    "# *ifdef[:>:]","# *ifndef[:>:]",
    /* Comment delimiters with no content, and line continuation */
    "/\\*+", "\\*+/", "\\*+", "^ *\\s*\\n", "\\\n",
    /* Common preprocessor macros, not significant by themselves. */
    "[:<:]ASSERT[:>:]", 
    "[:<:]FALSE[:>:]", 
    "[:<:]NULL[:>:]",
    "[:<:]STATIC[:>:]", 
    "[:<:]TRUE[:>:]",
    /* Macro include lines are noise, too. */
    "\\s*#include.*","#\\s*line.*",
    /* Common error macros. */
    "[:<:]EFAULT[:>:]",
    "[:<:]EINVAL[:>:]",
    "[:<:]ENOSYS[:>:]",
};
static regex_t c_regexps[sizeof(c_patterns)/sizeof(*c_patterns)];

static char c_punct[] = {
    /* Punctuation and whitespace */
    '{', '}', '(', ')', '<', '>', '[', ']',
    '^', '&', '|', '*', '?', '.', '+', 
    ';', ':', '%', ',', '-', '/', '=', '!', 
    ' ', '\n', '\t', '\v',
};

static char *shell_patterns[] = {
    /* Idioms that don't convey any meaning in isolation */
    "exit *[01];?",
    /* Bare shell keywords */
    "[:<:]break[:>:]", "[:<:]case[:>:]", "[:<:]done[:>:]", 
    "[:<:]do[:>:]", "[:<:]else[:>:]", "[:<:]esac[:>:]",
    "[:<:]fi[:>:]", "[:<:]if[:>:]", "[:<:]return[:>:]",
    "[:<:]shift[:>:]", "[:<:]true[:>:]", "[:<:]while[:>:]", 
    /* Blank comment */
    "^#\n",
};
static regex_t shell_regexps[sizeof(shell_patterns)/sizeof(*shell_patterns)];

static char shell_punct[] = {
    /* Punctuation and whitespace */
    ':', ';', '\n','\t', '\v',
};

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

static filtertype	active = none;
static char *punct, **patterns;

void filter_set(filtertype newfilter)
{
    active = newfilter;

    if (newfilter == c)
    {
	patterns = c_patterns;
	punct = c_punct;
    }
    else
    {
	patterns = shell_patterns;
	punct = shell_punct;
    }
}

int filter_pass(const char *line)
{
    if (active == none)
	return(1);
    else
    {
	char	buf[2][BUFSIZ];
	int	changed , i, flip = 0;

	buf[0][0] = ' ';
	strncpy(buf[0]+1, line, BUFSIZ-2);

	do {
	    char *s, *t, *sp, *tp;

	    s = buf[flip]; t = buf[!flip]; flip ^= 1; 
	    changed = 0;

	    /* remove all punctuation */
	    for (sp = s, tp = t; *sp; sp++)
		if (strchr(punct, *sp))
		    changed++;
	        else
		    *tp++ = *sp;
	    *tp = '\0';

	    /* replace all regexps with the empty string */

	    if (t[0] == '\0')
		return(0);
	} while 
	    (changed);
	return(1);
    }
}

int main(int argc, char *argv[])
{
    char	buf[BUFSIZ];

    filter_init();
    filter_set(c);
    while(fgets(buf, BUFSIZ, stdin))
    {
	if (filter_pass(buf))
	    fputs("yes: ", stdout);
	else
	    fputs("no: ", stdout);
	fputs(buf, stdout);
    }
}

/* filter.c ends here */
