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
# the entire Linux 2.6.o and Sysem V kernels.

import sys, os, os.path, re, md5, getopt, time, cPickle

shredsize = 5
verbose = False
mark_time = None
ws = re.compile(r"\s+")

class Shred:
    "Represent a range of lines."
    def __init__(self, file, start, end):
        self.file = file
        self.start = start
        self.end = end
    def fetch(self):
        rfp = open(self.file)
        for i in range(self.start):
            rfp.readline()
        text = ""
        for i in range(self.start, self.end):
            text += rfp.readline()
        rfp.close()
        return text
    def linerange(self):
        return range(self.start, self.end)
    def intersects(self, other):
        return self.file == other.file and \
           self.start in other.linerange() or other.start in self.linerange()
    def merge(self, other):
        self.start = min(self.start, other.start)
        self.end   = max(self.end,   other.end)
    def __cmp__(self, other):
        return cmp((self.file, self.start, self.end), 
                   (other.file, other.start, other.end))
    def __hash_(self):
        return hash((self.file, self.start, self.end))
    def __str__(self):
        # 0-origin line numbers internally, 1-origin externally.
        return "%s:%d-%d" % (self.file, self.start+1, self.end)
    __repr__ = __str__

def smash_whitespace(line):
    "Replace each string of whitespace in a line with a single space."
    return ws.sub(' ', line)

def is_line_relevant(line):
    "Don't start chunks on lines with no features.  This is a speed hack."
    return line.strip()

def shredfile(file, tree):
    "Build a dictionary of shred tuples corresponding to a specified file."
    if verbose:
        print file
    shreds = {}
    fp = open(file)
    lines = map(smash_whitespace, fp.readlines())
    fp.close()
    for i in range(len(lines)-shredsize):
        if is_line_relevant(lines[i]):
            m = md5.new()
            for j in range(shredsize):
                m.update(lines[i+j])
            # Merging shreds into a dict automatically suppresses duplicates.
            shreds[m.digest()] = Shred(file, i, i+shredsize)
    return shreds

def eligible(file):
    "Is a file eligible for comparison?"
    return filter(lambda x: file.endswith(x), ('.c', 'h', '.y', '.l', '.txt'))

def shredtree(tree):
    "Shred an entire file tree; return a dictionary of checksums-to-locations."
    shreds = {}
    if verbose:
        print "% Shredding", tree
    # No precomputed hashes.  Prepare a news shred list.
    for root, dirs, files in os.walk(tree):
        for file in files:
            path = os.path.join(root, file)
            if eligible(path):
                shreds.update(shredfile(path, tree))
    return shreds

def shredcompare(tree1, tree2):
    "Compare two trees.  Returns a list of matching shred pairs"
    shreds1 = read_hash(tree1)
    if not shreds1:
        shreds1 = shredtree(tree1)
    shreds2 = read_hash(tree2)
    if not shreds2:
        shreds2 = shredtree(tree2)
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

def read_hash(tree):
    "Try to read a hash digest corresponding to a tree."
    if not os.path.exists(tree + ".hash"):
        return None
    else:
        # There's a precomputed shred list.
        rfp = open(tree + ".hash")
        shreds = {}
        while True:
            line = rfp.readline()
            if not line:
                break
            digest = line[:16]
            (file, range) = line[16:].split(":")
            (start, end) = range.split("-")
            shreds[digest] = Shred(file, int(start), int(end))
        rfp.close()
        return shreds

def write_hash(shreds, tree):
    "Write a hash digest corresponding to a tree."
    wfp = open(tree + ".hash", "w")
    for key in shreds:
        wfp.write(key + str(shreds[key]) + "\n")
    wfp.close()

def report_time(legend=None):
    "Report time since start_time was set."
    global mark_time
    endtime = time.time()
    if mark_time and verbose:
        elapsed = endtime - mark_time 
        hours = elapsed/3600; elapsed %= 3600
        minutes = elapsed/60; elapsed %= 60
        seconds = elapsed
        print "%%%%#%%%% %s: %dh, %dm, %ds" % (legend, hours, minutes, seconds)
    mark_time = endtime

if __name__ == '__main__':
    try:
        (optlist, args) = getopt.getopt(sys.argv[1:], 'h:s:v')
    except getopt.GetoptError:
        sys.stderr.write("usage: shredcompare [-s shredsize] [-v] [-h] tree1 tree2\n")
        sys.exit(2)
    makehash = None
    for (opt, val) in optlist:
        if opt == '-s':
            shredsize = int(val)
        elif opt == '-v':
            verbose = True
        elif opt == '-h':
            makehash = val
    report_time()
    if makehash:
        shreds = shredtree(makehash)
        report_time("Shred list complete.")
        write_hash(shreds, makehash)
        report_time("Digesting complete")
    else:
        matches = shredcompare(args[0], args[1])
        report_time("Match list complete")
        for (source, target) in matches:
            print source, "->", target
            if verbose:
                sys.stdout.write(source.fetch())

