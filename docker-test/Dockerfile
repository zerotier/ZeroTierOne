# A basic harnessed application example using ZeroTier-One and user-space IP stack
FROM fedora:20

MAINTAINER https://www.zerotier.com/

# Install Apache
RUN yum -y update && yum clean all
RUN yum -y install httpd && yum clean all

# Add files
ADD zerotier-one /
ADD zerotier-cli /
ADD intercept /
ADD lib/libintercept.so.1.0 /
ADD lib/liblwip.so /
ADD htdocs/index.html /
ADD htdocs/ZeroTierIcon.png /

RUN cp libintercept.so.1.0 lib/libintercept.so.1.0
RUN cp liblwip.so lib/liblwip.so

# Install sys-call intercept library
RUN cp libintercept.so.1.0 /lib/libintercept.so.1.0
RUN ln -sf /lib/libintercept.so.1.0 /lib/libintercept
RUN /usr/bin/install -c intercept /usr/bin

RUN echo "Apache" >> /var/www/html/index.html
EXPOSE 9993/udp

RUN mv index.html /var/www/html/index.html
RUN mv ZeroTierIcon.png /var/www/html/ZeroTierIcon.png

# Start ZeroTier-One
RUN mkdir -p /var/lib/zerotier-one/networks.d
RUN touch /var/lib/zerotier-one/networks.d/8056c2e21c000001.conf

ADD entrypoint.sh /entrypoint.sh
RUN chmod -v +x /entrypoint.sh
CMD ["./entrypoint.sh"]
