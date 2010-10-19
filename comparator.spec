Name: comparator
Summary: fast comparison of large source-code trees
Version: 2.8
Release: 1
License: BSD
Group: Utilities
URL: http://www.catb.org/~esr/comparator/
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
#Project-Tags: source code, comparison, forensics, copyright

%description
comparator and filterator are a pair of tools for rapidly finding common
code segments in large source trees.  They can be useful as tools for 
detecting copyright infringement.

%prep
%setup -q

%build
make %{?_smp_mflags} comparator comparator.1

%install
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT"%{_bindir}
mkdir -p "$RPM_BUILD_ROOT"%{_mandir}/man1/
set -- `ls -d %{_libdir}/python*/site-packages/`
while [ "$2" ]; do shift; done;
pylib=$1
mkdir -p "$RPM_BUILD_ROOT"${pylib}
cp comparator filterator "$RPM_BUILD_ROOT"%{_bindir}
cp comparator.1 "$RPM_BUILD_ROOT"%{_mandir}/man1/
cp comparator.py "$RPM_BUILD_ROOT"${pylib}

%clean
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_bindir}/comparator
%{_bindir}/filterator
%{_mandir}/man1/comparator.1*
%{_libdir}/python*/site-packages/comparator.py

%changelog
* Tue Oct 19 2005 Eric S. Raymond <esr@snark.thyrsus.com> - 2.8-1
- Change license to BSD. Tweak Python to use hashlib rather than
  the deprecated md5 module.  Suppress some compiler warnings.

* Fri Jan  7 2005 Eric S. Raymond <esr@snark.thyrsus.com> - 2.7-1
- Improvements to regression-test machinery.  
- Document the FAQ about comparing a tree to itself.

* Mon Sep 20 2004 Eric S. Raymond <esr@snark.thyrsus.com> - 2.6-1
- Ignore .svn files and and SVN subdirectories.  Typo fixes on manual page.

* Sat Aug  7 2004 Eric S. Raymond <esr@snark.thyrsus.com> - 2.5-1
- Alan Burlinson's tweaks to ease porting to SunOS. Emil Sit's 
  dramatic speedup for SCF-file reads.  Fix incorrect statistics
  generation with filterator -mfF options -- counts were too high.
  Documentation fixes.

* Thu Mar 11 2004 Eric S. Raymond <esr@snark.thyrsus.com> 2.4-1
- Fixed strtok() bug apparently introduced by C library change.
  Added -n, -f, and -F options to filterator.

* Mon Dec 29 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.3-1
- Source RPMs no longer depend on myversion symbol.
- New -N option style supports multiple normalizers.
- Fix a bug that might have caused a SEGV on really large shreds.

* Wed Dec 24 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.2-1
- Better error reporting in comparator.py.
- Fix a packaging error.

* Tue Sep 19 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.1-1
- Refactored code to make it possible to plug in new feature analyzers.
- Logic for handling trailing blank lines in files works slightly better.

* Mon Sep 18 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.0-1
- Now uses custom 8-byte digest function suggested by Ron Rivest, the inventor
  of MD5.  The new function is faster and about halves the size of SCF files.
  This format change is not compatible with 1.x versions.
- There is a new -m option to set the minimum span size to be output.
  The filterator -s option has been changed to -m to match it.
- filterator is now a wrapper around comparator.py, a loadable Python
  module which can be used to write other report generators.
- Significance filtering for C and shell is now done within comparator
  itself, and is accordingly much faster.
- FreeBSD port patches from Warren Toomey.

* Fri Sep 15 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.6-1
- Regression-test production now in place.
- Second application of qsort trick, for significant speedups on
  trees with many common matches.
- New -v option to enable progress messages; they're off by default.

* Fri Sep 12 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.5-1
- Fix bug in -r option.
- Capture trailing braces (as on C functions) in chunks even when they are
  ignored for comparison purposes.
- Range reports now include the line length of the file.
- Documentation improvements.

* Fri Sep 12 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.4-1
- Fix brown-paper-bag bug in reduction logic, introduced when I optimized it.
- SCF-B format now includes codebase size so we can compute percentages.
- filterator now generates a trailer containing percentage overlaps.

* Fri Sep 12 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.3-1
- -C now implies -w.
- A new -r option strips comments.
- Code is cleaned up to pass gcc -Wall.
- filterator can now perform significance checks on shreds from shellscripts.
- There	is now a progress meter while making SCF files.
- The logic for merging ranges has been seriously speed-tuned and is 
  66% faster.  This code is O(n--2) on the number of common shreds and
  is the bottleneck on large trees.

* Wed Sep 10 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.2-1
- Add -C option; -w wasn't good enough to make indent-style difference
  invisible by itself.

* Tue Sep 09 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.1-1
- Fix silly division by zero error. Add COPYING file.
- Add -s option to filterator and make it ignore form feeds in filtered mode.

* Sun Sep  7 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.0
- Initial build.




