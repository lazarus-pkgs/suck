%define version 4.3.5
%define name suck
%define release 1

Name: %{name}
Summary: suck - download news from remote NNTP server
Version: %{version}
Release: %{release}
Source: http://www.sucknews.org/%{name}-%{version}.tar.gz
Packager: Robert Yetman <bobyetman@sucknews.org>
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Copyright: Unknown
Group: News

%description
This package contains software for copying news from an NNTP server to your
local machine, and copying replies back up to an NNTP server.  It works
with most standard NNTP servers, including INN, CNEWS, DNEWS, and typhoon.

%prep
if [ "${RPM_BUILD_ROOT}" != '/' ] ; then rm -rf ${RPM_BUILD_ROOT} ; fi

%setup
./configure --prefix=${RPM_BUILD_ROOT}/usr --mandir=${RPM_BUILD_ROOT}%{_mandir}

%build
make

%install
make \
    install

%clean
if [ "${RPM_BUILD_ROOT}" != '/' ] ; then rm -rf ${RPM_BUILD_ROOT} ; fi

%post
echo Please look in %{_docdir}/%{name}-%{version} for example scripts

%files

%defattr(0644,root,root,0755)
%doc README CHANGELOG sample

%defattr(-,root,root,0755)
%{_bindir}/suck
%{_bindir}/rpost
%{_bindir}/testhost
%{_bindir}/lmove

%{_mandir}/man1/suck.1*
%{_mandir}/man1/rpost.1*
%{_mandir}/man1/testhost.1*
%{_mandir}/man1/lmove.1*
