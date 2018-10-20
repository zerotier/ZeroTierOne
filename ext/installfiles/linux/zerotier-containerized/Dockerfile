FROM debian:stretch as builder

## Supports x86_64, x86, arm, and arm64

RUN apt-get update && apt-get install -y curl gnupg
RUN apt-key adv --keyserver ha.pool.sks-keyservers.net --recv-keys 0x1657198823e52a61  && \
    echo "deb http://download.zerotier.com/debian/stretch stretch main" > /etc/apt/sources.list.d/zerotier.list
RUN apt-get update && apt-get install -y zerotier-one=1.2.12

FROM alpine:latest
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

LABEL version="1.2.12"
LABEL description="Containerized ZeroTier One for use on CoreOS or other Docker-only Linux hosts."

# Uncomment to build in container
#RUN apk add --update alpine-sdk linux-headers

RUN apk add --update libgcc libstdc++

RUN mkdir -p /var/lib/zerotier-one

COPY --from=builder /var/lib/zerotier-one/zerotier-cli /usr/sbin/zerotier-cli
COPY --from=builder /var/lib/zerotier-one/zerotier-idtool /usr/sbin/zerotier-idtool
COPY --from=builder /usr/sbin/zerotier-one /usr/sbin/zerotier-one

ADD main.sh /
RUN chmod 0755 /main.sh
ENTRYPOINT ["/main.sh"]
CMD ["zerotier-one"]