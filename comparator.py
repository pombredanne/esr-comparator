import os, sys, re, exceptions

# Tokens to be removed when checking whether a shred is significant
junk = {
    "C": map(re.compile, (
        # Idioms that don't convey any meaning in isolation
        r"return *\(?[a-z]+\)? *;", r"return *\(?-?[01]+\)? *;",
        r"goto +[a-z]+;", r"exit *\([01]\);",
        # Pragmas
        r'/\* *ARGSUSED *\*/',
        r'/\* *NOTREACHED *\*/', 
        r'/\* *FALL *THRO?UG?H? *\*/',
        # Bare C keywords
        r'\bbreak\b',  r'\bcase\b',r'\bcontinue\b', r'\bdefault\b',
        r'\bdo\b', r'\belse\b', r'\benum\b', r'\bif\b', r'\bgoto\b',
        r'\breturn\b', r'\bswitch\b', r'\bwhile\b',
        r'enum', r'\bint\b', r'\blong\b', r'\bshort\b', r'\bstatic\b',
        r'\bstruct\b', r'\btypedef\b', r'\bunion\b', r'\bvoid\b',
        r'# *define',r'# *endif',r'# *else',r'# *if\b',
        r'# *ifdef\b',r'# *ifndef\b',
        # Comment delimiters with no content
        r'/\*+', r'\*+/', r'\*+', r'^ *\\s*\n',
        # Common preprocessor macros, not significant by themselves.
        r'\bASSERT\b', r'\bFALSE\b', r'\bNULL\b', r'\bSTATIC\b', r'\bTRUE\b',
        # Macro include lines are noise, too.
        r'\s*#include.*',r'#\s*line.*',
        # Common error macros.
        r'\bEFAULT\b', r'\bEINVAL\b', r'\bENOSYS\b',
        # Punctuation and whitespace
        r'\{', r'\}', r'\(', r'\)', r'\<', r'\>', r'\[', r'\]',
        r'\^', '&', r'\|', r'\*', r'\?', r'\.', r'\+', 
        '\\\n', ';', ':', '%', ',', '-', '/', '=', '!', '\n','\t', '\v'
        )),
    "shell": map(re.compile, (
        # Idioms that don't convey any meaning in isolation
        r"exit *[01];?",
        # Bare shell keywords
        r"\bbreak\b", r"\bcase\b", r"\bdone\b", r"\bdo\b", r"\belse\b",
        r"\besac\b", r"\bfi\b", r"\bif\b", r"\breturn\b", r"\bshift\b",
        r"\btrue\b", r"\bwhile\b", 
        # Blank comment
        "^#\n",
        # Punctuation and whitespace
        ':', ';', '\n','\t', '\v'
        )),
    }

def nontrivial(filetype, text, debug=None):
    "Identify a shred as trivial or nontrivial."
    if filetype:
        if debug:
            print "New shred, type %s:" % `filetype`
            sys.stdout.write(text)
        # Basic theory of this function is that if we throw out all C
        # syntax and and common constants, and there is still an
        # identifier, we're looking at something that might be
        # interesting.
        text = ' ' + text
        while 1:
            savecopy = text
            for regexp in junk[filetype]:
                text = regexp.sub(' ', text)
            if savecopy == text:
                break
            else:
                if debug:
                    print "Reduced:", `text`
                continue
    return text.strip()

class ComparatorException(exceptions.Exception):
    def __init__(self, message, retval=1):
        self.message = message
        self.retval = retval

