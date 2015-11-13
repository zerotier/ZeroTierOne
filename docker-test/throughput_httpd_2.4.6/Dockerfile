# ZT Netcon Throughput test
FROM centos:7
MAINTAINER https://www.zerotier.com/

# Install apps
RUN yum -y update
RUN yum -y install httpd-2.4.17-3.fc23
RUN yum -y install nano
RUN yum -y install strace
RUN yum clean all

EXPOSE 9993/udp 9992/udp 9991/udp 9990/udp

# Install sys-call intercept library
ADD intercept /
ADD libintercept.so.1.0 /
RUN cp libintercept.so.1.0 lib/libintercept.so.1.0
RUN cp libintercept.so.1.0 /lib/libintercept.so.1.0
RUN ln -sf /lib/libintercept.so.1.0 /lib/libintercept
RUN /usr/bin/install -c intercept /usr/bin

# Add ZT files
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN touch /var/lib/zerotier-one/networks.d/e5cd7a9e1c5311ab.conf
ADD zerotier-one /
ADD zerotier-cli /

# Install test-setup scripts
ADD generate_file.sh /generate_file.sh
RUN chmod -v +x /generate_file.sh
ADD entrypoint.sh /entrypoint.sh
RUN chmod -v +x /entrypoint.sh

# Install LWIP library used by service
ADD liblwip.so /
RUN mkdir -p ext/bin/lwip
RUN cp liblwip.so ext/bin/lwip/liblwip.so

# Start ZeroTier-One
CMD ["./entrypoint.sh"]
