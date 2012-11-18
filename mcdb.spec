%define name mcdb
%define version 0.06

Name:    %{name}
Version: %{version}
Release: 1%{?dist}
Summary: mcdb - fast, reliable, simple code to create, read constant databases
Group:   Databases

License:	LGPLv2+
Vendor:		Glue Logic LLC
URL:		https://github.com/gstrauss/mcdb/
Source0:	mcdb-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	gcc
Requires:	glibc

%package libs
Summary: mcdb - shared libraries
Group:   System Environment/Libraries


%description
==
mcdb (mmap constant database) is originally based on the cdb package, a:
"fast, reliable, simple package for creating and reading constant databases."
mcdb is almost 33% faster, provides support for use in threaded programs, and
supports databases larger than 4 GB.

nss_mcdb is an nss implementation of standard authentication and netdb files
built as mcdb databases, which are faster than using nscd.  

https://github.com/gstrauss/mcdb/ provides the latest information about mcdb.
http://cr.yp.to/cdb.html provides information about cdb, on which mcdb is based.
==

%description libs
mcdb - fast, reliable, simple code to create, read constant databases
This package contains mcdb shared libraries.


%prep
%setup -q


%build
make %{?_smp_mflags} PREFIX=


%install
rm -rf $RPM_BUILD_ROOT
make install PREFIX=$RPM_BUILD_ROOT PREFIX_USR=$RPM_BUILD_ROOT/usr
make install-doc PREFIX=$RPM_BUILD_ROOT PREFIX_USR=$RPM_BUILD_ROOT/usr
make install-headers PREFIX=$RPM_BUILD_ROOT PREFIX_USR=$RPM_BUILD_ROOT/usr
mv $RPM_BUILD_ROOT/usr/share/doc/mcdb \
   $RPM_BUILD_ROOT/usr/share/doc/mcdb-%{version}
# permissions restored in files section below, after 'strip' is possibly run
chmod u+w $RPM_BUILD_ROOT/sbin/* $RPM_BUILD_ROOT/usr/bin/*
chmod u+w $RPM_BUILD_ROOT/%{_lib}/* $RPM_BUILD_ROOT/usr/%{_lib}/*


%clean
rm -rf $RPM_BUILD_ROOT


%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%config(noreplace)      /etc/mcdb
%attr(0555,root,root)   /sbin/*
%attr(0555,root,root)   /usr/bin/*
                        /usr/include/mcdb
%doc                    /usr/share/doc/mcdb-%{version}

%files libs
%defattr(-,root,root,-)
%attr(0555,root,root)   /%{_lib}/libnss_mcdb*
                        %{_libdir}/libnss_mcdb*
%attr(0555,root,root)   %{_libdir}/libmcdb*

