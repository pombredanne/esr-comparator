import os, sys, re, exceptions

class ComparatorException(exceptions.Exception):
    def __init__(self, message, retval=1):
        self.message = message
        self.retval = retval

class CommonReport:
    "Capture the state of a common-segment report."
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
        self.filtering = None
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
            elif tag == "Matches":
                self.matches = int(value)
            elif tag == "Filtering":
                self.filtering = value
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
                self.trees[tree][property.strip()] = int(value)
        self.cliques = []
        # Reading the clique data is slow, so we only do it
        # on demand.

    def read_matches(self):
        # Now read the common-segment stuff
        if self.cliques:
            return
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

    def extract_text(self, clique):
        "Return text corresponding to the given clique."
        self.read_matches()
        text = None
        if self.dir:
            olddir = os.getcwd()
            os.chdir(self.dir)
        for (file, start, end) in clique:
            try:
                # Skip to the relevant chunk
                rfp = open(file)
                for i in range(start-1):
                    line = rfp.readline()
                text = ""
                for i in range(start, end+1):
                    nextline = rfp.readline()
                    if nextline[0] == '%':
                        nextline = '%' + nextline
                    text += nextline
                rfp.close()
                break
            except IOError:
                raise ComparatorException("no such file as %s" % file)
        if self.dir:
            os.chdir(olddir)
        return (file, start, end, text)

    def cliquefilter(self, predicate):
        self.read_matches()
        filtered = []
        for clique in self.cliques:
            for (file, start, end) in clique:
                if predicate(file, start, end):
                    filtered.append(clique)
                    break
            else:
                properties = self.trees[file.split("/")[0]]
                properties['matches'] -= 1
                properties['matchlines'] -= end - start + 1
        self.cliques = filtered

    def filter_by_size(self, minsize):
        "Throw out all common segments below a specified size."
        self.cliquefilter(lambda file, start, end: end - start + 1 >= minsize)

    def filter_by_filename(self, regexp):
        "Filter by name of involved file."
        self.cliquefilter(lambda file, start, end: regexp.search(file))

    def metadump(self, fp=sys.stdout, divider="%%n", fmt=None):
        "Dump the header in a canonical format."
        fp.write("Filter-Program: filterator 1.0\n")
        fp.write("Hash-Method: RXOR\n")
        if self.merge_program:
            fp.write("Merge-Program: %s\n" % self.merge_program)
        fp.write("Normalization: %s\n" % self.normalization)
        fp.write("Shred-Size: %d\n" %  self.shredsize)
        fp.write("Filtering: %s\n" %  self.filtering)
        #if minsize:
        #    fp.write("Minimum-Size: %d\n" % minsize)
        fp.write(divider)
        for tree in self.trees:
            rep = tree + ":"
            for (key, value) in self.trees[tree].items():
                rep += " %s=%s," % (key, value)
            fp.write(rep[:-1] + "\n")
        fp.write(divider)
        if fmt:
            for clique in self.cliques:
                for (file, start, end) in clique:
                    fp.write(fmt(file, start, end, self.files[file]))
                fp.write(divider)

    def dump(self, fp):
        "Dump this in the same format as the input."
        fp.write("#SCF-B\n")
        self.metadump(fp=fp, divider="%%\n",
                      fmt=lambda f,s,e, z: "%s:%d:%d:%d\n" % (f,s,e,z))

# Property flags
C_CODE		= 0x01
SHELL_CODE	= 0x02
SIGNIFICANT	= 0x40

# End
