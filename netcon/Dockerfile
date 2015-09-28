# A basic harnessed application example using ZeroTier-One and user-space IP stack
FROM fedora:20

MAINTAINER https://www.zerotier.com/

# Add files
ADD ZeroTierOneInstaller-linux-x64-1_0_5 /
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

EXPOSE 80

# Simple startup script to avoid some issues observed with container restart 
ADD run-apache.sh /run-apache.sh
RUN chmod -v +x /run-apache.sh

CMD ["/run-apache.sh"]





# Install ZeroTier-One
#chmod 755 ZeroTierOneInstaller-linux-x64-1_0_5
#sudo ./ZeroTierOneInstaller-linux-x64-1_0_5

# Start ZeroTier-One
CMD ["./zerotier-one", "-U -p9990 /root/dev/ztest", "FOREGROUND"]

# Start Applications
#CMD ["intercept", "/sbin/sshd", "FOREGROUND"]
RUN intercept httpd
