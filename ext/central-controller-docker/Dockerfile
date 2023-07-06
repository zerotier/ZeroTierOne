# Dockerfile for ZeroTier Central Controllers
FROM registry.zerotier.com/zerotier/ctlbuild:latest as builder
MAINTAINER Adam Ierymekno <adam.ierymenko@zerotier.com>, Grant Limberg <grant.limberg@zerotier.com>
ADD . /ZeroTierOne
RUN export PATH=$PATH:~/.cargo/bin && cd ZeroTierOne && make clean && make central-controller -j8

FROM registry.zerotier.com/zerotier/ctlrun:latest
COPY --from=builder /ZeroTierOne/zerotier-one /usr/local/bin/zerotier-one
RUN chmod a+x /usr/local/bin/zerotier-one
RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/usr-local-lib64.conf && ldconfig

ADD ext/central-controller-docker/main.sh /
RUN chmod a+x /main.sh

ENTRYPOINT /main.sh