class CommonReport:
    "Capture the state of common-segnent report."
    # Template for parsing the output from comparator.
    shredline = re.compile("(.*):([0-9]+):([0-9]+):([0-9]+)")

    def __init__(self, fp, dir):
        "Read and parse an SCF-B report."
        # Handle either file objects or strings
        self.name = None
        if type(fp) == type(""):
            self.name = fp
            self.fp = open(self.name)
        else:
            self.fp = fp
        # Read the SCF header
        self.dir = dir
        self.hash_method = "RXOR"
        self.merge_program = None
        id = self.fp.readline()
        if not id.startswith("#SCF-B "):
            raise ComparatorException("input is not a SCF-B file.\n")
        while 1:
            line = self.fp.readline()
            if not line or line == '%%\n':
                break
            (tag, value) = line.split(":")
            value = value.strip()
            if tag == "Normalization":
                self.normalization = value
            elif tag == "Shred-Size":
                self.shredsize = int(value)
            elif tag == "Merge-Program":
                self.merge_program = value
            elif tag == "Hash-Method":
                self.hash_method = value
        # Read file properties
        self.trees = {}
        while 1:
            line = self.fp.readline()
            if not line or line == '%%\n':
                break
            (tree, properties) = line.split(":")
            self.trees[tree] = {}
            for assignment in properties.split(", "):
                (property, value) = assignment.split("=")
                self.trees[tree][property] = int(value)
        # Now read the common-segment stuff
        self.cliques = []
        self.files = {}
        locations = []
        while 1:
            line = self.fp.readline()
            if not line:
                break
            m = CommonReport.shredline.search(line)
            if m:
                (file, start, end, length) = \
                       (m.group(1), int(m.group(2)),
                        int(m.group(3)), int(m.group(4)))
                locations.append((file, start, end))
                self.files[file] = length
            if line == '%%\n':
                self.cliques.append(locations)
                locations = []
        # We're done, clean up
        if self.name:
            self.fp.close()

    def segment_count(self):
        "Number of common chunks in the report."
        return len(self.cliques)

    def line_count(self, tree):
        "Number of common lines from a specified tree."
        lines = 0
        for clique in self.cliques:
            for (file, start, end) in clique:
                # We find only the first match in the clique that
                # is part of the specified tree.  There could be others,
                # and if whitespace removal is in effect they could
                # have different names.
                if file.startswith(tree):
                    lines += end - start + 1
                    break
        return lines

    def extract_text(self, clique):
        "Return text corresponding to the given clique."
        text = text_type = None
        if self.dir:
            olddir = os.getcwd()
            os.chdir(self.dir)
        for (file, start, end) in clique:
            try:
                # Try to deduce the text_type
                text_type = None
                if file.endswith(".c") or file.endswith(".h"):
                    text_type = "C"
                # Skip to the relevant chunk
                rfp = open(file)
                for i in range(start-1):
                    line = rfp.readline()
                    if not text_type and i == 0:
                        if "sh" in line:
                            text_type = "shell"
                text = ""
                for i in range(start, end+1):
                    nextline = rfp.readline()
                    if not text_type and i == 0 and start == 1:
                        if "sh" in line:
                            text_type = "shell"
                    if nextline[0] == '%':
                        nextline = '%' + nextline
                    text += nextline
                rfp.close()
                break
            except IOError:
                pass
        if self.dir:
            os.chdir(olddir)
        if not text:
            print >>sys.stderr, "Failed to extract text from", clique
        return (text_type, text)

    def filter_by_size(self, minsize):
        "Throw out all common segments below a specified size."
        filtered = []
        for clique in self.cliques:
            for (file, start, end) in clique:
                if end - start + 1 >= minsize:
                    filtered.append(clique)
                    break
        self.cliques = filtered

    def filter_by_significance(self):
        "Throw out all cliques that aren't significant."
        filtered = []
        for clique in self.cliques:
            (type, text) = self.extract_text(clique)
            if nontrivial(type, text):
                filtered.append(clique)
        self.cliques = filtered

    def statistics(self):
        "Return a statistical summary object for the current report."
        return CommonStatistics(self)

class CommonStatistics:
    "Statistical information from a report."
    def __init__(self, report):
        self.chunks = report.segment_count()
        self.trees = []
        self.normalization = report.normalization
        for (tree, chunks, lines, totallines) in report.trees.items():
            lines = report.line_count(tree)
            self.trees.append((tree, lines, totallines))

    def __str__(self):
        rep = "Trees: " + " ".join(map(lambda x: x[0], self.trees))
        rep = "%d overlaps with %s normalization\n" % (self.chunks, self.normalization)
        for tree in self.trees:
            lines = tree['lines']
            total = tree['total']
            rep += "%s:\t%d of %d (%02.2f%%)\n" % (tree, lines, total, (lines * 100.0)/total)
        return rep
