Name: comparator
Summary: fast comparison of large source-code trees
Version: 2.5
Release: 1
License: GPL
Group: Utilities
URL: http://www.catb.org/~esr/comparator/
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
#Keywords: source code, comparison

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
* Thu Jul  8 2004 Eric S. Raymond <esr@snark.thyrsus.com> - 2.5-1
- Alan Burlinson's tweaks to ease porting to SunOS.

* Thu Mar 11 2004 Eric S. Raymond <esr@snark.thyrsus.com> 2.4-1
- Fixed strtok() bug apparently introduced by C library change.
  Added -n, -f, and -F options to filterator.

* Mon Dec 29 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.3-1
- Source RPMs no longer depend on myversion symbol.

* Wed Dec 24 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.2-1
- Better error reporting in comparator.py.

* Sun Sep  7 2003 Eric S. Raymond <esr@snark.thyrsus.com> 
- Initial build.


