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
        self.matches = None
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
        self.trees = []
        while 1:
            line = self.fp.readline()
            if not line or line == '%%\n':
                break
            (tree, properties) = line.split(":")
            propdict = {}
            for assignment in properties.split(", "):
                (property, value) = assignment.split("=")
                propdict[property.strip()] = int(value)
            self.trees.append([tree, propdict])
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
        if self.matches and len(self.cliques) != self.matches:
            raise ComparatorException("Matches field not equal to clique count.")

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
                for (treename, properties) in self.trees:
                    if treename == file.split("/")[0]:
                        break
                properties['matches'] -= 1
                properties['matchlines'] -= end - start + 1
        self.cliques = filtered
        self.matches = len(filtered)

    def metadump(self, fp=sys.stdout, divider="%%\n", fmt=None, moredict={}):
        "Dump the header in a canonical format."
        fp.write("Filtering: %s\n" %  self.filtering)
        fp.write("Filter-Program: filterator 1.0\n")
        fp.write("Hash-Method: RXOR\n")
        fp.write("Matches: %d\n" %  self.matches)
        if self.merge_program:
            fp.write("Merge-Program: %s\n" % self.merge_program)
        fp.write("Normalization: %s\n" % self.normalization)
        fp.write("Shred-Size: %d\n" %  self.shredsize)
        for (key, value) in moredict.items():
            fp.write("%s: %d\n" %  (key, value))
        fp.write(divider)
        for (treename, properties) in self.trees:
            rep = treename + ":"
            keys = properties.keys()
            keys.sort()
            for key in keys:
                rep += " %s=%s," % (key, properties[key])
            fp.write(rep[:-1] + "\n")
        fp.write(divider)
        if fmt:
            for clique in self.cliques:
                fp.write(fmt(clique))

    def preen(self):
        "Fix the file properties of this report."
        matches = {}
        matchlines = {}
        for (treename, dummy) in self.trees:
            matches[treename] = matchlines[treename] = 0
        for clique in self.cliques:
            intree = {}
            for (treename, dummy) in self.trees:
                intree[treename] = False
            for (file, start, end) in clique:
                for (treename, properties) in self.trees:
                    if treename == file.split("/")[0]:
                        break
                if not intree.get(treename, False):
                    matches[treename] += 1
                    intree[treename] = True
                matchlines[treename] += end - start + 1
        for (treename, properties) in self.trees:
            properties['matches'] = matches[treename]
            properties['matchlines'] = matchlines[treename]
        self.matches = len(self.cliques)

    def cliquedump(self, clique):
        "Dump a clique in a form identical to the input one."
        rep = ""
        for (file, start, end) in clique:
            rep += "%s:%d:%d:%d\n" % (file, start, end, self.files[file])
        rep += "%%\n"
        return rep

    def dump(self, fp):
        "Dump in SCF-B format."
        self.preen()
        fp.write("#SCF-B 2.0\n")
        self.metadump(fp=fp, fmt=self.cliquedump)

# Property flags
C_CODE		= 0x01
SHELL_CODE	= 0x02
SIGNIFICANT	= 0x40

# End
