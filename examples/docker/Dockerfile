FROM centos:7

MAINTAINER https://www.zerotier.com/

RUN yum -y update && yum install -y sqlite net-tools && yum clean all

EXPOSE 9993/udp

RUN mkdir -p /var/lib/zerotier-one
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN ln -sf /var/lib/zerotier-one/zerotier-one /usr/local/bin/zerotier-cli
RUN ln -sf /var/lib/zerotier-one/zerotier-one /usr/local/bin/zerotier-idtool

ADD zerotier-one /var/lib/zerotier-one/

ADD main.sh /
RUN chmod a+x /main.sh

CMD ["./main.sh"]
