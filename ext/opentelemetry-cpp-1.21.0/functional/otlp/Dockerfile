# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

FROM otel/opentelemetry-collector:0.123.0@sha256:c8e36258c1b26927fb7b05c5186b90e9c3d77315efc24f65d6fddec1c14b60b3
COPY . .
CMD ["--config", "/otel-cpp/otel-config.yaml"]
EXPOSE 4317
EXPOSE 4318
