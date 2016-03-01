# add --with controller option to build controller (builds zerotier-controller package)
%bcond_with controller
# add --with miniupnpc option to enable the miniupnpc option during build 
%bcond_with miniupnpc
# add --with cluster option to enable the cluster option during build 
%bcond_with cluster
# add --with debug option to enable the debug option during build 
%bcond_with debug
%if %{with controller}
Name:zerotier-controller
Conflicts:zerotier
%else
Name:zerotier
Conflicts:zerotier-controller
%endif
Version: 1.1.4
Release: 1
Summary:  Network Virtualization Everywhere https://www.zerotier.com/ 
Group: network
License: GPLv3
BuildRoot: %{_tmppath}/%{name}-root
Provides: zerotier-one
Source0:  http:///download/%{name}-%{version}.tar.gz
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: gcc
%if %{with server}
BuildRequires: sqlite-devel
BuildRequires: wget
BuildRequires: unzip
Requires: sqlite
%endif
%description
ZeroTier One creates virtual Ethernet networks that work anywhere and everywhere.
Visit https://www.zerotier.com/ for more information.

%prep
cd `mktemp -d`
wget -O master.zip https://github.com/zerotier/ZeroTierOne/archive/master.zip
unzip master.zip
mv ZeroTierOne-master zerotier-1.1.4
ln -s zerotier-1.1.4 zerotier-controller-1.1.4
tar zcvf zerotier-1.1.4.tar.gz zerotier-1.1.4 zerotier-controller-1.1.4
ln -s zerotier-1.1.4.tar.gz zerotier-controller-1.1.4.tar.gz
mv zero*.tar.gz ~/rpmbuild/SOURCES
cd -
%setup -q

%build
%if %{with miniupnpc}
ZT_USE_MINIUPNPC=1; export ZT_USE_MINIUPNPC;
%endif

%if %{with controller}
ZT_ENABLE_NETWORK_CONTROLLER=1; export ZT_ENABLE_NETWORK_CONTROLLER;
%endif

%if %{with cluster}
export ZT_ENABLE_CLUSTER=1
%endif

%if %{with debug}
export ZT_DEBUG=1
%endif

make

%install


