# Dockerfile for ZeroTier Central Controllers
FROM registry.zerotier.com/zerotier/ctlbuild:2025-07-14 AS builder
ADD . /ZeroTierOne
RUN export PATH=$PATH:~/.cargo/bin && cd ZeroTierOne && make clean && make central-controller -j8

FROM golang:bookworm AS go_base
RUN go install -tags 'postgres' github.com/golang-migrate/migrate/v4/cmd/migrate@latest

FROM registry.zerotier.com/zerotier/ctlrun:2025-07-14 AS run_base
COPY --from=builder /ZeroTierOne/zerotier-one /usr/local/bin/zerotier-one
COPY --from=builder /ZeroTierOne/ext/opentelemetry-cpp-1.21.0/localinstall/lib/*.so /usr/local/lib64/
COPY --from=go_base /go/bin/migrate /usr/local/bin/migrate
COPY ext/central-controller-docker/migrations /migrations

RUN chmod a+x /usr/local/bin/zerotier-one
RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/usr-local-lib64.conf && ldconfig

ADD ext/central-controller-docker/main.sh /
RUN chmod a+x /main.sh

ENTRYPOINT /main.sh
