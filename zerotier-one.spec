Name:           zerotier-one
Version:        1.14.2
Release:        1%{?dist}
Summary:        ZeroTier network virtualization service

License:        ZeroTier BUSL 1.1
URL:            https://www.zerotier.com

# Fedora

%if "%{?dist}" == ".fc35"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".fc36"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".fc37"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".fc38"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".fc39"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".fc40"
BuildRequires: systemd clang openssl openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

# RHEL

%if "%{?dist}" == ".el6"
Requires: chkconfig
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".el7"
BuildRequires: systemd openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".el8"
BuildRequires: systemd openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".el9"
BuildRequires: systemd openssl-devel
Requires:      systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

# Amazon

%if "%{?dist}" == ".amzn2"
BuildRequires:  systemd openssl-devel
Requires:       systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if "%{?dist}" == ".amzn2022"
BuildRequires:  systemd openssl-devel
Requires:       systemd openssl
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%description
ZeroTier is a software defined networking layer for Earth.

It can be used for on-premise network virtualization, as a peer to peer VPN
for mobile teams, for hybrid or multi-data-center cloud deployments, or just
about anywhere else secure software defined virtual networking is useful.

This is our OS-level client service. It allows Mac, Linux, Windows,
FreeBSD, and soon other types of clients to join ZeroTier virtual networks
like conventional VPNs or VLANs. It can run on native systems, VMs, or
containers (Docker, OpenVZ, etc.).

%prep
%if "%{?dist}" != ".el6"
rm -rf BUILD BUILDROOT RPMS SRPMS SOURCES
ln -s %{getenv:PWD} %{name}-%{version}
mkdir -p SOURCES
tar --exclude=%{name}-%{version}/.git --exclude=%{name}-%{version}/%{name}-%{version} -czf SOURCES/%{name}-%{version}.tar.gz %{name}-%{version}/*
rm -f %{name}-%{version}
# cp -a %{getenv:PWD}/* .
%endif

%build
%if "%{?dist}" != ".el6"
make ZT_USE_MINIUPNPC=1 %{?_smp_mflags} one
%endif

%pre
/usr/bin/getent passwd zerotier-one || /usr/sbin/useradd -r -d /var/lib/zerotier-one -s /sbin/nologin zerotier-one

%install
%if "%{?dist}" != ".el6"
make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp %{getenv:PWD}/debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service
%else
rm -rf $RPM_BUILD_ROOT
pushd %{getenv:PWD}
make install DESTDIR=$RPM_BUILD_ROOT
popd
mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp %{getenv:PWD}/ext/installfiles/linux/zerotier-one.init.rhel6 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
chmod 0755 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
%endif

%files
%{_sbindir}/*
%{_mandir}/*
%{_localstatedir}/*

%if 0%{?rhel} && 0%{?rhel} <= 6
/etc/init.d/zerotier-one
%else
%{_unitdir}/%{name}.service
%endif

%post
%if ! 0%{?rhel} && 0%{?rhel} <= 6
%systemd_post zerotier-one.service
%endif

%preun
%if ! 0%{?rhel} && 0%{?rhel} <= 6
%systemd_preun zerotier-one.service
%endif

%postun
%if ! 0%{?rhel} && 0%{?rhel} <= 6
%systemd_postun_with_restart zerotier-one.service
%endif

%changelog
* Wed Oct 23 2024 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.14.2
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Mar 19 2024 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.14.0
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Sep 12 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.12.2
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Aug 25 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.12.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Thu Aug 17 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.12.0
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Mar 21 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.6
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Mar 10 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.5
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Mar 06 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.4
- see https://github.com/zerotier/ZeroTierOne for release notes

* Sat Jan 21 2023 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.3
- see https://github.com/zerotier/ZeroTierOne for release notes

* Thu Oct 13 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.2
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Jun 27 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Jun 03 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.10.0
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue May 10 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.10
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Apr 25 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.9
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Apr 11 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.8
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Mar 21 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.7
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Mar 07 2022 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.6
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Dec 17 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.5
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Nov 23 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.4
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Nov 15 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.3
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Nov 08 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.2
- see https://github.com/zerotier/ZeroTierOne for release notes

* Wed Oct 20 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Wed Sep 15 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.0
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Apr 13 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.5
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Feb 15 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.4
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Nov 30 2020 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.2-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Nov 24 2020 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.1-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Thu Nov 19 2020 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.0-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Oct 05 2020 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.6.0-beta1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Aug 23 2019 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.4.4-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Jul 29 2019 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.4.0-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue May 08 2018 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.2.10-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Thu May 03 2018 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.2.8-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Mon Apr 24 2017 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.2.2-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Mar 17 2017 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.2.2-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Mar 14 2017 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.2.0-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Jul 12 2016 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.1.10-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Fri Jul 08 2016 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.1.8-0.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Sat Jun 25 2016 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.1.6-0.1
- now builds on CentOS 6 as well as newer distros, and some cleanup

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.3
- include systemd unit file

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.2
- add libnatpmp as (build)dependency

* Wed Jun 08 2016 François Kooman <fkooman@tuxed.net> - 1.1.5-0.1
- initial package
