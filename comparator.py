import os, sys, re, exceptions

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
            elif tag == "Matches":
                self.matches = int(value)
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

    def extract_text(self, clique):
        "Return text corresponding to the given clique."
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
                pass
        if self.dir:
            os.chdir(olddir)
        return (file, start, end, text)

    def filter_by_size(self, minsize):
        "Throw out all common segments below a specified size."
        filtered = []
        for clique in self.cliques:
            for (file, start, end) in clique:
                if end - start + 1 >= minsize:
                    filtered.append(clique)
                    break
            else:
                properties = self.trees[file.split("/")[0]]
                properties['matches'] -= 1
                properties['matchlines'] -= end - start + 1
        self.cliques = filtered

# Property flags
C_CODE		= 0x01
SHELL_CODE	= 0x02
SIGNIFICANT	= 0x40

# End
