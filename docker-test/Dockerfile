# A basic harnessed application example using ZeroTier-One and user-space IP stack
FROM fedora:20

MAINTAINER https://www.zerotier.com/

# Add files
ADD zerotier-one /
ADD intercept /
ADD lib/libintercept.so.1.0 /
ADD lib/liblwip.so /

# Install sys-call intercept library
RUN cp libintercept.so.1.0 /lib/libintercept.so.1.0
RUN ln -sf /lib/libintercept.so.1.0 /lib/libintercept
RUN /usr/bin/install -c intercept /usr/bin

# Install Apache
RUN yum -y update && yum clean all
RUN yum -y install httpd && yum clean all
RUN echo "Apache" >> /var/www/html/index.html
EXPOSE 9993/udp

# Simple startup script to avoid some issues observed with container restart 
ADD run-apache.sh /run-apache.sh
RUN chmod -v +x /run-apache.sh
CMD ["/run-apache.sh"]

# Start ZeroTier-One
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN touch /var/lib/zerotier-one/networks.d/e5cd7a9e1c5311ab.conf
RUN ./zerotier-one -d -U -p9993 /root/dev/ztest

