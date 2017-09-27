Summary: suck - download news from remote NNTP server
Name: suck
Version: 4.2.5
Release: 1
Source: http://home.att.net/~bobyetman/suck-4.2.5.tar.gz
Packager: Robert Yetman <bobyetman@att.net>
BuildRoot: /tmp/suck-%{PACKAGE_VERSION}-%{PACKAGE_RELEASE}
Copyright: Unknown
Group: News

%description
This package contains software for copying news from an NNTP server to your
local machine, and copying replies back up to an NNTP server.  It works
with most standard NNTP servers, including INN, CNEWS, DNEWS, and typhoon.

%prep
%setup
./configure --prefix=${RPM_BUILD_ROOT}/usr

%build
make

%install
make \
    install

%clean
if [ "${RPM_BUILD_ROOT}" != '/' ] ; then rm -rf ${RPM_BUILD_ROOT} ; fi

%files

%doc README CHANGELOG

%attr(- root root) /usr/bin/suck
%attr(- root root) /usr/bin/rpost
%attr(- root root) /usr/bin/testhost
%attr(- root root) /usr/bin/lmove

%attr(- root root) /usr/man/man1/suck.1
%attr(- root root) /usr/man/man1/rpost.1
%attr(- root root) /usr/man/man1/testhost.1
%attr(- root root) /usr/man/man1/lmove.1
