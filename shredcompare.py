#!/usr/bin/env python
#
# shredcompare -- find common cliques in shred lists
#
import getopt, sys, binascii

class SHIF:
    "SHIF-A file metadata container."
    def __init__(self, name):
        self.name = name
        self.fp = open(name)
        self.comments = []
        id = self.fp.readline()
        if not id.startswith("#SHIF-A "):
            sys.stderr.write("shredcompare: %s is not a SHIF-A file."%fp.name)
            sys.exit(1)
        while True:
            line = self.fp.readline()
            if line == '%%\n':
                break
            (tag, value) = line.split(":")
            value = value.strip()
            if tag == "Normalization":
                self.normalization = value.split(", ")
                self.normalization.sort()
            elif tag == "Shred-Size":
                self.shredsize = int(value)
            elif tag == "Hash-Method":
                self.hash = value
            elif tag == "Generator-Program":
                self.generator = value
            elif tag == "Comment":
                self.comments.append(value)
    def __check_match(self, other, attr):
        if getattr(self, attr) != getattr(other, attr):
            sys.stderr.write("shredcompare: %s of %s and %s doesn't match." \
                             % (attr, self.name, other.name))
            sys.exit(1)
    def compatible(self, other):
        self.__check_match(other, 'normalization')
        self.__check_match(other, 'shredsize')
        self.__check_match(other, 'hash')

class Shred:
    "Represent a range of lines."
    def __init__(self, file, start, end):
        self.file = file
        self.start = start
        self.end = end
    def dump(self):
        rfp = open(self.file)
        for i in range(self.start):
            rfp.readline()
        text = ""
        for i in range(self.start, self.end):
            text += rfp.readline()
        rfp.close()
        return text
    def __range(self):
        return range(self.start, self.end)
    def intersects(self, other):
        return self.file == other.file and \
           self.start in other.__range() or other.start in self.__range()
    def merge(self, other):
        self.start = min(self.start, other.start)
        self.end   = max(self.end,   other.end)
    # Make these sortable
    def __cmp__(self, other):
        return cmp((self.file, self.start, self.end), 
                   (other.file, other.start, other.end))
    def __hash_(self):
        return hash((self.file, self.start, self.end))
    # External representation
    def __str__(self):
        # 0-origin line numbers internally, 1-origin externally.
        return "%s:%d-%d" % (self.file, self.start, self.end)
    __repr__ = __str__

def merge_hashes(fp, dict):
    "Read and merge hashes corresponding to one file."
    file = fp.readline().rstrip()
    while True:
        line = fp.readline()
        if not line:
            return False
        if not line.strip():
            return True
        (start, end, hash) = line.split()
        start = int(start)
        end = int(end)
        hash = binascii.unhexlify(hash)
        if hash not in dict:
            dict[hash] = []
        dict[hash].append((file, start, end))

if __name__ == '__main__':
    try:
        (optlist, args) = getopt.getopt(sys.argv[1:], 'd')
    except getopt.GetoptError:
        sys.stderr.write("usage: shredcompare [-h] [-d] file\n")
        sys.exit(2)
    local_duplicates = True
    for (opt, val) in optlist:
        if opt == '-d':
            local_duplicates = False
        elif opt == '-h':
	    sys.stderr.write("usage: shredcompare [-d] file\n");
	    sys.stderr.write(" -d      = discard local duplicates.\n");
	    sys.stderr.write(" -h      = help (display this message).\n");
	    sys.exit(0);

    shiflist = []
    # Read input metadata
    for file in args:
        shiflist.append(SHIF(file))
    # Metadata sanity check
    for i in range(len(args)-1):
        shiflist[i].compatible(shiflist[i+1])
    # Read in all hashes
    hashdict = {}
    for shif in shiflist:
        while merge_hashes(shif.fp, hashdict):
            continue
    # Nuke all unique hashes
    for (key, matches) in hashdict.items():
        if len(matches) == 1:
            del hashdict[key]
    # Maybe nuke all match sets that don't cross a tree boundary
    if not local_duplicates:
        pass			# FIXME
    # Turn the remaining matches into match objects, because we're
    # going to want to do logic on them.  We deferred it this long
    # to lower memory usage.
    matches = []
    for clique in hashdict.values():
        matches.append(map(lambda x: Shred(x[0], x[1], x[2]), clique))
    # Merge overlapping ranges if possible.
    # This is O(n**2) in the number of matching chunks.
    # Should performance ever become a problem, break this up into
    # separate passes over the chunks in each file. (Would correspond
    # to eliminating a bunch of off-diagonal entries.)
    # FIXME: code currently only does two-way merge.
    retry = True
    while retry:
        retry = False
        #print "Matches at start:", matches
        for i in range(len(matches)):
            for j in range(len(matches)):
                if i >= j:
                    continue
                # Neither shred must have been deleted
                if not matches[j] or not matches[i]:
                    continue
                #print "Checking:", matches[j], matches[i]
                # Toss them out if they don't intersect
                if not (matches[j][0].intersects(matches[i][0]) and \
                	matches[j][1].intersects(matches[i][1])):
                    #print "alpha and beta don't intersect"
                    continue
                # Merge lower chunk into upper; delete lower
                #print "Found a merge:", matches[j], matches[i]
                matches[j][0].merge(matches[i][0])
                matches[j][1].merge(matches[i][1])
                matches[i] = None
                retry = True
    matches = filter(lambda x: x, matches)
    matches.sort(lambda x, y: cmp(x[0], y[0]))	# by source chunk
    # OK, dump all matches.
    print "#SHIF-B 1.0"
    print "Merge-Program: shredcompare.py 1.0"
    print "Hash-Method: MD5"
    print "Shred-Size: %d" % shiflist[0].shredsize
    print "Normalization:", ",".join(shiflist[0].normalization)
    print "%%"
    # FIXME: do something appropriate with comments
    for match in matches:
        for range in match:
            print `range`
        print "-"

