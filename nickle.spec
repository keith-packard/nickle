%define name nickle
%define version 1.99.1
%define release 1
%define prefix /usr

Summary: Desk calculator language, similar to C.

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Programming
Copyright: MIT

Url: http://nickle.keithp.com/

Source: http://nickle.keithp.com/nickle-%{version}.tar.bz2
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
Nickle is a desk calculator language with powerful programming and scripting
capabilities.  Nickle supports a variety of datatypes, especially arbitrary
precision integers, rationals, and imprecise reals.  The input language
vaguely resembles C.  Some things in C which do not translate easily are
different, some design choices have been made differently, and a very few
features are simply missing. 

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install
cp -R examples $RPM_BUILD_ROOT%{prefix}/share/nickle/

%files
%defattr(-,root,root)
%doc README README.name COPYING AUTHORS ChangeLog INSTALL NEWS TODO
%attr(755,root,root) %{prefix}/bin/nickle
%dir %{prefix}/share/nickle/
%{prefix}/man/man1/nickle.1*
%{prefix}/share/nickle/*

%clean
rm -rf $RPM_BUILD_ROOT
