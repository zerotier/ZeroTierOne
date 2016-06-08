Name:           zerotier-one
Version:        1.1.5
Release:        0.3%{?dist}
Summary:        ZeroTier One network virtualization service

License:        GPLv3
URL:            https://www.zerotier.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  http-parser-devel
BuildRequires:  lz4-devel
BuildRequires:  libnatpmp-devel
BuildRequires:  systemd
%if 0%{?fedora} >= 21
BuildRequires:  json-parser-devel
%endif

Requires:       http-parser
Requires:       lz4
Requires:       libnatpmp
Requires:       systemd
%if 0%{?fedora} >= 21
BuildRequires:  json-parser
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

mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service

%files
%{_sbindir}/*
%{_bindir}/*
%{_mandir}/*
%{_localstatedir}/*
%{_unitdir}/%{name}.service
%doc AUTHORS.md README.md
%license LICENSE.GPL-3

%post
%systemd_post apache-httpd.service

%preun
%systemd_preun apache-httpd.service

%postun
%systemd_postun_with_restart apache-httpd.service

%changelog
* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.3
- include systemd unit file

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.2
- add libnatpmp as (build)dependency

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.1
- initial package