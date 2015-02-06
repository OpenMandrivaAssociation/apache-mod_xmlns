#Module-Specific definitions
%define mod_name mod_xmlns
%define mod_conf A67_%{mod_name}.conf
%define mod_so %{mod_name}.so

Summary:	Adds XML Namespace processing to the Apache Webserver
Name:		apache-%{mod_name}
Version:	0.97
Release:	16
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
%{_bindir}/apxs -c %{mod_name}.c

%install

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

%files
%doc README
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/httpd/modules.d/%{mod_conf}
%attr(0755,root,root) %{_libdir}/apache-extramodules/%{mod_so}

%files devel
%attr(0644,root,root) %{_includedir}/xmlns.h


%changelog
* Sat Feb 11 2012 Oden Eriksson <oeriksson@mandriva.com> 0.97-15mdv2012.0
+ Revision: 773242
- rebuild

* Tue May 24 2011 Oden Eriksson <oeriksson@mandriva.com> 0.97-14
+ Revision: 678447
- mass rebuild

* Sun Oct 24 2010 Oden Eriksson <oeriksson@mandriva.com> 0.97-13mdv2011.0
+ Revision: 588102
- rebuild

* Mon Mar 08 2010 Oden Eriksson <oeriksson@mandriva.com> 0.97-12mdv2010.1
+ Revision: 516281
- rebuilt for apache-2.2.15

* Sat Aug 01 2009 Oden Eriksson <oeriksson@mandriva.com> 0.97-11mdv2010.0
+ Revision: 406688
- rebuild

* Tue Jan 06 2009 Oden Eriksson <oeriksson@mandriva.com> 0.97-10mdv2009.1
+ Revision: 326279
- rebuild

* Mon Jul 14 2008 Oden Eriksson <oeriksson@mandriva.com> 0.97-9mdv2009.0
+ Revision: 235135
- rebuild

* Thu Jun 05 2008 Oden Eriksson <oeriksson@mandriva.com> 0.97-8mdv2009.0
+ Revision: 215678
- fix rebuild

* Fri Mar 07 2008 Oden Eriksson <oeriksson@mandriva.com> 0.97-7mdv2008.1
+ Revision: 181988
- rebuild

  + Olivier Blin <blino@mandriva.org>
    - restore BuildRoot

  + Thierry Vignaud <tv@mandriva.org>
    - kill re-definition of %%buildroot on Pixel's request

* Mon Oct 01 2007 Oden Eriksson <oeriksson@mandriva.com> 0.97-6mdv2008.1
+ Revision: 94139
- rebuilt due to missing packages

* Mon Oct 01 2007 Oden Eriksson <oeriksson@mandriva.com> 0.97-5mdv2008.0
+ Revision: 94135
- bunzip the sources

* Sat Sep 08 2007 Oden Eriksson <oeriksson@mandriva.com> 0.97-4mdv2008.0
+ Revision: 82707
- rebuild


* Sat Mar 10 2007 Oden Eriksson <oeriksson@mandriva.com> 0.97-3mdv2007.1
+ Revision: 140780
- rebuild

* Thu Nov 09 2006 Oden Eriksson <oeriksson@mandriva.com> 0.97-2mdv2007.0
+ Revision: 79562
- Import apache-mod_xmlns

* Tue Jul 18 2006 Oden Eriksson <oeriksson@mandriva.com> 0.97-2mdv2007.0
- fix deps

* Tue Jul 18 2006 Oden Eriksson <oeriksson@mandriva.com> 0.97-1mdv2007.0
- initial Mandriva package

