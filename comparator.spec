Name: comparator
Summary: fast comparison of large source-code trees
Version: 2.3
Release: 1
License: GPL
Group: Utilities
URL: http://www.catb.org/~esr/comparator/
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root

%description
comparator and filterator are a pair of tools for rapidly finding common
code segments in large source trees.  They can be useful as tools for 
detecting copyright infringement.

%prep
%setup -q

%build
make
make comparator.1

%install
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT"/usr/bin
mkdir -p "$RPM_BUILD_ROOT"/usr/share/man/man1/
pylib=`ls -d /usr/local/lib/python*/site-packages/`
mkdir -p "$RPM_BUILD_ROOT"${pylib}
cp comparator filterator "$RPM_BUILD_ROOT"/usr/bin
cp comparator.1 "$RPM_BUILD_ROOT"/usr/share/man/man1/
cp comparator.py "$RPM_BUILD_ROOT"${pylib}

%clean
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root,-)
%doc README COPYING
%{_bindir}/comparator
%{_bindir}/filterator
%{_mandir}/man1/comparator.1*
/usr/local/lib/python*/site-packages/comparator.py

%changelog
* Mon Dec 29 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.3-1
- Source RPMs no longer depend on myversion symbol.

* Wed Dec 24 2003 Eric S. Raymond <esr@snark.thyrsus.com> 2.2-1
- Better error reporting in comparator.py.

* Sun Sep  7 2003 Eric S. Raymond <esr@snark.thyrsus.com> 
- Initial build.


