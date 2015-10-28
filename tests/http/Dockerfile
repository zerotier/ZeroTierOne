FROM centos:latest

MAINTAINER https://www.zerotier.com/

EXPOSE 9993/udp

RUN yum -y update && yum -y install epel-release && yum -y install nodejs npm && yum clean all

RUN mkdir -p /var/lib/zerotier-one
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN touch /var/lib/zerotier-one/networks.d/ffffffffffffffff.conf

ADD package.json /
RUN npm install

ADD zerotier-one /
RUN chmod a+x /zerotier-one

ADD agent.js /
ADD main.sh /
RUN chmod a+x /docker-main.sh

CMD ["./docker-main.sh"]
