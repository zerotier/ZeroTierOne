Name:           zerotier-one
Version:        1.8.2
Release:        1%{?dist}
Summary:        ZeroTier network virtualization service

License:        ZeroTier BSL 1.1
URL:            https://www.zerotier.com

%if 0%{?rhel} >= 7
BuildRequires:  systemd
%endif

%if 0%{?fedora} >= 21
BuildRequires:  systemd
%endif

Requires:       iproute libstdc++

%if 0%{?rhel} >= 7
Requires:       systemd
Requires(pre): /usr/sbin/useradd, /usr/bin/getent
%endif

%if 0%{?rhel} <= 6
Requires:       chkconfig
%endif

%if 0%{?fedora} >= 21
Requires:       systemd
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
%if 0%{?rhel} >= 7
rm -rf *
ln -s %{getenv:PWD} %{name}-%{version}
tar --exclude=%{name}-%{version}/.git --exclude=%{name}-%{version}/%{name}-%{version} -czf %{_sourcedir}/%{name}-%{version}.tar.gz %{name}-%{version}/*
rm -f %{name}-%{version}
cp -a %{getenv:PWD}/* .
%endif

%build
#%if 0%{?rhel} <= 7
#make CFLAGS="`echo %{optflags} | sed s/stack-protector-strong/stack-protector/`" CXXFLAGS="`echo %{optflags} | sed s/stack-protector-strong/stack-protector/`" ZT_USE_MINIUPNPC=1 %{?_smp_mflags} one manpages selftest
#%else
%if 0%{?rhel} >= 7
make ZT_USE_MINIUPNPC=1 %{?_smp_mflags} one
%endif

%pre
%if 0%{?rhel} >= 7
/usr/bin/getent passwd zerotier-one || /usr/sbin/useradd -r -d /var/lib/zerotier-one -s /sbin/nologin zerotier-one
%endif
%if 0%{?fedora} >= 21
/usr/bin/getent passwd zerotier-one || /usr/sbin/useradd -r -d /var/lib/zerotier-one -s /sbin/nologin zerotier-one
%endif

%install
rm -rf $RPM_BUILD_ROOT
%if 0%{?rhel} < 7
pushd %{getenv:PWD}
%endif
make install DESTDIR=$RPM_BUILD_ROOT
%if 0%{?rhel} < 7
popd
%endif
%if 0%{?rhel} >= 7
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp %{getenv:PWD}/debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service
%endif
%if 0%{?fedora} >= 21
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
cp ${getenv:PWD}/debian/zerotier-one.service $RPM_BUILD_ROOT%{_unitdir}/%{name}.service
%endif
%if 0%{?rhel} <= 6
mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp %{getenv:PWD}/ext/installfiles/linux/zerotier-one.init.rhel6 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
chmod 0755 $RPM_BUILD_ROOT/etc/init.d/zerotier-one
%endif

%files
%{_sbindir}/*
%{_mandir}/*
%{_localstatedir}/*
%if 0%{?rhel} >= 7
%{_unitdir}/%{name}.service
%endif
%if 0%{?fedora} >= 21
%{_unitdir}/%{name}.service
%endif
%if 0%{?rhel} <= 6
/etc/init.d/zerotier-one
%endif

%post
%if 0%{?rhel} >= 7
%systemd_post zerotier-one.service
%endif
%if 0%{?fedora} >= 21
%systemd_post zerotier-one.service
%endif
%if 0%{?rhel} <= 6
case "$1" in
  1)
    chkconfig --add zerotier-one
  ;;
  2)
    chkconfig --del zerotier-one
    chkconfig --add zerotier-one
  ;;
esac
%endif

%preun
%if 0%{?rhel} >= 7
%systemd_preun zerotier-one.service
%endif
%if 0%{?fedora} >= 21
%systemd_preun zerotier-one.service
%endif
%if 0%{?rhel} <= 6
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
%if 0%{?rhel} >= 7
%systemd_postun_with_restart zerotier-one.service
%endif
%if 0%{?fedora} >= 21
%systemd_postun_with_restart zerotier-one.service
%endif

%changelog
* Mon Nov 08 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.2
- see https://github.com/zerotier/ZeroTierOne for release notes

* Wed Oct 20 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.1
- see https://github.com/zerotier/ZeroTierOne for release notes

* Tue Sep 15 2021 Adam Ierymenko <adam.ierymenko@zerotier.com> - 1.8.0
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
