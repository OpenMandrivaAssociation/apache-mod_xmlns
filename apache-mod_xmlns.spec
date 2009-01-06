#Module-Specific definitions
%define mod_name mod_xmlns
%define mod_conf A67_%{mod_name}.conf
%define mod_so %{mod_name}.so

Summary:	Adds XML Namespace processing to the Apache Webserver
Name:		apache-%{mod_name}
Version:	0.97
Release:	%mkrel 10
Group:		System/Servers
License:	GPL
URL:		http://apache.webthing.com/mod_xmlns/
# there is no official tar ball
# http://apache.webthing.com/svn/apache/filters/xmlns/
Source0:	http://apache.webthing.com/svn/apache/filters/xmlns/mod_xmlns.c
Source1:	http://apache.webthing.com/svn/apache/filters/xmlns/xmlns.h
Source2:	README.mod_xmlns
Source3:	%{mod_conf}
Requires(pre): rpm-helper
Requires(postun): rpm-helper
Requires(pre):	apache-conf >= 2.2.0
Requires(pre):	apache >= 2.2.0
Requires:	apache-conf >= 2.2.0
Requires:	apache >= 2.2.0
BuildRequires:	apache-devel >= 2.2.0
BuildRequires:	file
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
mod_xmlns adds XML Namespace support to Apache, and may form the basis of
XML-driven publishing systems. It runs as an output filter, so it works
automatically with any content generator.

%package	devel
Summary:	Development API for the mod_xmlns apache module
Group:		Development/C

%description	devel
mod_xmlns adds XML Namespace support to Apache, and may form the basis of
XML-driven publishing systems. It runs as an output filter, so it works
automatically with any content generator.

This package contains the development API for the mod_xmlns apache module.

%prep

%setup -q -c -T -n %{mod_name}-%{version}

cp %{SOURCE0} mod_xmlns.c
cp %{SOURCE1} xmlns.h
cp %{SOURCE2} README
cp %{SOURCE3} %{mod_conf}

# strip away annoying ^M
find . -type f|xargs file|grep 'CRLF'|cut -d: -f1|xargs perl -p -i -e 's/\r//'
find . -type f|xargs file|grep 'text'|cut -d: -f1|xargs perl -p -i -e 's/\r//'

%build
%{_sbindir}/apxs -c %{mod_name}.c

%install
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

install -d %{buildroot}%{_sysconfdir}/httpd/modules.d
install -d %{buildroot}%{_libdir}/apache-extramodules
install -d %{buildroot}%{_includedir}

install -m0755 .libs/*.so %{buildroot}%{_libdir}/apache-extramodules/
install -m0644 xmlns.h %{buildroot}%{_includedir}/
install -m0644 %{mod_conf} %{buildroot}%{_sysconfdir}/httpd/modules.d/%{mod_conf}

%post
if [ -f %{_var}/lock/subsys/httpd ]; then
  %{_initrddir}/httpd restart 1>&2;
fi

%postun
if [ "$1" = "0" ]; then
  if [ -f %{_var}/lock/subsys/httpd ]; then
	%{_initrddir}/httpd restart 1>&2
  fi
fi

%clean
[ "%{buildroot}" != "/" ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc README
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/httpd/modules.d/%{mod_conf}
%attr(0755,root,root) %{_libdir}/apache-extramodules/%{mod_so}

%files devel
%defattr(-,root,root)
%attr(0644,root,root) %{_includedir}/xmlns.h