rm -rf $RPM_BUILD_ROOT
rm -f $RPM_BUILD_ROOT%{_prefix}/bin/zerotier-idtool $RPM_BUILD_ROOT%{_prefix}/bin/zerotier-idtool
echo 'Install...'
mkdir -p $RPM_BUILD_ROOT%{_vardir}/lib/zerotier-one/initfiles/{init.d,systemd}
install -m 0755 -D zerotier-one -t $RPM_BUILD_ROOT%{_vardir}/lib/zerotier-one/
install -m 0755 -D ext/installfiles/linux/init.d/* -t $RPM_BUILD_ROOT%{_vardir}/lib/zerotier-one/initfiles/init.d/
install -m 0755 -D ext/installfiles/linux/systemd/* -t $RPM_BUILD_ROOT%{_vardir}/lib/zerotier-one/initfiles/systemd/



%posttrans
echo -n 'Getting version of new install... '
newVersion=`/var/lib/zerotier-one/zerotier-one -v`
echo $newVersion

echo 'Creating symlinks...'

rm -f /usr/bin/zerotier-cli /usr/bin/zerotier-idtool
ln -sf /var/lib/zerotier-one/zerotier-one /usr/bin/zerotier-cli
ln -sf /var/lib/zerotier-one/zerotier-one /usr/bin/zerotier-idtool
echo 'Installing zerotier-one service...'

SYSTEMDUNITDIR=
if [ -e /bin/systemctl -o -e /usr/bin/systemctl -o -e /usr/local/bin/systemctl -o -e /sbin/systemctl -o -e /usr/sbin/systemctl ]; then
        # Second check: test if systemd appears to actually be running. Apparently Ubuntu
        # thought it was a good idea to ship with systemd installed but not used. Issue #133
        if [ -d /var/run/systemd/system -o -d /run/systemd/system ]; then
                if [ -e /usr/bin/pkg-config ]; then
                        SYSTEMDUNITDIR=`/usr/bin/pkg-config systemd --variable=systemdsystemunitdir`
                fi
                if [ -z "$SYSTEMDUNITDIR" -o ! -d "$SYSTEMDUNITDIR" ]; then
                        if [ -d /usr/lib/systemd/system ]; then
                                SYSTEMDUNITDIR=/usr/lib/systemd/system
                        fi
                        if [ -d /etc/systemd/system ]; then
                                SYSTEMDUNITDIR=/etc/systemd/system
                        fi
                fi
        fi
fi

if [ -n "$SYSTEMDUNITDIR" -a -d "$SYSTEMDUNITDIR" ]; then
        # SYSTEMD

        # If this was updated or upgraded from an init.d based system, clean up the old
        # init.d stuff before installing directly via systemd.
        if [ -f /etc/init.d/zerotier-one ]; then
                if [ -e /sbin/chkconfig -o -e /usr/sbin/chkconfig -o -e /bin/chkconfig -o -e /usr/bin/chkconfig ]; then
                        chkconfig zerotier-one off
                fi
                rm -f /etc/init.d/zerotier-one
        fi

        cp -f /var/lib/zerotier-one/initfiles/systemd/zerotier-one.service "$SYSTEMDUNITDIR/zerotier-one.service"
        chown 0 "$SYSTEMDUNITDIR/zerotier-one.service"
        chgrp 0 "$SYSTEMDUNITDIR/zerotier-one.service"
        chmod 0755 "$SYSTEMDUNITDIR/zerotier-one.service"

        systemctl enable zerotier-one.service

        echo
        echo 'Done! Installed and service configured to start at system boot.'
        echo
       echo "To start now or restart the service if it's already running:"
        echo '  sudo systemctl restart zerotier-one.service'
else
        # SYSV INIT -- also covers upstart which supports SysVinit backward compatibility

        cp -f /var/lib/zerotier-one/initfiles/init.d/zerotier-one /etc/init.d/zerotier-one
        chmod 0755 /etc/init.d/zerotier-one
        
        if [ -f /sbin/chkconfig -o -f /usr/sbin/chkconfig -o -f /usr/bin/chkconfig -o -f /bin/chkconfig ]; then
                chkconfig zerotier-one on
        else
                # Yes Virginia, some systems lack chkconfig.
                if [ -d /etc/rc0.d ]; then
                        rm -f /etc/rc0.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc0.d/K89zerotier-one
                fi
                if [ -d /etc/rc1.d ]; then
                        rm -f /etc/rc1.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc1.d/K89zerotier-one
                fi
                if [ -d /etc/rc2.d ]; then
                        rm -f /etc/rc2.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc2.d/S11zerotier-one
                fi
                if [ -d /etc/rc3.d ]; then
                        rm -f /etc/rc3.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc3.d/S11zerotier-one
                fi
                if [ -d /etc/rc4.d ]; then
                        rm -f /etc/rc4.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc4.d/S11zerotier-one
                fi
                if [ -d /etc/rc5.d ]; then
                        rm -f /etc/rc5.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc5.d/S11zerotier-one
                fi
                if [ -d /etc/rc6.d ]; then
                        rm -f /etc/rc6.d/???zerotier-one
                        ln -sf /etc/init.d/zerotier-one /etc/rc6.d/K89zerotier-one
                fi
        fi
        echo
        echo 'Done! Installed and service configured to start at system boot.'
        echo
        echo "To start now or restart the service if it's already running:"
        echo '  sudo service zerotier-one restart'
fi
%preun
/sbin/chkconfig --del zerotier-one
rm -f /usr/bin/zerotier-cli /usr/bin/zerotier-idtool

%clean
rm -rf $RPM_BUILD_ROOT
%files
%{_vardir}/lib/zerotier-one/zerotier-one
%{_vardir}/lib/zerotier-one/initfiles/systemd/zerotier-one.service
%{_vardir}/lib/zerotier-one/initfiles/init.d/zerotier-one

%changelog
* Fri Feb 26 2016 Kristof Imre Szabo <kristof.szabo@lxsystems.de> 1.1.4-1
- initial package
