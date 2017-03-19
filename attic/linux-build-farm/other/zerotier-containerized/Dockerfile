FROM alpine:latest
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

LABEL version="1.1.14"
LABEL description="Containerized ZeroTier One for use on CoreOS or other Docker-only Linux hosts."

# Uncomment to build in container
#RUN apk add --update alpine-sdk linux-headers

RUN apk add --update libgcc libstdc++

ADD zerotier-one /
RUN chmod 0755 /zerotier-one
RUN ln -sf /zerotier-one /zerotier-cli
RUN mkdir -p /var/lib/zerotier-one

ADD main.sh /
RUN chmod 0755 /main.sh

ENTRYPOINT /main.sh
