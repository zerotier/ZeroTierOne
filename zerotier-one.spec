Name:           zerotier-one
Version:        1.1.6
Release:        0.1%{?dist}
Summary:        ZeroTier One network virtualization service

License:        GPLv3
URL:            https://www.zerotier.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  http-parser-devel
BuildRequires:  lz4-devel
BuildRequires:  libnatpmp-devel

%if 0%{rhel} > 7
BuildRequires:  libnatpmp-devel
BuildRequires:  systemd
%endif

%if 0%{?fedora} >= 21
BuildRequires:  systemd
BuildRequires:  json-parser-devel
%endif

Requires:       http-parser
Requires:       lz4
Requires:       libnatpmp

%if 0%{rhel} >= 7
Requires:       systemd
%endif

%if 0%{rhel} <= 6
Requires:       chkconfig
%endif

%if 0%{?fedora} >= 21
Requires:       systemd
Requires:       json-parser
%endif

Provides:       bundled(miniupnpc) = 2.0
%if 0%{?rhel}
Provides:       bundled(json-parser) = 1.1.0
%endif

%description
ZeroTier is a software defined networking layer for Earth.

It can be used for on-premise network virtualization, as a peer to peer VPN 
for mobile teams, for hybrid or multi-data-center cloud deployments, or just 
about anywhere else secure software defined virtual networking is useful.

ZeroTier One is our OS-level client service. It allows Mac, Linux, Windows, 
FreeBSD, and soon other types of clients to join ZeroTier virtual networks 
like conventional VPNs or VLANs. It can run on native systems, VMs, or 
containers (Docker, OpenVZ, etc.).

%prep
rm -rf *
ln -s %{getenv:PWD} %{name}-%{version}
tar --exclude=%{name}-%{version}/.git --exclude=%{name}-%{version}/%{name}-%{version} -czf %{_sourcedir}/%{name}-%{version}.tar.gz %{name}-%{version}/*
rm -f %{name}-%{version}
cp -a %{getenv:PWD}/* .

%build
make ZT_USE_MINIUPNPC=1 %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%if 0%{rhel} >= 7
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service
%endif
%if 0%{fedora} >= 21
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service
%endif
%if 0%{rhel} <= 6
cp ext/installfiles/linux/zerotier-one.init.rhel6 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
chmod 0755 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
%endif

%files
%{_sbindir}/*
%{_bindir}/*
%{_mandir}/*
%{_localstatedir}/*
%if 0%{rhel} >= 7
%{_unitdir}/%{name}.service
%endif
%if 0%{fedora} >= 21
%{_unitdir}/%{name}.service
%endif
%if 0%{rhel} <= 6
/etc/init.d/zerotier-one
%endif
%doc AUTHORS.md README.md
%license LICENSE.GPL-3

%post
%if 0%{rhel} >= 7
%systemd_post zerotier-one.service
%endif
%if 0%{fedora} >= 21
%systemd_post zerotier-one.service
%endif
%if 0%{rhel} <= 6
case "$1" in
  1)
    chkconfig --add zerotier-one
  ;;
  2)
    chkconfig --del newservice
    chkconfig --add newservice
  ;;
esac
%endif

%preun
%if 0%{rhel} >= 7
%systemd_preun zerotier-one.service
%endif
%if 0%{fedora} >= 21
%systemd_preun zerotier-one.service
%endif
%if 0%{rhel} <= 6
case "$1" in
  0)
    service zerotier-one stop
    chkconfig --del zerotier-one
  ;;
  1)
    # This is an upgrade.
    :
  ;;
esac
%endif

%postun
%if 0%{rhel} >= 7
%systemd_postun_with_restart zerotier-one.service
%endif
%if 0%{fedora} >= 21
%systemd_postun_with_restart zerotier-one.service
%endif

%changelog
* Sat Jun 25 2016 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.1.6-0.1
- now builds on CentOS 6 as well as newer distros, and some cleanup

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.3
- include systemd unit file

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.2
- add libnatpmp as (build)dependency

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.1
- initial package
