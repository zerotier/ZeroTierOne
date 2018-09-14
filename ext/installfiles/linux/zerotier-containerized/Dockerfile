FROM alpine:latest as builder
MAINTAINER Adam Ierymenko <adam.ierymenko@zerotier.com>

ARG VERSION=1.2.12
LABEL version=$VERSION
LABEL description="Containerized ZeroTier One"

RUN apk add --update alpine-sdk linux-headers
RUN git clone https://github.com/zerotier/ZeroTierOne.git
WORKDIR ZeroTierOne
RUN git checkout $VERSION
RUN make

FROM alpine:latest as final

RUN apk add --update libgcc libstdc++

COPY --from=builder /ZeroTierOne/zerotier-one /
RUN chmod 0755 /zerotier-one
RUN ln -sf /zerotier-one /zerotier-cli
RUN mkdir -p /var/lib/zerotier-one

ADD main.sh /
RUN chmod 0755 /main.sh

ENTRYPOINT /main.sh
