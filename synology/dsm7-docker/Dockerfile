# vim: ft=dockerfile

FROM alpine:latest as builder

ARG COMMIT=04b77773c4cd384c359e3a48f40307b288233194

RUN apk add --no-cache rust cargo
RUN apk add  openssl-dev

RUN apk add --update alpine-sdk linux-headers \
  && git clone --quiet https://github.com/zerotier/ZeroTierOne.git /src \
  && git -C src reset --quiet --hard ${COMMIT} \
  && cd /src \
  && make -f make-linux.mk

FROM alpine:latest
LABEL version="1.8.7"
LABEL description="ZeroTier One docker image for Synology NAS"

RUN apk add --update --no-cache bash jq libc6-compat libstdc++

EXPOSE 9993/udp

COPY --from=builder /src/zerotier-one /usr/sbin/
RUN mkdir -p /var/lib/zerotier-one \
  && ln -s /usr/sbin/zerotier-one /usr/sbin/zerotier-idtool \
  && ln -s /usr/sbin/zerotier-one /usr/sbin/zerotier-cli

COPY entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
