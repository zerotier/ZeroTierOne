FROM centos:latest

MAINTAINER https://www.zerotier.com/

EXPOSE 9993/udp

ADD nodesource-el.repo /etc/yum.repos.d/nodesource-el.repo
RUN yum -y update && yum install -y nodejs && yum clean all

RUN mkdir -p /var/lib/zerotier-one
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN touch /var/lib/zerotier-one/networks.d/ffffffffffffffff.conf

ADD package.json /
RUN npm install

ADD zerotier-one /
RUN chmod a+x /zerotier-one

ADD agent.js /
ADD docker-main.sh /
RUN chmod a+x /docker-main.sh

CMD ["./docker-main.sh"]
