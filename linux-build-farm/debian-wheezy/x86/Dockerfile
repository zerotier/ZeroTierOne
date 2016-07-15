#FROM tubia/debian:wheezy
#MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

#RUN apt-get update
#RUN apt-get install -y build-essential debhelper ruby-ronn g++ make devscripts

FROM zerotier/zt1-build-debian-wheezy-x86-base
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

RUN dpkg --purge libhttp-parser-dev

ADD zt1-src.tar.gz /

RUN mv -f /ZeroTierOne/debian/control.wheezy /ZeroTierOne/debian/control
RUN mv -f /ZeroTierOne/debian/rules.wheezy /ZeroTierOne/debian/rules
