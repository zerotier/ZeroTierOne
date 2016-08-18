FROM node:4.4
EXPOSE 8080/tcp 9993/udp

# Install ZT network conf files
RUN mkdir -p /var/lib/zerotier-one/networks.d
ADD *.conf /var/lib/zerotier-one/networks.d/
ADD zerotier /
ADD zerotier-cli /
ADD .zerotierCliSettings /root/

# Install App
ADD server.js /

# script which will start/auth VM on ZT network
ADD entrypoint.sh /
RUN chmod -v +x /entrypoint.sh 

CMD ["./entrypoint.sh"]