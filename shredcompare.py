#!/usr/bin/env python
#
# shredcompare -- find similarities in file trees.
#
# 0.1 by Eric S. Raymond, 24 Aug 2003.
#
# Implements the shred algorithm described at:
#
#	http://theinquirer.net/?article=10061
#
# In this implementation, blank lines are ignored.
#
# The -h option supports building a cache containing the shred list
# for a tree you specify.  This is strictly a speed hack.  If your
# tree is named `foo', the file will be named `foo.hash'.  On your
# next comparison run it will be picked up automatically. It is
# your responsibility to make sure each hash file is newer than
# the corresponding tree.
#
# This is alpha software. There is a bug in the implementation,
# somewhere in the logic for merging matching ranges before report
# generation, that very occasionally garbages the last number in the
# target-tree range.  I haven't fixed it yet because I haven't found a
# way to produce a test load that reproduces it that is smaller that
# the entire Linux 2.6.0 and Sysem V kernels.

import sys, os, os.path, re, md5, getopt, time, cPickle

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
    # Make these sortable (not presently used)
    def __cmp__(self, other):
        return cmp((self.file, self.start, self.end), 
                   (other.file, other.start, other.end))
    def __hash_(self):
        return hash((self.file, self.start, self.end))
    # External representation
    def __str__(self):
        # 0-origin line numbers internally, 1-origin externally.
        return "%s:%d-%d" % (self.file, self.start+1, self.end)
    __repr__ = __str__

class ShredTree:
    "Represent a collection of shreds corresponding to a file tree."
    ws = re.compile(r"\s+")

    def __init__(self, path, ignorehash=False, shredsize=5, verbose=False):
        self.path = path
        self.shredsize = shredsize
        self.verbose = verbose
        self.shreds = {}
        if ignorehash:
            self.__shredtree()
        else:
            self.__read_hash()
            if not self.shreds:
                self.__shredtree()

    # Dictionary emulation, meant to be called from outside
    def __getitem__(self, key):
        return self.shreds[key]
    def __setitem__(self, key, value):
        self.shreds[key] = value
    def __delitem__(self, key):
        del self.shreds[key]
    def __contains__(self, item):
        return item in self.shreds
    def update(self, dict):
        self.shreds.update(dict)
    def keys(self):
        return self.shreds.keys()

    # Internal methods
    def __shredtree(self):
        "Shred a file tree; srt up dictionary of checksums-to-locations."
        self.shreds = {}
        if self.verbose:
            print "% Shredding", self.path
        # No precomputed hashes.  Prepare a news shred list.
        for root, dirs, files in os.walk(self.path):
            for file in files:
                fullpath = os.path.join(root, file)
                if self.__eligible(fullpath):
                    self.__shredfile(fullpath)
    def __smash_whitespace(self, line):
        "Replace each string of whitespace in a line with a single space."
        return ShredTree.ws.sub(' ', line)
    def __is_line_relevant(self, line):
        "Don't start chunks on lines with no features.  This is a speed hack."
        return line.strip()
    def __shredfile(self, file):
        "Build a dictionary of shred tuples corresponding to a specified file."
        if self.verbose:
            print file
        fp = open(file)
        lines = map(self.__smash_whitespace, fp.readlines())
        fp.close()
        for i in range(len(lines)):
            if self.__is_line_relevant(lines[i]):
                m = md5.new()
                need = self.shredsize
                j = 0
                try:
                    while need > 0:
                        if i + j >= len(lines):
                            raise EOFError
                        if self.__is_line_relevant(lines[i+j]):
                            m.update(lines[i+j])
                            need -= 1
                        j += 1
                    # Merging shreds into a dict automatically suppresses dups.
                    self.shreds[m.digest()] = Shred(file, i, i + j)
                except EOFError:
                    pass
    def __eligible(self, file):
        "Is a file eligible for comparison?"
        return filter(lambda x: file.endswith(x), ('.c','h','.y','.l','.txt'))
    def __read_hash(self):
        "Try to read a hash digest corresponding to this tree."
        if os.path.exists(self.path + ".hash"):
            # There's a precomputed shred list.
            rfp = open(self.path + ".hash")
            shreds = {}
            while True:
                line = rfp.readline()
                if not line:
                    break
                digest = line[:16]
                (file, start, end) = line[16:].split(":")
                self.shreds[digest] = Shred(file, int(start), int(end))
            rfp.close()

    # The only non-dictionary public method
    def write_hash(self):
        "Write a hash digest corresponding to a tree."
        wfp = open(self.path + ".hash", "w")
        for key in self.shreds:
            shred = self.shreds[key]
            wfp.write(key+"%s:%d:%d"%(shred.file,shred.start,shred.end)+"\n")
        wfp.close()

def shredcompare(tree1, tree2, shredsize, verbose):
    "Compare two trees.  Returns a list of matching shred pairs"
    shreds1 = ShredTree(tree1, shredsize=shredsize, verbose=verbose)
    shreds2 = ShredTree(tree2, shredsize=shredsize, verbose=verbose)
    # Nuke everything but checksum matches between the trees
    for key in shreds1.keys():
        if not key in shreds2:
            del shreds1[key]
    for key in shreds2.keys():
        if not key in shreds1:
            del shreds2[key]
    # Merge the match lists
    matches = []
    for key in shreds1.keys():
        matches.append((shreds1[key], shreds2[key]))
    # Merge overlapping ranges if possible.
    # This is O(n**2) in the number of matching chunks.
    # Should performance ever become a problem, break this up into
    # separate passes over the chunks in each file. (Would correspond
    # to eliminating a bunch of off-diagonal entries.)
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
                    print "alpha and beta don't intersect"
                    continue
                # Merge lower chunk into upper; delete lower
                #print "Found a merge:", matches[j], matches[i]
                matches[j][0].merge(matches[i][0])
                matches[j][1].merge(matches[i][1])
                matches[i] = None
                retry = True
    return filter(lambda x: x, matches)

if __name__ == '__main__':
    mark_time = None

    def report_time(legend=None):
        "Report time since start_time was set."
        global mark_time
        endtime = time.time()
        if mark_time:
            elapsed = endtime - mark_time 
            hours = elapsed/3600; elapsed %= 3600
            minutes = elapsed/60; elapsed %= 60
            seconds = elapsed
            print "%%%%#%%%% %s: %dh, %dm, %ds" % \
            	(legend, hours, minutes, seconds)
        mark_time = endtime

    try:
        (optlist, args) = getopt.getopt(sys.argv[1:], 'h:s:v')
    except getopt.GetoptError:
        sys.stderr.write("usage: shredcompare [-s shredsize] [-v] [-h] tree1 tree2\n")
        sys.exit(2)
    makehash = None
    shredsize=5
    verbose = False
    for (opt, val) in optlist:
        if opt == '-s':
            shredsize = int(val)
        elif opt == '-v':
            verbose = True
        elif opt == '-h':
            makehash = val
    report_time()
    if makehash:
        shreds = ShredTree(makehash, ignorehash=True, shredsize=shredsize, verbose=verbose)
        if verbose:
            report_time("Shred list complete.")
        shreds.write_hash()
        if verbose:
            report_time("Digesting complete.")
    else:
        matches = shredcompare(args[0], args[1], shredsize, verbose)
        if verbose:
            report_time("Match list complete.")
        for (source, target) in matches:
            print source, "->", target
            if verbose:
                sys.stdout.write(source.dump())

# End
